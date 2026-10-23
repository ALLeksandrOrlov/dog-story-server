/**
 * @file
 * @brief Splits and trims strings without copying characters.
 */
#include "common/string_processing.hpp"

namespace util
{
std::string_view Trim(const std::string_view text)
{
    const auto start = text.find_first_not_of(' ');
    if (start == text.npos)
    {
        return {};
    }
    return text.substr(start, text.find_last_not_of(' ') + 1 - start);
}

std::vector<std::string_view> Split(const std::string_view text, const char delimiter)
{
    std::vector<std::string_view> result;
    std::size_t position = text.find_first_not_of(' ');
    while(position < text.length())
    {
        auto delimiter_position = text.find(delimiter, position);
        if (delimiter_position == text.npos)
        {
            delimiter_position = text.size();
        }
        const auto fragment = Trim(text.substr(position, delimiter_position - position));
        if (!fragment.empty())
        {
            result.emplace_back(fragment);
        }
        position = text.find_first_not_of(' ', delimiter_position + 1);
    }
    return result;
}
} // namespace util
