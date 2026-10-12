#pragma once
/**
 * @file
 * @brief Не владеющие памятью операции разбора строк протокола.
 */

#include <string_view>
#include <vector>

namespace util
{
/** @brief Возвращает представление строки без крайних пробелов; память принадлежит исходной строке. */
std::string_view Trim(const std::string_view text);
/**
 * @brief Разделяет строку по символу, удаляя крайние пробелы и пустые элементы.
 * @return Представления фрагментов. Исходная строка должна жить дольше результата.
 */
std::vector<std::string_view> Split(const std::string_view text, const char delimiter);
} // namespace util
