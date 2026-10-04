#pragma once
#include "application/players.h"
#include <chrono>
#include <filesystem>
#include <optional>
namespace persistence {
class StateStore {
  public:
    StateStore(std::filesystem::path path, std::optional<int> period_ms);
    void Restore(model::Game& game, app::Players& players) const;
    void Save(const model::Game& game, const app::Players& players) const;
    void OnTick(int delta_ms, model::Game& game, const app::Players& players);

  private:
    std::filesystem::path path_;
    std::optional<int> period_ms_;
    std::chrono::milliseconds elapsed_{0};
    std::chrono::milliseconds last_save_{0};
};
} // namespace persistence
