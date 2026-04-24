#include "arena/benchmark.h"
#include "arena/arena.h"
#include <iostream>

Benchmark::Benchmark(Model* m1, Model* m2) {
    this->m1 = m1;
    this->m2 = m2;
}

BenchmarkStats Benchmark::run_benchmark(Game& game, u32 iters) {
    stats.m1_score = 0;
    stats.m2_score = 0;
    stats.draws = 0;

    bool m1_white = true;


    for (u32 i = 0; i < iters; i++) {

        Game b_game = game;

        Model* white_player = (m1_white) ? m1 : m2;
        Model* black_player = (m1_white) ? m2 : m1;

        Arena arena(&b_game, white_player, black_player);
        ArenaResult result = arena.play();

        switch (result) {
            case WHITE_WIN:
                if (m1_white) {
                    stats.m1_score++;
                }else {
                    stats.m2_score++;
                }
                break;
            case BLACK_WIN:
                if (m1_white) {
                    stats.m2_score++;
                } else {
                    stats.m1_score++;
                }
                break;
            case DRAW:
                stats.draws++;
                break;
        }

        if (i % 2) {
            print_stats();
        }

        m1_white = !m1_white;
    }

    return stats;
}

void Benchmark::print_stats() {
    std::cout << "Model 1 score: " << stats.m1_score << "\n";
    std::cout << "Model 2 score: " << stats.m2_score << "\n";
    std::cout << "Draws: " << stats.draws << "\n";
    std::cout << "------------------------------\n";
}
