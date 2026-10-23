#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "infrastructure/config/json_loader.hpp"

namespace json_loader
{
namespace json = boost::json;
namespace
{
/** @brief Conversion factor from configuration seconds to model milliseconds. */
constexpr double milliseconds_per_second = 1000.0;
/** @brief Smallest positive configuration interval representable as whole milliseconds. */
constexpr double minimum_period_seconds = 1.0 / milliseconds_per_second;

/** @brief Reads a finite JSON number in either integer or floating-point form. */
double ReadNumber(const json::value& value)
{
    if (!value.is_number())
    {
        throw std::invalid_argument("Expected a numeric setting");
    }
    const double result = value.to_number<double>();
    if (!std::isfinite(result))
    {
        throw std::invalid_argument("Expected a finite setting");
    }
    return result;
}

/** @brief Validates type and range before narrowing a JSON integer to the model int type. */
int ReadInteger(const json::value& value, const std::string_view setting_name,
        const int minimum = std::numeric_limits<int>::min())
{
    constexpr int maximum = std::numeric_limits<int>::max();
    if (value.is_int64())
    {
        const auto integer = value.as_int64();
        if (integer >= minimum && integer <= maximum)
        {
            return static_cast<int>(integer);
        }
    }
    else if (value.is_uint64())
    {
        const auto integer = value.as_uint64();
        if (integer <= static_cast<std::uint64_t>(maximum) &&
                (minimum <= 0 || integer >= static_cast<std::uint64_t>(minimum)))
        {
            return static_cast<int>(integer);
        }
    }
    throw std::invalid_argument(std::string(setting_name) + " must be an integer in the supported range");
}

/** @brief Converts a positive interval to milliseconds without overflow. */
int ToMilliseconds(const double seconds)
{
    if (seconds < minimum_period_seconds || seconds > std::numeric_limits<int>::max() / milliseconds_per_second)
    {
        throw std::invalid_argument("Time setting is outside the supported range");
    }
    return static_cast<int>(seconds * milliseconds_per_second);
}
} // namespace

JsonLoader::JsonLoader(const std::filesystem::path& json_path)
{
    std::ifstream input(json_path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Unable to open configuration file");
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad())
    {
        throw std::runtime_error("Unable to read configuration file");
    }
    m_config = json::parse(contents.str());
}

model::Game JsonLoader::LoadGame() const
{
    model::Game game;
    AddMaps(game);
    AddLootGeneratorConfig(game);
    AddDogRetirementTime(game);
    return game;
}

const json::value& JsonLoader::GetConfiguration() const noexcept
{
    return m_config;
}

void JsonLoader::AddMaps(model::Game& game) const
{
    const auto& configuration = m_config.as_object();
    const auto& maps = configuration.at("maps").as_array();
    for(const auto& entry : maps)
    {
        const auto& description = entry.as_object();
        const std::string id(description.at("id").as_string());
        const std::string name(description.at("name").as_string());
        model::Map map(model::Map::Id{id}, name);
        AddRoads(description.at("roads"), map);
        AddBuildings(description.at("buildings"), map);
        AddOffices(description.at("offices"), map);
        AddLoots(description.at("lootTypes"), map);

        if (description.contains("dogSpeed"))
        {
            map.SetSpeed(ReadNumber(description.at("dogSpeed")));
        }
        else if (configuration.contains("defaultDogSpeed"))
        {
            map.SetSpeed(ReadNumber(configuration.at("defaultDogSpeed")));
        }
        if (map.GetSpeed() < 0)
        {
            throw std::invalid_argument("Dog speed must be nonnegative");
        }
        if (map.GetRoads().empty())
        {
            throw std::invalid_argument("A map must have at least one road");
        }
        if (description.contains("bagCapacity"))
        {
            map.SetBagCapacity(ReadInteger(description.at("bagCapacity"), "bagCapacity", 0));
        }
        else if (configuration.contains("defaultBagCapacity"))
        {
            map.SetBagCapacity(ReadInteger(configuration.at("defaultBagCapacity"), "defaultBagCapacity", 0));
        }
        game.AddMap(std::move(map));
    }
}

void JsonLoader::AddOffices(const json::value& offices, model::Map& map)
{
    for(const auto& entry : offices.as_array())
    {
        const auto& office = entry.as_object();
        const std::string id(office.at("id").as_string());
        const model::Point position{ReadInteger(office.at("x"), "office.x"), ReadInteger(office.at("y"), "office.y")};
        const model::Offset offset{ReadInteger(office.at("offsetX"), "office.offsetX"),
                ReadInteger(office.at("offsetY"), "office.offsetY")};
        map.AddOffice(model::Office{model::Office::Id{id}, position, offset});
    }
}

void JsonLoader::AddBuildings(const json::value& buildings, model::Map& map)
{
    for(const auto& entry : buildings.as_array())
    {
        const auto& building = entry.as_object();
        const model::Point position{
                ReadInteger(building.at("x"), "building.x"), ReadInteger(building.at("y"), "building.y")};
        const model::Size size{
                ReadInteger(building.at("w"), "building.w", 0), ReadInteger(building.at("h"), "building.h", 0)};
        map.AddBuilding(model::Building{model::Rectangle{position, size}});
    }
}

void JsonLoader::AddRoads(const json::value& roads, model::Map& map)
{
    for(const auto& entry : roads.as_array())
    {
        const auto& road = entry.as_object();
        const model::Point start{ReadInteger(road.at("x0"), "road.x0"), ReadInteger(road.at("y0"), "road.y0")};
        if (road.contains("x1"))
        {
            map.AddRoad(model::Road{model::Road::HORIZONTAL, start, ReadInteger(road.at("x1"), "road.x1")});
        }
        else if (road.contains("y1"))
        {
            map.AddRoad(model::Road{model::Road::VERTICAL, start, ReadInteger(road.at("y1"), "road.y1")});
        }
        else
        {
            throw std::invalid_argument("Road requires x1 or y1");
        }
    }
}

void JsonLoader::AddLoots(const json::value& loots, model::Map& map)
{
    const auto& types = loots.as_array();
    if (types.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("Too many loot types");
    }
    int type = 0;
    for(const auto& entry : types)
    {
        const auto& loot = entry.as_object();
        const int value = ReadInteger(loot.at("value"), "loot.value", 0);
        map.AddLoot(type, {type, value});
        ++type;
    }
}

void JsonLoader::AddLootGeneratorConfig(model::Game& game) const
{
    const auto& settings = m_config.as_object().at("lootGeneratorConfig").as_object();
    const double period = ReadNumber(settings.at("period"));
    ToMilliseconds(period);
    const double probability = ReadNumber(settings.at("probability"));
    if (probability < 0 || probability > 1)
    {
        throw std::invalid_argument("Probability must be between zero and one");
    }
    game.SetLootGeneratorConfig({period, probability});
}

void JsonLoader::AddDogRetirementTime(model::Game& game) const
{
    const auto& settings = m_config.as_object();
    if (settings.contains("dogRetirementTime"))
    {
        game.SetDogRetirementTime(ToMilliseconds(ReadNumber(settings.at("dogRetirementTime"))));
    }
}
} // namespace json_loader
