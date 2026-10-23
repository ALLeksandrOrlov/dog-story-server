#include <boost/asio/io_context.hpp>
#include <boost/json.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

#include "infrastructure/config/json_loader.hpp"
#include "infrastructure/http/http_server.hpp"
#include "infrastructure/http/ticker.hpp"
#include "infrastructure/persistence/postgres.hpp"

namespace
{
/** @brief Temporary configuration file for testing input boundaries. */
class ConfigurationFile
{
public:
    /** @brief Creates a dedicated directory and loads the base test configuration. */
    ConfigurationFile() :
        m_directory(std::filesystem::temp_directory_path() /
                ("infrastructure-review-" + domain::RetiredPlayerId::New().ToString())),
        m_configuration(json_loader::JsonLoader(TEST_CONFIG).GetConfiguration())
    {
        std::filesystem::create_directory(m_directory);
    }

    /** @brief Removes temporary data without throwing from the destructor. */
    ~ConfigurationFile()
    {
        std::error_code error;
        std::filesystem::remove_all(m_directory, error);
    }

    /** @brief Returns mutable configuration data solely for test setup. */
    boost::json::object& GetConfiguration()
    {
        return m_configuration.as_object();
    }

    /** @brief Saves the JSON and passes it to the real loader. */
    model::Game LoadGame() const
    {
        const auto path = m_directory / "config.json";
        std::ofstream(path) << boost::json::serialize(m_configuration);
        return json_loader::JsonLoader(path).LoadGame();
    }

private:
    std::filesystem::path m_directory;
    boost::json::value m_configuration;
};
} // namespace

TEST_CASE("Configuration rejects negative capacities and narrowing integer conversions")
{
    ConfigurationFile file;
    auto& configuration = file.GetConfiguration();
    auto& map = configuration.at("maps").as_array().front().as_object();
    const auto too_large = static_cast<std::int64_t>(std::numeric_limits<int>::max()) + 1;
    const auto too_small = static_cast<std::int64_t>(std::numeric_limits<int>::min()) - 1;

    SECTION("Negative capacity")
    {
        map["bagCapacity"] = -1;
    }
    SECTION("Overflowing capacity")
    {
        map["bagCapacity"] = too_large;
    }
    SECTION("Nonintegral capacity")
    {
        map["bagCapacity"] = 2.5;
    }
    SECTION("Negative default capacity")
    {
        configuration["defaultBagCapacity"] = -1;
    }
    SECTION("Overflowing road coordinate")
    {
        map.at("roads").as_array().front().as_object()["x0"] = too_large;
    }
    SECTION("Underflowing road coordinate")
    {
        map.at("roads").as_array().front().as_object()["y0"] = too_small;
    }
    SECTION("Overflowing office offset")
    {
        map.at("offices").as_array().front().as_object()["offsetY"] = too_large;
    }
    SECTION("Negative building dimension")
    {
        map.at("buildings").as_array().front().as_object()["w"] = -1;
    }
    SECTION("Overflowing loot value")
    {
        map.at("lootTypes").as_array().front().as_object()["value"] = too_large;
    }
    SECTION("Negative loot value")
    {
        map.at("lootTypes").as_array().front().as_object()["value"] = -1;
    }
    REQUIRE_THROWS_AS(file.LoadGame(), std::invalid_argument);
}

TEST_CASE("Zero capacity and integral boundary values remain valid configuration")
{
    ConfigurationFile file;
    auto& map = file.GetConfiguration().at("maps").as_array().front().as_object();
    map["bagCapacity"] = 0;
    map.at("offices").as_array().front().as_object()["offsetX"] = std::numeric_limits<int>::min();
    map.at("lootTypes").as_array().front().as_object()["value"] = std::numeric_limits<int>::max();
    const auto game = file.LoadGame();
    const auto* loaded_map = game.FindMap(model::Map::Id{"square"});
    REQUIRE(loaded_map->GetBagCapacity() == 0);
    REQUIRE(loaded_map->GetOffices().front().GetOffset().dx == std::numeric_limits<int>::min());
    REQUIRE(loaded_map->GetLoot(0)->value == std::numeric_limits<int>::max());
}

TEST_CASE("Infrastructure resource owners expose safe lifetime contracts")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<postgres::Database>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<postgres::Database>);
    STATIC_REQUIRE(std::has_virtual_destructor_v<http_server::SessionBase>);
    boost::asio::io_context context;
    const auto strand = boost::asio::make_strand(context);
    REQUIRE_THROWS_AS((ticker::Ticker{strand, std::chrono::milliseconds::zero(), [](auto) {}}),
            std::invalid_argument);
    REQUIRE_THROWS_AS((ticker::Ticker{strand, std::chrono::milliseconds(1), {}}), std::invalid_argument);
}
