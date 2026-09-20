// Verification harness: feeds an example srcMatrix.txt through
// matrixOperationsInit and compares cell-by-cell against the expected
// dstMatrix.txt.

#include "../MatrixOperations.h"
#include "../FileRead.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: Verify <example_dir>\n";
        return 2;
    }
    const std::string dir = argv[1];

    auto src      = fileRead(dir + "/srcMatrix.txt");
    auto expected = fileRead(dir + "/dstMatrix.txt");

    if (src.empty() || expected.empty())
    {
        std::cerr << "failed to read example files in " << dir << '\n';
        return 2;
    }

    const int dim = static_cast<int>(src.size());
    std::vector<std::vector<double>> got(dim, std::vector<double>(dim, 0.0));

    matrixOperationsInit(&src, &got);

    int mismatches = 0;
    double maxDiff = 0.0;
    for (int i = 0; i < dim; ++i)
    {
        for (int j = 0; j < dim; ++j)
        {
            const double d = std::abs(got[i][j] - expected[i][j]);
            if (d > 1e-6)
            {
                if (mismatches < 10)
                {
                    std::cerr << "[" << i << "][" << j << "] got=" << got[i][j]
                              << " expected=" << expected[i][j] << '\n';
                }
                ++mismatches;
                if (d > maxDiff) maxDiff = d;
            }
        }
    }

    std::cout << dir << ": dim=" << dim
              << " mismatches=" << mismatches
              << " maxDiff=" << maxDiff << '\n';
    return mismatches == 0 ? 0 : 1;
}
