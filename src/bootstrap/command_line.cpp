#include "bootstrap/command_line.h"
#include <boost/program_options.hpp>
#include <iostream>
namespace bootstrap {
using namespace std::literals;
namespace po = boost::program_options;
[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc{"All options"s};
    Args args{};
    auto desc_init = desc.add_options();
    desc_init("tick-period,t", po::value<int>(&args.tick_period), "set tick period");
    desc_init("config-file,c", po::value(&args.config_file_path), "set config file path");
    desc_init("www-root,w", po::value(&args.www_root_path), "set static files root");
    desc_init("randomize-spawn-points", "spawn dogs at random positions");
    desc_init("help,h", "produce help message");
    desc_init("state-file", po::value(&args.state_file_path), "set state file path");
    desc_init("save-state-period", po::value<int>(&args.save_state_period), "save state period in ms");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.contains("tick-period"s)) {
        args.mode = app::ApplicationMode::Manual;
    } else {
        args.mode = app::ApplicationMode::Auto;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Specify --help for usage"s);
    }

    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Specify --help for usage"s);
    }

    args.randomize_spawn_points = vm.contains("randomize-spawn-points"s);

    args.state_file_is_set = vm.contains("state-file"s);
    args.save_state_period_is_set = vm.contains("save-state-period"s);

    if ((vm.contains("tick-period") && args.tick_period <= 0) ||
        (args.save_state_period_is_set && args.save_state_period <= 0)) {
        throw std::invalid_argument("Tick and save periods must be positive");
    }
    return args;
};

} // namespace bootstrap
