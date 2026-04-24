#pragma once

#include "model/model.h"
#include "common/aliases.h"

struct BenchmarkStats {
    i32 m1_score;
    i32 m2_score;
    i32 draws;
};

struct Benchmark{
    Model* m1;
    Model* m2;

    Benchmark(Model* m1, Model* m2);

    BenchmarkStats stats;
    
    BenchmarkStats run_benchmark(Game& game, u32 iters);

    void print_stats();
};