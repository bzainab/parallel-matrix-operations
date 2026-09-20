#pragma once

#include <vector>

// Entry point invoked by main.cpp. The signature is fixed by the assessment
// specification and must not be altered.
void matrixOperationsInit(std::vector<std::vector<double>>* srcMatrix,
                          std::vector<std::vector<double>>* dstMatrix);
