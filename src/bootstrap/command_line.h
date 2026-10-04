#pragma once
#include "application/app.h"
#include <optional>
#include <string>
namespace bootstrap {
struct Args {
    app::ApplicationMode mode;
    int tick_period;
    std::string config_file_path;
    std::string www_root_path;
    bool randomize_spawn_points;
    std::string state_file_path;
    int save_state_period;
    bool state_file_is_set;
    bool save_state_period_is_set;
};

std::optional<Args> ParseCommandLine(int argc, const char* const argv[]);
} // namespace bootstrap
