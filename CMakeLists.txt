cmake_minimum_required(VERSION 3.15)
project(optimization_testing_ground)
add_subdirectory(benchmark)

set(CMAKE_CXX_STANDARD 20)

option(PGO_INSTRUMENTATION_GENERATE "Enable this if you want the benchmark to be built with IR level instrumentation" OFF)
option(PGO_INSTRUMENTATION_USE "Enable this if you want the benchmark to be built using optimizations enabled by IR level instrumentation" OFF)

SET(CMAKE_CXX_FLAGS "-O2")
SET(CMAKE_C_FLAGS "-O2")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.50 REQUIRED COMPONENTS filesystem)
message(STATUS "Boost version: ${Boost_VERSION}")

if (PGO_INSTRUMENTATION_GENERATE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -Xclang -fprofile-instrument=llvm")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -Xclang -fprofile-instrument=llvm")
ENDIF(PGO_INSTRUMENTATION_GENERATE)

if (PGO_INSTRUMENTATION_USE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-use=${PGO_INSTRUMENTATION_USE}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-use=${PGO_INSTRUMENTATION_USE}")
ENDIF(PGO_INSTRUMENTATION_USE)

add_executable(optimization_testing_ground main.cpp clusteredness/clusteredness.cpp clusteredness/clusteredness.h clusteredness/prefetching.cpp clusteredness/prefetching.h)
target_link_libraries(optimization_testing_ground benchmark::benchmark)
target_link_libraries(optimization_testing_ground boost)
