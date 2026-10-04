#include "infrastructure/logging/logger.h"
#include <boost/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <mutex>

namespace logger {

namespace keywords = boost::log::keywords;
namespace logging = boost::log;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object map_obj;

    map_obj["timestamp"] = to_iso_extended_string(*rec[timestamp]);
    map_obj["data"] = rec[additional_data]->as_object();
    map_obj["message"] = *rec[logging::expressions::smessage];

    strm << map_obj;
}

void PrintLog(json::value data, std::string_view message) {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        logging::add_common_attributes();
        logging::add_console_log(std::cout, keywords::format = &MyFormatter, keywords::auto_flush = true);
    });

    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << message;
}
} // namespace logger