#pragma once

#include <string_view>

#include <boost/json.hpp>

namespace logger
{
namespace json = boost::json;

/**
 * @brief Потокобезопасно записывает одно событие в JSON-журнал стандартного вывода.
 * @param data Объект с полями события; должен иметь JSON-тип object.
 * @param message Название события, копируемое в запись журнала.
 * @details Настройка общего журнала выполняется однократно при первом обращении.
 * Непустой GAME_LOG_FILE дополнительно включает файл с ротацией: 10 MiB и до пяти архивов.
 * Сбой необязательного файла не отключает стандартный вывод и не прерывает запрос.
 */
void PrintLog(const json::value& data, const std::string_view message);
} // namespace logger
