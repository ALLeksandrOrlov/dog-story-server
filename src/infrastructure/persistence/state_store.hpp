#pragma once

#include <chrono>
#include <filesystem>
#include <optional>

#include "application/players.hpp"

namespace persistence
{
/**
 * @brief Saves the active game to a file snapshot separately from the leaderboard.
 * @details Save and OnTick run sequentially after a game update;
 * Restore runs only before application startup. An empty path disables persistence.
 */
class StateStore
{
public:
    /** @brief Sets the snapshot path and an optional positive game-time interval. */
    StateStore(std::filesystem::path path, const std::optional<int> period_ms);
    /**
     * @brief Restores a snapshot into a game with no active sessions or players.
     * @details Leaves the original objects unchanged on failure. A missing file is allowed.
     * Player velocity and timers are zero after restoration.
     */
    void Restore(model::Game& game, app::Players& players) const;
    /**
     * @brief Writes a temporary file and replaces the snapshot only after serialization succeeds.
     * @details Propagates write errors to the caller. File replacement is atomic,
     * but is not a transaction shared with PostgreSQL or a durability guarantee on power loss.
     */
    void Save(const model::Game& game, const app::Players& players) const;
    /** @brief Saves after the interval is exceeded; preserves elapsed time on write failure. */
    void OnTick(const int delta_ms, const model::Game& game, const app::Players& players);

private:
    /** @brief Final snapshot path; an empty path disables saving and restoration. */
    std::filesystem::path m_path;
    /** @brief Optional positive game-time interval in milliseconds. */
    std::optional<int> m_period_ms;
    /** @brief Game time accumulated since store creation. */
    std::chrono::milliseconds m_elapsed{0};
    /** @brief Game time of the last successful snapshot file replacement. */
    std::chrono::milliseconds m_last_save{0};
};
} // namespace persistence
