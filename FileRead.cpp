#include <fstream>
#include <iostream>

#include "FileRead.h"

// Reading matrix from file
std::vector<std::vector<double>> fileRead(const std::string FileName)
{
    // Declare matrix
    std::vector<std::vector<double>> matrix;

    // Open file for reading
    std::ifstream file(FileName);

    // Check if file opened successfully
    if (!file.is_open())
    {
        std::cout << "Error opening file: " << FileName << std::endl;
        return matrix; // Return empty matrix if file cannot be opened
    }

    // Read matrix size from size
    int dim;
    file >> dim;

    // Initialise matrix as requested by the input parameter
    matrix.resize(dim);

    // Read matrix - if the parameter requires a smaller matrix than the one provided in the file
    // The matrix read is only a subportion of the whole file
    // Loop through all the rows in the destination matrix
    for (int i = 0; i < dim; i++)
    {
        int j;

        // Resize the matrix row, so that it is ready to accept values form file
        matrix[i].resize(dim);

        // Loop through each element in the row, reading the value from file
        for (j = 0; j < dim; j++)
        {
            double temp;

            //read value from file checking if the file is not at the end
            if (file.eof())
            {
                matrix.resize(0);

                std::cout << "Error: End of file reached before reading the entire matrix." << std::endl;

                break; // Exit the loop if end of file is reached
            }

            if (file >> temp)
            {
                // Convert the integer value to double and assign it to the matrix
                matrix[i][j] = temp;
            }
            else
            {
                matrix.resize(0);

                std::cout << "Error reading value from file." << std::endl;

                break; // Exit the loop if an error occurs while reading
            }
        }
    }

    // File close
    file.close();

    // Return the matrix read from file
    return matrix;
}
