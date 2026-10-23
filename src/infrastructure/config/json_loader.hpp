#pragma once

#include <filesystem>

#include <boost/json.hpp>

#include "domain/game.hpp"

namespace json_loader
{
/**
 * @brief Loads JSON configuration and converts it to a valid game model.
 * @details Stores immutable configuration, including client metadata.
 * Read, type, and range errors abort startup before the HTTP server is created.
 */
class JsonLoader
{
public:
    /** @brief Reads a UTF-8 file; propagates filesystem and JSON errors to the caller. */
    explicit JsonLoader(const std::filesystem::path& json_path);
    /** @brief Creates a separate game without active sessions, validating all numeric settings. */
    model::Game LoadGame() const;
    /** @brief Returns immutable configuration without copying; the reference lasts as long as the loader. */
    const boost::json::value& GetConfiguration() const noexcept;

private:
    /** @brief Parsed JSON; read-only after construction. */
    boost::json::value m_config;

    /** @brief Adds maps, inheriting default speed and bag capacity settings. */
    void AddMaps(model::Game& game) const;
    /** @brief Converts offices and validates their coordinate ranges. */
    static void AddOffices(const boost::json::value& offices, model::Map& map);
    /** @brief Converts buildings, rejecting negative dimensions. */
    static void AddBuildings(const boost::json::value& buildings, model::Map& map);
    /** @brief Converts axis-aligned roads with coordinates representable in the model. */
    static void AddRoads(const boost::json::value& roads, model::Map& map);
    /** @brief Adds loot types with nonnegative values. */
    static void AddLoots(const boost::json::value& loots, model::Map& map);
    /** @brief Validates the generator interval and probability before passing them to the domain. */
    void AddLootGeneratorConfig(model::Game& game) const;
    /** @brief Applies the optional retirement timeout in milliseconds. */
    void AddDogRetirementTime(model::Game& game) const;
};
} // namespace json_loader
