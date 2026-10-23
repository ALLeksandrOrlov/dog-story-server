#pragma once

#include <string_view>

#include <boost/json.hpp>

namespace logger
{
namespace json = boost::json;

/**
 * @brief Writes one event to the JSON log on standard output in a thread-safe manner.
 * @param data Event fields; must have the JSON object type.
 * @param message Event name copied into the log record.
 * @details Initializes the shared log once on first use.
 * A nonempty GAME_LOG_FILE also enables file rotation at 10 MiB with up to five archives.
 * An optional file failure neither disables standard output nor interrupts the request.
 */
void PrintLog(const json::value& data, const std::string_view message);
} // namespace logger
