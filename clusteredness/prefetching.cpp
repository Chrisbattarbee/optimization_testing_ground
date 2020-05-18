#include <benchmark/benchmark.h>
#include <random>
#include "prefetching.h"

static void* flush_data_cache() {
    const int size = 40*1024*1024; // Allocate 40M. Much larger than L3 cache
    char *c = (char *)malloc(size);
    for (int j = 0; j < size; j++) {
        benchmark::DoNotOptimize(c[j] = 0);
    }
    return c;
}

/*
 * This benchmark aims to show the speedup provided by hardware prefetching on a per element read basis
 * Higher amounts of memory should have an overall higher throughput of the read due to the predictable stride
 * access pattern and the hardware prefetcher kicking in.
 */
template <bool is_cache_flushed>
static void BM_HardwarePrefetching(benchmark::State& state) {
    // Setup
    srand(time(NULL));
    auto num_elements = state.range(0);
    auto* array = (uint32_t*) malloc(sizeof(u_int32_t) * num_elements);
    for (uint32_t x = 0; x < num_elements; x ++) {
        array[x] = rand();
    }

    // Actual benchmark
    for (auto _ : state) {
        if constexpr (is_cache_flushed) {
            state.PauseTiming();
            free(flush_data_cache());
            state.ResumeTiming();
        }
        for (int x = 0; x < num_elements; x ++) {
            benchmark::DoNotOptimize(array[x]);
        }
    }

    // Teardown
    free(array);
}

/*
 * The key things to note about this benchmark are the following:
 *   General Hypothesis:
 *     Create an array of random indexes to index into the data array.
 *     This will mean that the hardware prefetcher will not be able to notice a pattern in the accesses and therefore
 *     will not be able to load the elements into cache ahead of time, leading to stalls in the pipeline.
 *
 *  Design decisions:
 *    Why not generate a random each iteration of the access?
 *      The time taken to pause and unpause the clock is too large as well as giving the loads additional time to resolve
 *      from the main memory system
 *    Won't the index array fill up the cache?
 *      Yes potentially though the idea is that the index array is iterated through sequentially and therefore the
 *      prefetcher should notice this stride and prefetch the index array into cache whilst leaving the rest of the cache
 *      open for use
 */

#define CACHE_SIZE 32 * 1024 // Assume the L1 data cache is 32KB, this is the case on my machine and the lab machine I was testing on
template <bool is_cache_flushed>
static void BM_LackOfHardwarePrefetching(benchmark::State& state) {
    // Setup

    srand(time(NULL));
    auto num_elements = state.range(0);
            auto* array = (uint32_t*) malloc(sizeof(u_int32_t) * num_elements);
    for (uint32_t x = 0; x < num_elements; x ++) {
        array[x] = rand();
    }

    // Create an index array then randomly shuffle it
    u_int32_t INDEX_ARRAY_SIZE = num_elements;
    auto* index_array = (uint32_t*) malloc(sizeof(u_int32_t) * INDEX_ARRAY_SIZE);
    for (uint32_t x = 0; x < INDEX_ARRAY_SIZE; x ++) {
        index_array[x] = x;
    }
    std::shuffle(&index_array[0], &index_array[INDEX_ARRAY_SIZE], std::mt19937(std::random_device()()));

    // Actual benchmark
    for (auto _ : state) {
        if constexpr (is_cache_flushed) {
            state.PauseTiming();
            free(flush_data_cache());
            // Load in the first part of the index array in after we flush cache (as much as we can fit into cache
            // to give the hardware pre-fetched a fighting chance of working without the initial delay
            for (int x = 0; x < std::min((ulong) INDEX_ARRAY_SIZE, CACHE_SIZE / sizeof(uint32_t)); x++) {
                benchmark::DoNotOptimize(index_array[x]);
            }
            state.ResumeTiming();
        }

        for (int x = 0; x < num_elements; x ++) {
            benchmark::DoNotOptimize(array[index_array[x % INDEX_ARRAY_SIZE]]);
        }
    }

    // Teardown
    free(array);
}

#define MAX_NUM_ELEMENTS_IN_ARRAY 100000
// Provides arguments in the range 1..100000, skewing the produced arguments towards 0
static void CustomArguments(benchmark::internal::Benchmark* b) {
    for (int i = 1; i < 10; i+= 1)
        b->Args({i});
    for (int i = 10; i < 100; i+= 10)
        b->Args({i});
    for (int i = 100; i < 1000; i+= 100)
        b->Args({i});
    for (int i = 1000; i < 10000; i+= 1000)
        b->Args({i});
    for (int i = 10000; i < MAX_NUM_ELEMENTS_IN_ARRAY; i+= 10000)
        b->Args({i});
}

void prefetching::register_benchmarks() {
    BENCHMARK_TEMPLATE(BM_HardwarePrefetching, true)->Apply(CustomArguments)->Iterations(100);
    BENCHMARK_TEMPLATE(BM_HardwarePrefetching, false)->Apply(CustomArguments)->Iterations(100);
    BENCHMARK_TEMPLATE(BM_LackOfHardwarePrefetching, true)->Apply(CustomArguments)->Iterations(100);
    BENCHMARK_TEMPLATE(BM_LackOfHardwarePrefetching, false)->Apply(CustomArguments)->Iterations(100);
}