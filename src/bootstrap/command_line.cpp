#include <iostream>

#include <boost/program_options.hpp>

#include "bootstrap/command_line.hpp"
namespace bootstrap
{
using namespace std::literals;
namespace po = boost::program_options;
[[nodiscard]] std::optional<Args> ParseCommandLine(const int argc, const char* const argv[])
{
    po::options_description description{"All options"s};
    Args args{};
    auto add_option = description.add_options();
    add_option("tick-period,t", po::value<int>(&args.tick_period), "set tick period");
    add_option("config-file,c", po::value(&args.config_file_path), "set config file path");
    add_option("www-root,w", po::value(&args.www_root_path), "set static files root");
    add_option("randomize-spawn-points", "spawn dogs at random positions");
    add_option("help,h", "produce help message");
    add_option("state-file", po::value(&args.state_file_path), "set state file path");
    add_option("save-state-period", po::value<int>(&args.save_state_period), "save state period in ms");

    po::variables_map options;
    po::store(po::parse_command_line(argc, argv, description), options);
    po::notify(options);

    if (options.contains("help"s))
    {
        std::cout << description;
        return std::nullopt;
    }

    if (!options.contains("tick-period"s))
    {
        args.mode = app::ApplicationMode::Manual;
    }
    else
    {
        args.mode = app::ApplicationMode::Auto;
    }

    if (!options.contains("config-file"s))
    {
        throw std::runtime_error("Specify --help for usage"s);
    }

    if (!options.contains("www-root"s))
    {
        throw std::runtime_error("Specify --help for usage"s);
    }

    args.randomize_spawn_points = options.contains("randomize-spawn-points"s);

    args.state_file_is_set = options.contains("state-file"s);
    args.save_state_period_is_set = options.contains("save-state-period"s);

    if ((options.contains("tick-period") && args.tick_period <= 0) ||
            (args.save_state_period_is_set && args.save_state_period <= 0))
    {
        throw std::invalid_argument("Tick and save periods must be positive");
    }
    return args;
}

} // namespace bootstrap
