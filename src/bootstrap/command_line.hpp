#pragma once

#include <optional>
#include <string>

#include "application/app.hpp"

namespace bootstrap
{
/** @brief Проверенные настройки запуска, не содержащие ресурсов или состояния работающей игры. */
struct Args
{
    /** @brief Источник тиков симуляции: HTTP или внутренний таймер. */
    app::ApplicationMode mode = app::ApplicationMode::Manual;
    /** @brief Период автоматического тика в миллисекундах; используется только в режиме Auto. */
    int tick_period = 0;
    /** @brief Путь к JSON-конфигурации карт и игровых настроек. */
    std::string config_file_path;
    /** @brief Корневой каталог файлов клиентского приложения. */
    std::string www_root_path;
    /** @brief Разрешает выбирать случайную точку дороги при входе игрока. */
    bool randomize_spawn_points = false;
    /** @brief Путь снимка активного мира; применяется при state_file_is_set. */
    std::string state_file_path;
    /** @brief Период сохранения по игровому времени в миллисекундах. */
    int save_state_period = 0;
    /** @brief Указывает, что пользователь включил загрузку и сохранение снимка. */
    bool state_file_is_set = false;
    /** @brief Указывает, что включено периодическое сохранение снимка. */
    bool save_state_period_is_set = false;
};

/**
 * @brief Разбирает аргументы запуска и проверяет обязательные пути и положительные периоды.
 * @return Настройки либо nullopt после печати справки.
 * @throws std::exception Если аргументы не соответствуют поддерживаемому формату.
 */
std::optional<Args> ParseCommandLine(const int argc, const char* const argv[]);
} // namespace bootstrap
