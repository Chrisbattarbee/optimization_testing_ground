#include <benchmark/benchmark.h>
#include "clusteredness/clusteredness.h"
#include "clusteredness/prefetching.h"

int main(int argc, char *argv[]) {
    clusteredness::register_benchmarks();
//    prefetching::register_benchmarks();
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
}

