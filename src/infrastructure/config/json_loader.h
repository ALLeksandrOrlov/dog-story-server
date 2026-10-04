#pragma once

#include <boost/json.hpp>
#include <filesystem>
#include <optional>

#include "domain/model.h"

namespace json_loader {

class JsonLoader {
  public:
    explicit JsonLoader(const std::filesystem::path& json_path);
    model::Game LoadGame();
    std::optional<boost::json::value> GetConfig() const;

  private:
    void AddDefaultDogSpeed(model::Game& game);
    void AddMaps(model::Game& game);
    void AddOffices(model::Map& map, boost::json::value& offices_val);
    void AddBuildings(model::Map& map, boost::json::value& buildings_val);
    void AddRoads(model::Map& map, boost::json::value& roads_val);
    void AddLoots(model::Map& map, boost::json::value& loots_val);
    void AddLootGeneratorConfig(model::Game& game);
    void AddDogRetirementTime(model::Game& game);

    std::optional<boost::json::value> config_val_;
};

} // namespace json_loader
