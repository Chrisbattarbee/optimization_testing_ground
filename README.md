# Description

A suite of benchmarks showcasing different results and laying the foundation for different proof of concept optimizations.

# Dependencies

Google Benchmarking library: https://github.com/google/benchmark

# Usage
```bash
git submodule update --init --recursive
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j 6
./optimization_testing_ground
```
