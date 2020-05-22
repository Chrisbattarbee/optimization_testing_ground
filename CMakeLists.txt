cmake_minimum_required(VERSION 3.15)
project(optimization_testing_ground)
add_subdirectory(benchmark)
add_subdirectory(IntelSEAPI/ittnotify)

set(CMAKE_CXX_STANDARD 20)

option(PGO_INSTRUMENTATION_GENERATE "Enable this if you want the benchmark to be built with IR level instrumentation" OFF)
option(PGO_INSTRUMENTATION_USE "Enable this if you want the benchmark to be built using optimizations enabled by IR level instrumentation" OFF)

#SET(CMAKE_CXX_FLAGS "-O2  -Xclang -load -Xclang /home/chris/programming/camb_software_prefetch_revival/cmake-build-debug-clang-7/swprefetcher/libSWPrefetecherPass.so")
#SET(CMAKE_C_FLAGS "-O2 -Xclang -load -Xclang /home/chris/programming/camb_software_prefetch_revival/cmake-build-debug-clang-7/swprefetcher/libSWPrefetecherPass.so")
#SET(CMAKE_CXX_FLAGS "-Xclang -load -Xclang /home/chris/CK-TOOLS/plugin-llvm-sw-prefetch-pass-0.1-llvm-3.9.0-linux-64/lib/SwPrefetchPass.so -O3")
#SET(CMAKE_C_FLAGS "-Xclang -load -Xclang /home/chris/CK-TOOLS/plugin-llvm-sw-prefetch-pass-0.1-llvm-3.9.0-linux-64/lib/SwPrefetchPass.so -O3")

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
target_link_libraries(optimization_testing_ground ittnotify)
