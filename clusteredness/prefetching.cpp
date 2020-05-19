#include <benchmark/benchmark.h>
#include <random>
#include "prefetching.h"
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;


// Tunable parameters
#define PREFETCH_OFFSET 64 // Assuming 64 for now, taken from https://www.cl.cam.ac.uk/~sa614/papers/Software-Prefetching-CGO2017.pdf
#define CACHE_SIZE 32 * 1024 // Assume the L1 data cache is 32KB, this is the case on my machine and the lab machine I was testing on
#define MAX_NUM_ELEMENTS_IN_ARRAY 100000

// Test Control
#define TESTING_EFFECTS_OF_CACHE_FLUSHING false
#define REPETITIONS_OF_EXPERIMENTS 100

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
 *
 * This is no longer used, as we want to have the additional layer of indirection but just not jumbled up to have a more
 * reasonable comparison with the indirect memory access comparison
 */
template <bool is_cache_flushed, bool is_software_prefetching_used>
static void BM_HardwarePrefetching(benchmark::State& state) {
    // Setup
    srand(time(NULL));
    auto num_elements = state.range(0);
    auto* array = (uint512_t*) malloc(sizeof(uint512_t) * num_elements);
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
            if constexpr (is_software_prefetching_used) {
                // Taken from https://www.cl.cam.ac.uk/~sa614/papers/Software-Prefetching-CGO2017.pdf
                __builtin_prefetch(&array[x + 2 * PREFETCH_OFFSET]);
            }
            benchmark::DoNotOptimize(array[x]++);
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
 *    Why are you using a 512 bit integer?
 *      So that each element in our array spans one cache line, this means that the performance between LackOfHardware
 *      and Hardware are more comparable as each access is one cache line, with another structure Hardware gets to access
 *      cacheline / sizeof(element_structure) sequentially.
 */

template <bool shuffled_memory_access, bool is_cache_flushed, bool is_software_prefetching_used>
static void BM_Prefetching(benchmark::State& state) {
    // Setup
    srand(time(NULL));
    auto num_elements = state.range(0);
            auto* array = (uint512_t*) malloc(sizeof(uint512_t) * num_elements);
    for (uint32_t x = 0; x < num_elements; x ++) {
        array[x] = rand();
    }

    // Create an index array then randomly shuffle it
    u_int32_t INDEX_ARRAY_SIZE = num_elements;
    auto* index_array = (uint32_t*) malloc(sizeof(u_int32_t) * INDEX_ARRAY_SIZE);
    for (uint32_t x = 0; x < INDEX_ARRAY_SIZE; x ++) {
        index_array[x] = x;
    }
    if constexpr (shuffled_memory_access) {
        std::shuffle(&index_array[0], &index_array[INDEX_ARRAY_SIZE], std::mt19937(std::random_device()()));
    }

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
            if constexpr (is_software_prefetching_used) {
                // Taken from https://www.cl.cam.ac.uk/~sa614/papers/Software-Prefetching-CGO2017.pdf
                __builtin_prefetch(&array[index_array[x + PREFETCH_OFFSET ]]);
                __builtin_prefetch(&index_array[x + 2 * PREFETCH_OFFSET]);
            }
            benchmark::DoNotOptimize(array[index_array[x]]++);
        }
    }

    // Teardown
    free(array);
}

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
    std::cerr << "Prefetch distance is: " << PREFETCH_OFFSET  << std::endl;
    // Iterate over all of the possible combinations of the template
    if constexpr (TESTING_EFFECTS_OF_CACHE_FLUSHING) {
        // Add tests where the cache is not flushed
        BENCHMARK_TEMPLATE(BM_Prefetching, false, false, false)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
        BENCHMARK_TEMPLATE(BM_Prefetching, true, false, false)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
        BENCHMARK_TEMPLATE(BM_Prefetching, true, false, true)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
        BENCHMARK_TEMPLATE(BM_Prefetching, false, false, true)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
    }
    // Add all tests where the cache is flushed
    BENCHMARK_TEMPLATE(BM_Prefetching, false, true, false)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
    BENCHMARK_TEMPLATE(BM_Prefetching, false, true, true)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
    BENCHMARK_TEMPLATE(BM_Prefetching, true, true, false)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
    BENCHMARK_TEMPLATE(BM_Prefetching, true, true, true)->Apply(CustomArguments)->Iterations(REPETITIONS_OF_EXPERIMENTS);
}