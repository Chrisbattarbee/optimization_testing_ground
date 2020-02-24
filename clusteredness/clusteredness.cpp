//
// Created by chris on 24/02/2020.
//

#include "clusteredness.h"

#define NUM_ITERATIONS 100000000

static void BM_RandomBranch(benchmark::State& state) {
    // Setup
    double selectivity = ((float) state.range(0)) / 10.0;
    double clusteredness = ((float)state.range(1)) / 10.0;

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
    for (int i = 1; i <= 10; i++)
        for (int j = 1; j <= 10; j++)
            b->Args({i, j});
}


void clusteredness::register_benchmarks() {
    BENCHMARK(BM_RandomBranch)->Apply(CustomArguments);
}
