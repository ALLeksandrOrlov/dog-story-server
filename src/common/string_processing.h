#pragma once
#include <string>
#include <vector>

std::string_view Trim(std::string_view string);
std::vector<std::string_view> Split(std::string_view string, char delim);