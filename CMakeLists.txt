cmake_minimum_required(VERSION 3.15)
project(optimization_testing_ground)

set(CMAKE_CXX_STANDARD 20)

find_package(benchmark REQUIRED)

add_executable(optimization_testing_ground main.cpp clusteredness/clusteredness.cpp clusteredness/clusteredness.h)
target_link_libraries(optimization_testing_ground benchmark::benchmark)
