#pragma once

#include <optional>
#include <string>

#include "application/app.hpp"

namespace bootstrap
{
/** @brief Validated startup options with no resources or live game state. */
struct Args
{
    /** @brief Source of simulation ticks: HTTP or an internal timer. */
    app::ApplicationMode mode = app::ApplicationMode::Manual;
    /** @brief Automatic tick period in milliseconds; used only in Auto mode. */
    int tick_period = 0;
    /** @brief Path to the JSON configuration of maps and game settings. */
    std::string config_file_path;
    /** @brief Root directory of client application files. */
    std::string www_root_path;
    /** @brief Allows random road spawn points when players join. */
    bool randomize_spawn_points = false;
    /** @brief Path to the active world snapshot; used when state_file_is_set is true. */
    std::string state_file_path;
    /** @brief Snapshot interval in milliseconds of game time. */
    int save_state_period = 0;
    /** @brief Whether the user enabled snapshot loading and saving. */
    bool state_file_is_set = false;
    /** @brief Whether periodic snapshot saving is enabled. */
    bool save_state_period_is_set = false;
};

/**
 * @brief Parses startup arguments and validates required paths and positive intervals.
 * @return Options, or nullopt after printing help.
 * @throws std::exception Arguments do not match the supported format.
 */
std::optional<Args> ParseCommandLine(const int argc, const char* const argv[]);
} // namespace bootstrap
