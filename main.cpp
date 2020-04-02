#include <benchmark/benchmark.h>
#include "clusteredness/clusteredness.h"

int main(int argc, char *argv[]) {
    clusteredness::register_benchmarks();
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
}

