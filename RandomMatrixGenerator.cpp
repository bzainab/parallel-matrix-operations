#include <ctime>

#include "RandomMatrixGenerator.h"

std::vector<std::vector<double>> randomMatrixGenerator(int dimension)
{
    std::vector<std::vector<double>> srcMatrix(dimension);

    // Initialise random number generator
    srand((unsigned int)time(NULL));

    // Generate random data
    for (int i = 0; i < dimension; i++) {
        srcMatrix[i].resize(dimension);
        for (int j = 0; j < dimension; j++) {
            srcMatrix[i][j] = rand() % 10;
        }
    }

    return srcMatrix;
}
