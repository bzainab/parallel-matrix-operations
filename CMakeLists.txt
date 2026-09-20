cmake_minimum_required(VERSION 3.16)

project(ParallelMatrixOperations LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(matrix_operations
    Main.cpp
    FileRead.cpp
    MatrixOperations.cpp
    RandomMatrixGenerator.cpp
)

target_include_directories(matrix_operations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(verify_matrix_operations
    tools/Verify.cpp
    FileRead.cpp
    MatrixOperations.cpp
)

target_include_directories(verify_matrix_operations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

