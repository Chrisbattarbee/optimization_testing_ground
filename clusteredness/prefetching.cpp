#include <benchmark/benchmark.h>
#include <random>
#include "prefetching.h"
#include <boost/multiprecision/cpp_int.hpp>
#include "ittnotify.h"

using namespace boost::multiprecision;


// Tunable parameters
#define PREFETCH_OFFSET 64 // Assuming 64 for now, taken from https://www.cl.cam.ac.uk/~sa614/papers/Software-Prefetching-CGO2017.pdf
// After testing this seemed about right, lead to large speedups
#define CACHE_SIZE 32 * 1024 // Assume the L1 data cache is 32KB, this is the case on my machine and the lab machine I was testing on
#define MAX_NUM_ELEMENTS_IN_ARRAY 100000001
#define CACHE_LINE_SIZE_IN_BITS 64 * 8
#define NUM_32BIT_INTS_IN_CACHE_LINE CACHE_LINE_SIZE_IN_BITS / 32

// Test Control
#define TESTING_EFFECTS_OF_CACHE_FLUSHING false
#define REPETITIONS_OF_EXPERIMENTS 100
#define ADD_VTUNE_INSTRUMENTATION false
#define SHOULD_PREFETCH_INDEX_ARRAY false
#define TESTING_SORTEDNESS false
#define SORTEDNESS_CLUSTERED false
#define RANDOM_INDEX_ARRAY_ADDITION false
#define RANDOM_INDEX_ARRAY_ADDITION_RANGE_IN_ELEMENTS_MAX NUM_32BIT_INTS_IN_CACHE_LINE * 1024 * 1024 * 16 // 256 is where it appears to be even with the prefetching
#define RANDOM_STRIDE_FROM_PREVIOUS false
#define RANDOM_STRIDE_FROM_PREVIOUS_RANGE NUM_32BIT_INTS_IN_CACHE_LINE * 1024 * 1024 * 16
#define RANDOM_STRIDE_DISTANCE 16348 * 2 * 2 * 2 * 2 * 2 * 2
#define CONSTANT_LARGE_STRIDE_DISTANCE_MAX 1024 * 2 + 3


static void *flush_data_cache() {
    const int size = 40 * 1024 * 1024; // Allocate 40M. Much larger than L3 cache
    char *c = (char *) malloc(size);
    for (volatile int j = 0; j < size; j++) {
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
template<bool is_cache_flushed, bool is_software_prefetching_used>
static void BM_HardwarePrefetching(benchmark::State &state) {
    // Setup
    srand(time(NULL));
    auto num_elements = state.range(0);
    auto *array = (uint512_t *) malloc(sizeof(uint512_t) * num_elements);
    for (uint32_t x = 0; x < num_elements; x++) {
        array[x] = rand();
    }

    // Actual benchmark
    for (auto _ : state) {
        if constexpr (is_cache_flushed) {
            state.PauseTiming();
            free(flush_data_cache());
            state.ResumeTiming();
        }
        for (int x = 0; x < num_elements; x++) {
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
 *      So that each element in our array spans one cache line, this means that the performance between sequential access
 *      and random access are more comparable as each access is one cache line, with another structure Hardware gets to access
 *      cacheline / sizeof(element_structure) sequentially.
 */

template<bool shuffled_memory_access, bool is_cache_flushed, bool is_software_prefetching_used>
static void BM_Prefetching(benchmark::State &state) {
    // Setup
    srand(time(NULL));
    auto num_elements = state.range(0);
//    std::cout << "Num elements: " << num_elements << std::endl;
    auto *array = (int32_t *) malloc(sizeof(int32_t) * num_elements);
    for (volatile int32_t x = 0; x < num_elements; x += 1) {
        array[x] = rand();
    }

    __itt_domain *domain = __itt_domain_create("Hardware Prefetcher");
    __itt_string_handle *task = __itt_string_handle_create("Memory Load Iteration");

    // Create an index array then randomly shuffle it
    int32_t INDEX_ARRAY_SIZE = num_elements;
    auto *index_array = (int32_t *) malloc(sizeof(int32_t) * INDEX_ARRAY_SIZE);
    for (volatile int32_t x = 0; x < INDEX_ARRAY_SIZE; x++) {
        index_array[x] = x;
    }

    // Mess with the sortedness
    if constexpr(TESTING_SORTEDNESS) {
        int unsortedness = 100 - state.range(1);
        if constexpr(SORTEDNESS_CLUSTERED) {
            for (volatile int x = 0; x < INDEX_ARRAY_SIZE; x++) {
                if (x % 100 < unsortedness) {
                    index_array[x] = rand() % INDEX_ARRAY_SIZE;
                }
            }
        } else {
            for (volatile int x = 0; x < INDEX_ARRAY_SIZE; x++) {
                if (rand() % 100 < unsortedness) {
                    index_array[x] = rand() % INDEX_ARRAY_SIZE;
                }
            }
        }
    }

    if constexpr(RANDOM_INDEX_ARRAY_ADDITION) {
        int offset = state.range(1) + 1;
        for (volatile int x = 0; x < INDEX_ARRAY_SIZE; x++) {
            // The following code will add +- RANDOM_INDEX_ARRAY_ADDITION_RANGE_IN_ELEMENTS to x while keeping it in bounds
            int amount_to_add =
                    (rand() % (offset))
                    - (rand() % offset);
            int index = x + amount_to_add;
            int in_bounds = std::min(INDEX_ARRAY_SIZE, std::max(0, index));
            index_array[x] = in_bounds;
        }
    }

    if constexpr(RANDOM_STRIDE_FROM_PREVIOUS) {
        // Deal with the first array element, set it to 0 because one element will not have an effect on the
        // run time of the huge array
        index_array[0] = 0;

        int offset = state.range(1) + 1;
        for (volatile int x = 1; x < INDEX_ARRAY_SIZE; x++) {
            // The following code will add +- RANDOM_INDEX_ARRAY_ADDITION_RANGE_IN_ELEMENTS to x while keeping it in bounds
            int amount_to_add =
                    (rand() % offset)
                    - (rand() % offset);
//            std::cout << "Amount to add: " << amount_to_add << std::endl;
            int index = index_array[x - 1] + amount_to_add;
            int in_bounds = std::min(INDEX_ARRAY_SIZE, std::max(0, index));

//            std::cout << in_bounds << std::endl;
            index_array[x] = in_bounds;
        }
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
            if constexpr(SHOULD_PREFETCH_INDEX_ARRAY) {
                for (volatile int x = 0; x < std::min((ulong) INDEX_ARRAY_SIZE, CACHE_SIZE / sizeof(uint32_t)); x++) {
                    __builtin_prefetch(&index_array[x]);
                }
                // Sleep for 0.005 seconds to allow the load in from memory
                usleep(5000);
            }
            state.ResumeTiming();
        }

        if constexpr(ADD_VTUNE_INSTRUMENTATION) {
            __itt_task_begin(domain, __itt_null, __itt_null, task);
        }
        for (int x = 0; x < num_elements; x++) {
            if constexpr (is_software_prefetching_used) {
                // Taken from https://www.cl.cam.ac.uk/~sa614/papers/Software-Prefetching-CGO2017.pdf
                __builtin_prefetch(&array[index_array[x + PREFETCH_OFFSET]]);
                __builtin_prefetch(&index_array[x + 2 * PREFETCH_OFFSET]);
            }
            benchmark::DoNotOptimize(array[index_array[x]]++);
        }
        if constexpr(ADD_VTUNE_INSTRUMENTATION) {
            __itt_task_end(domain);
        }
    }

    // Teardown
    free(index_array);
    free(array);
}

template<bool is_software_prefetching_used>
static void BM_Large_Stride_Distance(benchmark::State &state) {
    // Setup
    srand(time(NULL));
    const auto num_elements_orig = state.range(0);
    const auto stride_distance = state.range(1);

    // Make sure that we access the same number of elements in each run
    auto num_elements = num_elements_orig * stride_distance;
    auto *array = (int32_t *) malloc(sizeof(int32_t) * num_elements);
    if (!array) {
        std::cout << "Could not alloc" << std::endl;
    }
    for (volatile uint64_t x = 0; x < num_elements; x += 1) {
        array[x] = rand();
    }


    __itt_domain *domain = __itt_domain_create("Hardware Prefetcher");
    __itt_string_handle *task = __itt_string_handle_create("Memory Load Iteration");

    // Actual benchmark
    for (auto _ : state) {
            state.PauseTiming();
            free(flush_data_cache());
            state.ResumeTiming();

        if constexpr(ADD_VTUNE_INSTRUMENTATION) {
            __itt_task_begin(domain, __itt_null, __itt_null, task);
        }
        for (volatile uint64_t x = 0; x < num_elements; x += stride_distance) {
            if constexpr (is_software_prefetching_used) {
                // Taken from https://www.cl.cam.ac.uk/~sa614/papers/Software-Prefetching-CGO2017.pdf
                __builtin_prefetch(&array[x + (PREFETCH_OFFSET * stride_distance)]);
            }
            benchmark::DoNotOptimize(array[x]++);
        }
        if constexpr(ADD_VTUNE_INSTRUMENTATION) {
            __itt_task_end(domain);
        }
    }

    // Teardown
    free(array);
}

// Provides arguments in the range 1..100000, skewing the Hardwareproduced arguments towards 0
static void CustomArguments(benchmark::internal::Benchmark *b) {
    /*
    for (int i = 1; i < std::min(10, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 3)
        b->Args({i});
    for (int i = 10; i < std::min(100, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 30)
        b->Args({i});
    for (int i = 100; i < std::min(1000, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 300)
        b->Args({i});
    for (int i = 1000; i < std::min(10000, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 3000)
        b->Args({i});
    for (int i = 10000; i < std::min(100000, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 30000)
        b->Args({i});
    for (int i = 100000; i < std::min(1000000, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 300000)
        b->Args({i});
    for (int i = 1000000; i < std::min(10000000, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 3000000)
        b->Args({i});
    for (int i = 10000000; i < std::min(100000001, MAX_NUM_ELEMENTS_IN_ARRAY); i+= 30000000)
        b->Args({i});
        */
    if constexpr(TESTING_SORTEDNESS) {
        for (int x = 0; x <= 100; x += 10) {
            b->Args({100000000, x});
        }
    } else if constexpr(RANDOM_INDEX_ARRAY_ADDITION) {
        for (int x = 1; x < RANDOM_INDEX_ARRAY_ADDITION_RANGE_IN_ELEMENTS_MAX; x *= 2) {
//            std::cout << x << std::endl;
            b->Args({100000000, x});
        }
    } else if constexpr(RANDOM_STRIDE_FROM_PREVIOUS) {
//        for (int x = 1; x < RANDOM_STRIDE_FROM_PREVIOUS_RANGE; x *= 2) {
//            std::cout << x << std::endl;
//            b->Args({100000000, x});
//        }
        std::cout << RANDOM_STRIDE_DISTANCE << std::endl;
        b->Args({100000000, RANDOM_STRIDE_DISTANCE});

    } else {
        b->Args({100000000});
    }
}

static void CustomArgumentsLargeStride(benchmark::internal::Benchmark *b) {
    for (int x = 1; x < CONSTANT_LARGE_STRIDE_DISTANCE_MAX; x *= 2) {
        std::cout << x << std::endl;
        b->Args({100000, x});
    }
    std::cout << "Finished processing input" << std::endl;
}

static void CustomArgumentsLargeStrideOffset(benchmark::internal::Benchmark *b) {
    for (int x = 1; x < CONSTANT_LARGE_STRIDE_DISTANCE_MAX; x *= 2) {
        std::cout << x << std::endl;
        b->Args({100000, x + 2});
    }
    std::cout << "Finished processing input" << std::endl;
}

void prefetching::register_benchmarks() {
    std::cerr << "Prefetch distance is: " << PREFETCH_OFFSET << std::endl;
    // Iterate over all of the possible combinations of the template
    if constexpr (TESTING_EFFECTS_OF_CACHE_FLUSHING) {
        // Add tests where the cache is not flushed
        BENCHMARK_TEMPLATE(BM_Prefetching, false, false, false)->Apply(CustomArguments)->Iterations(
                REPETITIONS_OF_EXPERIMENTS);
        BENCHMARK_TEMPLATE(BM_Prefetching, true, false, false)->Apply(CustomArguments)->Iterations(
                REPETITIONS_OF_EXPERIMENTS);
        BENCHMARK_TEMPLATE(BM_Prefetching, true, false, true)->Apply(CustomArguments)->Iterations(
                REPETITIONS_OF_EXPERIMENTS);
        BENCHMARK_TEMPLATE(BM_Prefetching, false, false, true)->Apply(CustomArguments)->Iterations(
                REPETITIONS_OF_EXPERIMENTS);
    }
    // Add all tests where the cache is flushed
//    BENCHMARK_TEMPLATE(BM_Prefetching, false, true, false)->Apply(CustomArguments)->MinTime(0.0001);
//    BENCHMARK_TEMPLATE(BM_Prefetching, false, true, false)->Apply(CustomArguments)->MinTime(0.0001);

//    BENCHMARK_TEMPLATE(BM_Prefetching, true, true, false)->Apply(CustomArguments)->MinTime(0.0001);
//    BENCHMARK_TEMPLATE(BM_Prefetching, true, true, true)->Apply(CustomArguments)->MinTime(0.0001);


    // Large stride analysis
    BENCHMARK_TEMPLATE(BM_Large_Stride_Distance, false)->Apply(CustomArgumentsLargeStride)->Iterations(100);
    BENCHMARK_TEMPLATE(BM_Large_Stride_Distance, true)->Apply(CustomArgumentsLargeStride)->Iterations(100);
    BENCHMARK_TEMPLATE(BM_Large_Stride_Distance, false)->Apply(CustomArgumentsLargeStrideOffset)->Iterations(100);
    BENCHMARK_TEMPLATE(BM_Large_Stride_Distance, true)->Apply(CustomArgumentsLargeStrideOffset)->Iterations(100);
}