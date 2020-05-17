#include <benchmark/benchmark.h>
#include "prefetching.h"

#define NUM_ITERATIONS 100000000
static void BM_HardwarePrefetching(benchmark::State& state) {
    // Setup

    // Actual benchmark
    for (auto _ : state) {
        volatile __uint32_t total = 0;

    }

    // Teardown
}

// Provides arguments of the cross product of [0..101, 10] x [0..101, 10]
static void CustomArguments(benchmark::internal::Benchmark* b) {
    for (int i = 0; i <= 100; i+= 10)
        for (int j = 0; j <= 100; j+= 10)
            b->Args({i, j});
}

void prefetching::register_benchmarks() {
    BENCHMARK(BM_HardwarePrefetching)->Apply(CustomArguments);
}