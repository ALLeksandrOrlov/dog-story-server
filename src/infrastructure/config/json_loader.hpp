#pragma once

#include <filesystem>

#include <boost/json.hpp>

#include "domain/game.hpp"

namespace json_loader
{
/**
 * @brief Загружает JSON-конфигурацию и преобразует её в валидную модель игры.
 * @details Объект хранит неизменяемую конфигурацию, включая метаданные для клиента.
 * Ошибки чтения, типов и диапазонов завершают запуск до создания HTTP-сервера.
 */
class JsonLoader
{
public:
    /** @brief Читает файл UTF-8; передаёт ошибки файловой системы и JSON вызывающему коду. */
    explicit JsonLoader(const std::filesystem::path& json_path);
    /** @brief Создаёт отдельную игру без активных сессий, проверяя все числовые настройки. */
    model::Game LoadGame() const;
    /** @brief Возвращает неизменяемую конфигурацию без копирования; ссылка живёт вместе с загрузчиком. */
    const boost::json::value& GetConfiguration() const noexcept;

private:
    /** @brief Разобранный JSON; после конструктора доступен только для чтения. */
    boost::json::value m_config;

    /** @brief Добавляет карты с наследованием настроек скорости и вместимости по умолчанию. */
    void AddMaps(model::Game& game) const;
    /** @brief Преобразует офисы и проверяет диапазоны их координат. */
    static void AddOffices(const boost::json::value& offices, model::Map& map);
    /** @brief Преобразует здания, запрещая отрицательные размеры. */
    static void AddBuildings(const boost::json::value& buildings, model::Map& map);
    /** @brief Преобразует осевые дороги с координатами, представимыми в модели. */
    static void AddRoads(const boost::json::value& roads, model::Map& map);
    /** @brief Добавляет типы предметов с неотрицательной стоимостью. */
    static void AddLoots(const boost::json::value& loots, model::Map& map);
    /** @brief Проверяет период и вероятность генератора перед передачей домену. */
    void AddLootGeneratorConfig(model::Game& game) const;
    /** @brief Применяет необязательный таймаут выхода в миллисекундах. */
    void AddDogRetirementTime(model::Game& game) const;
};
} // namespace json_loader
