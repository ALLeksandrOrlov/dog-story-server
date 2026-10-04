#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "infrastructure/config/json_loader.h"
#include <fstream>

using namespace std;

namespace json_loader {

namespace json = boost::json;
namespace {
double Number(const json::value& value) {
    if (!value.is_number())
        throw std::invalid_argument("Expected a numeric setting");
    double result = value.to_number<double>();
    if (!std::isfinite(result))
        throw std::invalid_argument("Expected a finite setting");
    return result;
}
int Milliseconds(double seconds) {
    if (seconds < 0.001 || seconds > std::numeric_limits<int>::max() / 1000.0) {
        throw std::invalid_argument("Time setting is outside the supported range");
    }
    return static_cast<int>(seconds * 1000.0);
}
} // namespace

JsonLoader::JsonLoader(const std::filesystem::path& json_path) {
    ifstream ifs(json_path, ios::binary);

    std::ostringstream sstream;
    if (ifs.is_open()) {
        sstream << ifs.rdbuf();
        ifs.close();
    } else {
        throw std::runtime_error("Unable to open file"s);
    }

    config_val_ = json::parse(sstream.str());
}

model::Game JsonLoader::LoadGame() {

    if (!config_val_.has_value()) {
        throw std::runtime_error("Config not loaded");
    }

    model::Game game;

    AddMaps(game);
    AddLootGeneratorConfig(game);
    AddDogRetirementTime(game);

    return game;
}

std::optional<json::value> JsonLoader::GetConfig() const {
    return config_val_;
}

void JsonLoader::AddMaps(model::Game& game) {
    auto config_obj = config_val_->as_object();
    auto maps_array = config_obj.at("maps").as_array();

    for (const auto& map_val : maps_array) {
        string_view id = map_val.as_object().at("id").as_string();
        string_view name = map_val.as_object().at("name").as_string();
        int loot_types_num = map_val.as_object().at("lootTypes").as_array().size();

        model::Map next_map(model::Map::Id{string{id}}, string{name});

        auto roads_val = map_val.as_object().at("roads");
        AddRoads(next_map, roads_val);

        auto buildings_val = map_val.as_object().at("buildings");
        AddBuildings(next_map, buildings_val);

        auto offices_val = map_val.as_object().at("offices");
        AddOffices(next_map, offices_val);

        auto loots_val = map_val.as_object().at("lootTypes");
        AddLoots(next_map, loots_val);

        if (map_val.as_object().contains("dogSpeed")) {
            next_map.SetSpeed(Number(map_val.as_object().at("dogSpeed")));
        } else if (config_obj.contains("defaultDogSpeed")) {
            next_map.SetSpeed(Number(config_obj.at("defaultDogSpeed")));
        }
        if (next_map.GetSpeed() < 0)
            throw std::invalid_argument("Dog speed must be nonnegative");
        if (next_map.GetRoads().empty())
            throw std::invalid_argument("A map must have at least one road");

        if (map_val.as_object().count("bagCapacity") > 0) {
            auto capacity_val = map_val.as_object().at("bagCapacity");
            if (capacity_val.is_int64()) {
                int capacity = capacity_val.as_int64();
                next_map.SetBagCapacity(capacity);
            }
        } else if (config_obj.count("defaultBagCapacity") > 0) {
            auto bag_capacity_val = config_obj.at("defaultBagCapacity");
            if (bag_capacity_val.is_int64()) {
                int bag_capacity = bag_capacity_val.as_int64();
                next_map.SetBagCapacity(bag_capacity);
            }
        }

        game.AddMap(next_map);
    }
}

void JsonLoader::AddOffices(model::Map& map, json::value& offices_val) {
    auto offices_array = offices_val.as_array();
    for (const auto& office_val : offices_array) {
        auto office_obj = office_val.as_object();

        string_view office_id = office_obj.at("id").as_string();
        model::Coord x = office_obj.at("x").as_int64();
        model::Coord y = office_obj.at("y").as_int64();
        model::Coord off_x = office_obj.at("offsetX").as_int64();
        model::Coord off_y = office_obj.at("offsetY").as_int64();

        model::Office next_office(model::Office::Id{string{office_id}}, {x, y}, {off_x, off_y});
        map.AddOffice(next_office);
    }
}

void JsonLoader::AddBuildings(model::Map& map, json::value& buildings_val) {
    auto buildings_array = buildings_val.as_array();
    for (const auto& building_val : buildings_array) {
        auto building_obj = building_val.as_object();

        model::Coord x = building_obj.at("x").as_int64();
        model::Coord y = building_obj.at("y").as_int64();

        model::Dimension w = building_obj.at("w").as_int64();
        model::Dimension h = building_obj.at("h").as_int64();

        model::Rectangle bounds({x, y}, {w, h});

        model::Building next_building(bounds);
        map.AddBuilding(next_building);
    }
}

void JsonLoader::AddRoads(model::Map& map, json::value& roads_val) {
    auto roads_array = roads_val.as_array();
    for (const auto& road_val : roads_array) {
        auto road_obj = road_val.as_object();

        model::Coord x0 = road_obj.at("x0").as_int64();
        model::Coord y0 = road_obj.at("y0").as_int64();
        model::Point start(x0, y0);

        if (road_obj.count("x1") > 0) {
            model::Coord x1 = road_obj.at("x1").as_int64();
            model::Road road(model::Road::HORIZONTAL, start, x1);
            map.AddRoad(road);
        } else if (road_obj.count("y1") > 0) {
            model::Coord y1 = road_obj.at("y1").as_int64();
            model::Road next_road(model::Road::VERTICAL, start, y1);
            map.AddRoad(next_road);
        } else {
            throw std::invalid_argument("invalid json format");
        }
    }
}

void JsonLoader::AddLoots(model::Map& map, json::value& loots_val) {
    auto loots_array = loots_val.as_array();
    int loot_type = 0;
    for (const auto& loot_val : loots_array) {
        auto loot_obj = loot_val.as_object();

        if (loot_obj.count("value") > 0) {
            int value = loot_obj.at("value").as_int64();
            map.AddLoot(loot_type, {loot_type, value});
            loot_type++;
        } else {
            throw std::invalid_argument("invalid json format");
        }
    }
}

void JsonLoader::AddLootGeneratorConfig(model::Game& game) {
    auto config_obj = config_val_->as_object();
    auto gen_config_obj = config_obj.at("lootGeneratorConfig").as_object();
    model::LootGeneratorConfig gen_config;
    gen_config.period = Number(gen_config_obj.at("period"));
    Milliseconds(gen_config.period);
    gen_config.probability = Number(gen_config_obj.at("probability"));
    if (gen_config.probability < 0 || gen_config.probability > 1)
        throw std::invalid_argument("Probability must be between zero and one");
    game.SetLootGeneratorConfig(gen_config);
}

void JsonLoader::AddDogRetirementTime(model::Game& game) {
    auto config_obj = config_val_->as_object();
    if (config_obj.count("dogRetirementTime") > 0) {
        int retirement_time = Milliseconds(Number(config_obj.at("dogRetirementTime")));
        game.SetDogRetirementTime(retirement_time);
    }
}

} // namespace json_loader
