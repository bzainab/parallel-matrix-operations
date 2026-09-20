# Parallel Matrix Operations

C++ project implementing a parallel matrix-processing pipeline with a custom worker pool built from `std::thread`, `std::mutex`, `std::condition_variable`, and a queue of jobs.

## What It Does

Given a square matrix of doubles, the program runs three operations:

1. Transpose the source matrix.
2. Calculate a zone-sum matrix using each cell and its valid neighbours.
3. Multiply the resulting matrix by itself.

The CLI generates a random matrix, runs the pipeline 10 times, and reports each run time plus the average execution time.

## Technical Highlights

- Uses a persistent thread pool so worker threads are created once and reused.
- Splits matrix work by row bands so each worker owns a separate output region.
- Uses condition variables as a completion barrier between pipeline stages.
- Stores intermediate matrices in a flat row-major `std::vector<double>` to improve cache locality.
- Fuses transpose and zone-sum into one pass.
- Uses an `i-k-j` loop order for matrix multiplication to improve memory access patterns.
- Includes sample matrices and a small verification harness for checking expected output.

## Project Structure

```text
MatrixOperations.cpp       # Parallel implementation and thread pool
MatrixOperations.h         # Public pipeline function
Main.cpp                   # Benchmarking CLI entry point
FileRead.cpp/.h            # Matrix file loading helper
FileWrite.h                # Matrix file writing helper
RandomMatrixGenerator.*    # Random square matrix generator
tools/Verify.cpp           # Example-output verification harness
examples/                  # Small input/expected-output matrix sets
MatrixOperations.sln       # Visual Studio solution
MatrixOperations.vcxproj   # Visual Studio C++ project
CMakeLists.txt             # Optional CMake build file
```

## Build With Visual Studio

Open `MatrixOperations.sln` in Visual Studio 2022 or later, then build the `Release|x64` configuration.

## Build With CMake

From the repository root:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the main benchmark:

```bash
./build/matrix_operations
```

On Windows multi-config generators, the executable may be under `build/Release/`.

## Verify Example Output

After building, run the verifier against one of the example folders:

```bash
./build/verify_matrix_operations examples/example-1
```

The verifier compares the pipeline output with the saved `dstMatrix.txt` file and reports mismatches.

## Portfolio Notes

This repository is a cleaned portfolio edition focused on the C++ implementation. Reference executables, object files, IDE metadata, downloads, reports, slides, and other coursework packaging files are intentionally excluded.

