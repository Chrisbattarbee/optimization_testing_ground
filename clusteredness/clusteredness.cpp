//
// Created by chris on 24/02/2020.
//

#include "clusteredness.h"
#include <cmath>
#include "ittnotify.h"

/*
 * Arguments.
 * 1) Selectivity (%), a measure of how likely a branch is to be taken
 * 2) Clusteredness (%), a measure of how likely the result of a branch is to be the same as the immediately preceding
 *    (in time) branch
 *
 * Optimization Hypothesis.
 * Compilers currently make use of an optimization known as if conversion.
 * The premise behind if conversion is that mispredicted branches are expensive and we would like to reduce the number
 * of mispredictions as much as possible. There is an assembly level instruction known as CMOVxx which will
 * conditionally perform a mov instruction based on the value of some prior comparison.
 * Therefore we can remove volatile branches by introducing cmov instructions in their place. However, modern compilers
 * currently do this based on the aggregate selectivity of a branch and do not utilise any underlying trend data.
 * This can lead to mis-optimizations. This optimization attempts to remove these mis-optimizations by utilizing a
 * trend characteristic known as selectivity which is the statistical probability that a branch instruction has the
 * same result as the immediately preceding branch.
 *
 * Implementation Details.
 * 1. We malloc some space (NUM_ITERATIONS * int_size).
 * 2. We then iterate over this array, assigning values to elements in this array either randomly or the same value
 *    as the preceding element with selection % probability.
 * 3. We then perform an aggregation based on this underlying data where we take a branch with selectivity % probability.
 *
 * Step 2 allows us to capture the effects of clusteredness.
 * Step 3 allows us to capture the effects of selectivity.
 *
 * Benchmark results.
 * We see that the benchmark takes longer to execute at selectivity = 50% without clusteredness coming into play
 * However with high clusteredness, the runtime approaches that of low selectivity scores.
 * Therefore, removing if conversion when a branch has high clusteredness appears to be a valuable optimization.
*/
#define NUM_ITERATIONS 1000000000
static void BM_Clusteredness(benchmark::State& state) {
    // Setup
    double selectivity = ((float) state.range(0)) / 100.0;
    double clusteredness = ((float)state.range(1)) / 100.0;

    long selectivity_pivot_position = RAND_MAX * selectivity;
    long clusteredness_pivot_position = RAND_MAX * clusteredness;

    auto* array = (uint*) malloc(sizeof(u_int32_t) * NUM_ITERATIONS);
    srand(time(NULL));
    int prev = random();
    for (int x = 0; x < NUM_ITERATIONS; x ++) {
        if (random() < clusteredness_pivot_position) {
            array[x] = prev;
        } else {
            array[x] = random();
        }
        prev = array[x];
    }

    // Actual benchmark
    for (auto _ : state) {
        volatile __uint32_t total = 0;
        for (__uint32_t x = 0; x < NUM_ITERATIONS; x ++) {
            if (array[x] < selectivity_pivot_position) {
                total += 1;
            } else {
                total += 2;
            }
        }
    }

    // Teardown
    free(array);
}

__attribute__ ((pure))
int expensive_function1() {
    return rand();
}

__attribute__ ((pure))
int expensive_function2() {
    return rand();
}

static void BM_Clusteredness_New(benchmark::State& state) {
    int* yesNoArr = (int*) malloc(sizeof(int) * NUM_ITERATIONS);
    for (volatile int x = 0; x < NUM_ITERATIONS; x ++) {
        if (x < NUM_ITERATIONS / 2) {
            yesNoArr[x] = 0;
        } else {
            yesNoArr[x] = 1;
        }
    }

    __itt_domain *domain = __itt_domain_create("Hardware Prefetcher");
    __itt_string_handle *task = __itt_string_handle_create("Memory Load Iteration");
    __itt_task_begin(domain, __itt_null, __itt_null, task);

    // Actual benchmark
    double a = 100;
    for (auto _ : state) {
        for (volatile uint32_t x = 0; x < NUM_ITERATIONS; x ++) {
            double b = (float) x;
            a /= x < NUM_ITERATIONS / 2 ? b * 147: 0.5 ;
        }
    }
    benchmark::DoNotOptimize(a);

    __itt_task_end(domain);

}


// Provides arguments of the cross product of [0..101, 10] x [0..101, 10]
static void CustomArguments(benchmark::internal::Benchmark* b) {
    for (int i = 0; i <= 100; i+= 10)
        for (int j = 0; j <= 100; j+= 10)
            b->Args({i, j});
}


void clusteredness::register_benchmarks() {
    BENCHMARK(BM_Clusteredness_New)->Iterations(10);
}