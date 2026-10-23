#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>

#include <boost/date_time.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/make_shared.hpp>

#include "infrastructure/logging/logger.hpp"

namespace logger
{

namespace keywords = boost::log::keywords;
namespace logging = boost::log;

namespace
{
/** @brief Active file size before rotation; individual records are kept intact. */
constexpr std::size_t rotation_size = 10 * 1024 * 1024;
/** @brief Maximum number of archived files, excluding the active log. */
constexpr std::size_t retained_files = 5;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

/** @brief Formats the attributes of a Boost.Log record as a JSON string. */
void FormatRecord(const logging::record_view& record, logging::formatting_ostream& output)
{
    json::object map_obj;

    map_obj["timestamp"] = to_iso_extended_string(*record[timestamp]);
    map_obj["data"] = record[additional_data]->as_object();
    map_obj["message"] = *record[logging::expressions::smessage];

    output << map_obj;
}

/** @brief Reports an optional file log failure once. */
void ReportFileLogFailure()
{
    static std::once_flag reported;
    std::call_once(reported,
            []
            {
                std::cerr << "Optional file logging failed; JSON logging to stdout remains enabled.\n";
            });
}

/**
 * @brief Adds a file log when GAME_LOG_FILE is set.
 * @details Appends to the file after a restart; limits archive count and size.
 * Failures in the optional sink do not interrupt game request processing.
 */
void ConfigureFileLog()
{
    const char* const configured_path = std::getenv("GAME_LOG_FILE");
    if (configured_path == nullptr || *configured_path == '\0')
    {
        return;
    }

    try
    {
        const auto file_path = std::filesystem::absolute(configured_path);
        std::filesystem::create_directories(file_path.parent_path());
        using FileSink = logging::sinks::synchronous_sink<logging::sinks::text_file_backend>;
        const auto sink = boost::make_shared<FileSink>(keywords::file_name = file_path.string(),
                keywords::target_file_name = file_path.string() + ".%5N",
                keywords::open_mode = std::ios_base::out | std::ios_base::app, keywords::rotation_size = rotation_size,
                keywords::enable_final_rotation = false, keywords::auto_flush = true);
        sink->set_formatter(&FormatRecord);
        sink->set_exception_handler(&ReportFileLogFailure);
        sink->locked_backend()->set_file_collector(
                logging::sinks::file::make_collector(keywords::target = file_path.parent_path().string(),
                        keywords::max_files = retained_files, keywords::max_size = rotation_size * retained_files));
        sink->locked_backend()->scan_for_files();
        logging::core::get()->add_sink(sink);
    }
    catch(const std::exception&)
    {
        ReportFileLogFailure();
    }
}

} // namespace

void PrintLog(const json::value& data, const std::string_view message)
{
    static std::once_flag initialized;
    std::call_once(initialized,
            []
            {
                logging::add_common_attributes();
                logging::add_console_log(std::cout, keywords::format = &FormatRecord, keywords::auto_flush = true);
                ConfigureFileLog();
            });

    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << message;
}
} // namespace logger
