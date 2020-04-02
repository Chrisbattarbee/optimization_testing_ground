//
// Created by chris on 24/02/2020.
//

#include "clusteredness.h"

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
*/
#define NUM_ITERATIONS 100000000
static void BM_Clusteredness(benchmark::State& state) {
    // Setup
    double selectivity = ((float) state.range(0)) / 100.0;
    double clusteredness = ((float)state.range(1)) / 100.0;

    long selectivity_pivot_position = RAND_MAX * selectivity;
    long clusteredness_pivot_position = RAND_MAX * clusteredness;

    // Set up
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
        // Actual Benchmark
        __uint32_t total = 0;
        for (__uint32_t x = 0; x < NUM_ITERATIONS; x ++) {
            if (array[x] < selectivity_pivot_position) {
                total += 1;
            } else {
                total += 2;
            }
        }
    }
    free(array);
}


static void CustomArguments(benchmark::internal::Benchmark* b) {
    for (int i = 0; i <= 100; i+= 10)
        for (int j = 0; j <= 100; j+= 10)
            b->Args({i, j});
}


void clusteredness::register_benchmarks() {
    BENCHMARK(BM_Clusteredness)->Apply(CustomArguments);
}
