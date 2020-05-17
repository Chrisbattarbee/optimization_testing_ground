#include <benchmark/benchmark.h>
#include "prefetching.h"

static void* flush_data_cache() {
    const int size = 40*1024*1024; // Allocate 40M. Much larger than L3 cache
    char *c = (char *)malloc(size);
    for (int j = 0; j < size; j++) {
        benchmark::DoNotOptimize(c[j] = rand());
    }
    return c;
}

/*
 * This benchmark aims to show the speedup provided by hardware prefetching on a per element read basis
 * Higher amounts of memory should have an overall higher throughput of the read due to the predictable stride
 * access pattern and the hardware prefetcher kicking in.
 */
#define NUM_ITERATIONS 100000000
static void BM_HardwarePrefetching(benchmark::State& state) {
    // Setup
    srand(time(NULL));
    auto num_elements = state.range(0);
    auto* array = (uint32_t*) malloc(sizeof(u_int32_t) * num_elements);
    for (uint32_t x = 0; x < num_elements; x ++) {
        array[x] = rand();
    }

    void* cache_flush_data = flush_data_cache();

    // Actual benchmark
    for (auto _ : state) {
        __uint32_t total = 0;
        for (int x = 0; x < num_elements; x ++) {
            benchmark::DoNotOptimize(total += array[x]);
        }
    }

    // Teardown
    free(array);
    free(cache_flush_data);
}

#define MAX_NUM_ELEMENTS_IN_ARRAY 100000
// Provides arguments in the range 1..100000, skewing the produced arguments towards 0
static void CustomArguments(benchmark::internal::Benchmark* b) {
    for (int i = 0; i < 100; i+= 10)
        b->Args({i});
    for (int i = 100; i < 1000; i+= 100)
        b->Args({i});
    for (int i = 1000; i < 10000; i+= 1000)
        b->Args({i});
    for (int i = 10000; i < MAX_NUM_ELEMENTS_IN_ARRAY; i+= 10000)
        b->Args({i});
}

void prefetching::register_benchmarks() {
    BENCHMARK(BM_HardwarePrefetching)->Apply(CustomArguments);
}