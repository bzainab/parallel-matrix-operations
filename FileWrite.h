#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

template <class type> int getMaxDigitCount(const std::vector<std::vector<type>>* srcMatrix);

// Template class allows for many type of values in teh source matrix.
// Any type of value supported by std::ofstream is allowed, including int and double
// In general this is a method to avoid conding the same function multiple times where
// the only change is the type of a parameter
template <class type>
void fileWrite(const std::string filename, const std::vector<std::vector<type>>* srcMatrix)
{
    // Open file to write
    std::ofstream file(filename);

    // Write matrix size
    file << srcMatrix->size() << std::endl;

    int maxDigitCount = getMaxDigitCount(srcMatrix);

    // Write matrix
    // looping through all the rows
    for (int i = 0; i < srcMatrix->size(); i++)
    {
        // Looping through all the elemtns in the row
        for (int j = 0; j < srcMatrix->at(i).size(); j++)
        {
            // Printing the value to the file with a fixed width of two digits, so the matrix shows nicely in the text file
            file << std::fixed << std::setw(maxDigitCount) << std::setprecision(0) << srcMatrix->at(i).at(j) << " ";
        }
        // At the end of each row insert a new line
        file << std::endl;
    }

    // Close the file
    file.close();
}

template <class type>
int getMaxDigitCount(const std::vector<std::vector<type>>* srcMatrix)
{
    // Identify the maximum value in the first row
    type maxValue = *std::max_element(srcMatrix->at(0).begin(), srcMatrix->at(0).end());

    // Loop through all the rows in the matrix and find the maximum value
    for (int i = 1; i < srcMatrix->size(); i++)
    {
        type maxRowValue = *std::max_element(srcMatrix->at(i).begin(), srcMatrix->at(i).end());
        if (maxRowValue > maxValue)
            maxValue = maxRowValue;
    }

    return maxValue != 0 ? (int)log10(std::abs((double)maxValue)) + 1 : 1;
}
