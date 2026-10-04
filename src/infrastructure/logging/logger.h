#pragma once

#include <boost/json.hpp>
#include <string>

namespace logger {
namespace json = boost::json;

void PrintLog(json::value data, std::string_view message);

} // namespace logger