// =============================================================================
// MatrixOperations.cpp
//
// Concurrent implementation of the three required operations on a square
// matrix of doubles:
//
//   op1: transpose(src)
//   op2: zone-sum of op1 (sum of each cell with its 3 / 5 / 8 neighbours)
//   op3: op2 * op2 (matrix multiplication of op2 by itself)
//
// Architecture (high level):
//   * A persistent thread pool is built once and reused for every iteration of
//     the 10-run benchmark in main.cpp. This amortises the cost of spawning
//     std::thread objects, which would otherwise dominate small problems and
//     bias the comparison against the reference implementation.
//   * The caller's nested std::vector<std::vector<double>> is copied once into
//     a flat row-major std::vector<double>. Two indirections per access plus
//     non-contiguous rows would defeat the CPU cache during the O(N^3) matmul.
//   * op1 and op2 are fused into a single pass: because zone-sum commutes with
//     transposition (zoneSum(M^T)[i][j] == zoneSum(M)[j][i]), op2 can be
//     written directly from src by reading the 3x3 neighbourhood around the
//     transposed coordinate. This eliminates a full O(N^2) memory shuffle.
//   * The matrix multiplication uses i-k-j loop ordering so that the second
//     operand is read row-wise inside the hot loop. The textbook i-j-k ordering
//     scans B by column and turns nearly every inner iteration into a cache
//     miss; reordering preserves arithmetic identity but transforms the access
//     pattern into a contiguous stream.
//   * Each operation is parallelised across worker threads by row-band
//     partitioning: each thread owns a disjoint set of output rows, which
//     removes any need for mutual exclusion inside the hot loops.
// =============================================================================

#include "MatrixOperations.h"
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace {

// -----------------------------------------------------------------------------
// ThreadPool
//
// A minimal pool built only from primitives covered by the module: std::thread
// for the workers and std::mutex / std::condition_variable for the job queue
// and the completion barrier. No SIMD or library-provided parallel algorithms
// are used.
//
// Lifetime: a single instance is created on first use (see pool() below) and
// kept alive for the rest of the program. main.cpp invokes the pipeline ten
// times back-to-back, so re-using the same workers means we pay the OS thread
// creation cost exactly once instead of thirty times (three operations per
// iteration * ten iterations).
// -----------------------------------------------------------------------------
class ThreadPool
{
public:
    explicit ThreadPool(unsigned workerCount)
        : m_stop(false)
        , m_pending(0)
    {
        m_workers.reserve(workerCount);
        for (unsigned i = 0; i < workerCount; ++i)
        {
            m_workers.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_stop = true;
        }
        m_jobCv.notify_all();
        for (auto& t : m_workers)
        {
            t.join();
        }
    }

    // Run fn(begin, end) over disjoint sub-ranges of [0, total). Blocks the
    // caller until every chunk has finished, so a single parallelFor doubles
    // as the synchronisation barrier between two consecutive operations.
    //
    // The work is split into `chunks` contiguous bands. We capture fn by
    // reference because parallelFor does not return until every worker has
    // finished, so fn is guaranteed to outlive the queued lambdas. Capturing
    // by reference avoids an std::function heap allocation per chunk.
    template <class Fn>
    void parallelFor(int total, int chunks, Fn&& fn)
    {
        // Fast path: nothing to split, run inline. Keeps tiny matrices off the
        // condition-variable / mutex critical path entirely.
        if (chunks <= 1 || total <= 1)
        {
            fn(0, total);
            return;
        }

        chunks = std::min(chunks, total);
        const int chunkSize = (total + chunks - 1) / chunks;

        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_pending = chunks;
            for (int c = 0; c < chunks; ++c)
            {
                const int begin = c * chunkSize;
                const int end   = std::min(begin + chunkSize, total);
                m_jobs.emplace([begin, end, &fn] { fn(begin, end); });
            }
        }
        m_jobCv.notify_all();

        // Block until every chunk has decremented m_pending to zero. Acts as
        // an implicit barrier between operations without a separate object.
        std::unique_lock<std::mutex> lk(m_mtx);
        m_doneCv.wait(lk, [this] { return m_pending == 0; });
    }

    unsigned size() const { return static_cast<unsigned>(m_workers.size()); }

private:
    void workerLoop()
    {
        for (;;)
        {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_mtx);
                m_jobCv.wait(lk, [this] { return m_stop || !m_jobs.empty(); });
                if (m_stop && m_jobs.empty()) return;
                job = std::move(m_jobs.front());
                m_jobs.pop();
            }
            job();
            {
                // Notify the submitter once the last chunk completes. Holding
                // the mutex around the decrement avoids a missed-wakeup race.
                std::lock_guard<std::mutex> lk(m_mtx);
                if (--m_pending == 0)
                {
                    m_doneCv.notify_one();
                }
            }
        }
    }

    std::mutex                          m_mtx;
    std::condition_variable             m_jobCv;
    std::condition_variable             m_doneCv;
    std::queue<std::function<void()>>   m_jobs;
    std::vector<std::thread>            m_workers;
    bool                                m_stop;
    int                                 m_pending;
};

// Lazy singleton. hardware_concurrency() reports logical cores; if the OS
// hides that information we fall back to a single worker so the pool still
// functions. Constructed on first call and torn down at program exit.
ThreadPool& pool()
{
    static const unsigned n = std::max(1u, std::thread::hardware_concurrency());
    static ThreadPool instance(n);
    return instance;
}

// Below this size the per-job synchronisation cost outweighs the work being
// distributed. Keep small matrices on a single thread.
constexpr int kSerialThreshold = 64;

} // namespace

// =============================================================================
// matrixOperationsInit
//
// Three operations executed sequentially on the input. Each operation is
// individually parallelised; consecutive operations are separated by the
// implicit barrier at the end of parallelFor.
// =============================================================================
void matrixOperationsInit(std::vector<std::vector<double>>* srcMatrix,
                          std::vector<std::vector<double>>* dstMatrix)
{
    const int dim = static_cast<int>(srcMatrix->size());
    if (dim == 0) return;

    ThreadPool& tp = pool();
    // One chunk per worker by default. parallelFor caps this at `total` so
    // small matrices never queue empty jobs.
    const int chunks = (dim >= kSerialThreshold) ? static_cast<int>(tp.size()) : 1;

    // -------------------------------------------------------------------------
    // 1. Pack the caller's nested vector into a flat row-major buffer.
    //
    // std::vector<std::vector<double>> stores each row as a separate heap
    // allocation, so consecutive rows are not adjacent in memory and every
    // element access goes through two pointer dereferences. The matmul reads
    // each cell of op2 ~dim times, so a flat layout pays for itself many
    // times over before we even reach the multiplication.
    // -------------------------------------------------------------------------
    std::vector<double> src(static_cast<std::size_t>(dim) * dim);
    tp.parallelFor(dim, chunks, [&](int begin, int end) {
        for (int i = begin; i < end; ++i)
        {
            const std::vector<double>& row = (*srcMatrix)[i];
            std::copy(row.begin(), row.end(), src.begin() + static_cast<std::ptrdiff_t>(i) * dim);
        }
    });

    // -------------------------------------------------------------------------
    // 2. Fused transpose + zone-sum.
    //
    // Original pipeline: op1 = transpose(src); op2 = zoneSum(op1).
    // zoneSum is symmetric in its (di, dj) offsets, so transposing the input
    // and then zone-summing is the same as zone-summing and then transposing:
    //
    //     zoneSum(M^T)[i][j] == zoneSum(M)[j][i]
    //
    // Therefore op2[i][j] equals the 3x3 neighbourhood sum of src around the
    // transposed coordinate (j, i). Writing op2 directly from src removes the
    // intermediate op1 buffer and one full O(N^2) pass over memory.
    // -------------------------------------------------------------------------
    std::vector<double> op2(static_cast<std::size_t>(dim) * dim);
    tp.parallelFor(dim, chunks, [&](int begin, int end) {
        for (int i = begin; i < end; ++i)
        {
            // Column window in src is centred on i; clamp to the matrix edge
            // so corner and side cells naturally see only their 3 / 5 valid
            // neighbours without a per-iteration boundary test.
            const int colStart = (i > 0)       ? i - 1 : 0;
            const int colEnd   = (i < dim - 1) ? i + 1 : dim - 1;

            double* op2Row = &op2[static_cast<std::ptrdiff_t>(i) * dim];

            for (int j = 0; j < dim; ++j)
            {
                const int rowStart = (j > 0)       ? j - 1 : 0;
                const int rowEnd   = (j < dim - 1) ? j + 1 : dim - 1;

                double sum = 0.0;
                for (int r = rowStart; r <= rowEnd; ++r)
                {
                    const double* srcRow = &src[static_cast<std::ptrdiff_t>(r) * dim];
                    for (int c = colStart; c <= colEnd; ++c)
                    {
                        sum += srcRow[c];
                    }
                }
                op2Row[j] = sum;
            }
        }
    });

    // -------------------------------------------------------------------------
    // 3. Matrix multiplication: op3 = op2 * op2.
    //
    // i-k-j loop ordering (vs textbook i-j-k):
    //   for i:                                    // each thread owns a band
    //     for k:                                  // pull a single A scalar
    //       aik = op2[i,k]
    //       for j:                                // stream a B row + C row
    //         op3[i,j] += aik * op2[k,j]
    //
    // Inside the inner loop both op2[k, .] and op3[i, .] advance in the
    // contiguous direction, so the prefetcher can keep the cache lines hot.
    // The textbook ordering would walk op2[., j] down a column, missing on
    // every iteration once the matrix exceeds L1.
    //
    // Parallelism: each thread owns a disjoint row band of op3. There is no
    // overlap between threads' write targets, so accumulation needs no mutex
    // and no atomic operations.
    // -------------------------------------------------------------------------
    std::vector<double> op3(static_cast<std::size_t>(dim) * dim, 0.0);
    tp.parallelFor(dim, chunks, [&](int begin, int end) {
        for (int i = begin; i < end; ++i)
        {
            double* cRow = &op3[static_cast<std::ptrdiff_t>(i) * dim];
            const double* aRow = &op2[static_cast<std::ptrdiff_t>(i) * dim];

            for (int k = 0; k < dim; ++k)
            {
                const double aik = aRow[k];
                const double* bRow = &op2[static_cast<std::ptrdiff_t>(k) * dim];
                for (int j = 0; j < dim; ++j)
                {
                    cRow[j] += aik * bRow[j];
                }
            }
        }
    });

    // -------------------------------------------------------------------------
    // 4. Copy the flat result back into the caller-supplied nested vector.
    // main.cpp pre-sizes dstMatrix to dim x dim, so no resizing is required.
    // -------------------------------------------------------------------------
    tp.parallelFor(dim, chunks, [&](int begin, int end) {
        for (int i = begin; i < end; ++i)
        {
            std::vector<double>& row = (*dstMatrix)[i];
            const double* srcRow = &op3[static_cast<std::ptrdiff_t>(i) * dim];
            std::copy(srcRow, srcRow + dim, row.begin());
        }
    });
}
