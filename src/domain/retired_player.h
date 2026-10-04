#pragma once

#include <string>
#include <vector>

#include "common/tagged_uuid.h"

namespace domain {
struct RetiredPlayer {
    std::string id;
    std::string name;
    int score;
    int play_time_ms;
};

namespace detail {
struct RetiredPlayerTag {};
} // namespace detail

using RetiredPlayerId = util::TaggedUUID<detail::RetiredPlayerTag>;

class RetiredPlayerRepository {
  public:
    virtual void SaveRetiredPlayer(const RetiredPlayer& player) = 0;

    virtual std::vector<RetiredPlayer> GetRetiredPlayers(int offset, int limit) = 0;

  protected:
    ~RetiredPlayerRepository() = default;
};

} // namespace domain