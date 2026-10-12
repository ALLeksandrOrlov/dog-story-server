#pragma once
#ifdef GAME_BASELINE
#include "retired_player.h"
#else
#include "domain/retired_player.hpp"
#endif
#include <algorithm>
#include <tuple>

class MemoryRepository : public domain::RetiredPlayerRepository {
  public:
    void SaveRetiredPlayer(const domain::RetiredPlayer& player) override {
        records.push_back(player);
    }
    std::vector<domain::RetiredPlayer> GetRetiredPlayers(int offset, int limit) override {
        auto sorted = records;
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            if (a.score != b.score)
                return a.score > b.score;
            return std::tie(a.play_time_ms, a.name) < std::tie(b.play_time_ms, b.name);
        });
        auto first = std::clamp(offset, 0, static_cast<int>(sorted.size()));
        auto last = std::clamp(first + std::max(0, limit), first, static_cast<int>(sorted.size()));
        return {sorted.begin() + first, sorted.begin() + last};
    }
    std::vector<domain::RetiredPlayer> records;
};
