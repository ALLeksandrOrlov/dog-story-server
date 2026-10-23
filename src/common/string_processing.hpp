#pragma once
/**
 * @file
 * @brief Non-owning operations for parsing protocol strings.
 */

#include <string_view>
#include <vector>

namespace util
{
/** @brief Returns a trimmed view of a string; the original string owns the memory. */
std::string_view Trim(const std::string_view text);
/**
 * @brief Splits a string on a character, trimming whitespace and removing empty elements.
 * @return Views of the fragments. The original string must outlive the result.
 */
std::vector<std::string_view> Split(const std::string_view text, const char delimiter);
} // namespace util
