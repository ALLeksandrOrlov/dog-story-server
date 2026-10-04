#include "view/json_view.h"

namespace view {
namespace json = boost::json;
JsonView::JsonView(const json::value& config) {
    for (const auto& entry : config.as_object().at("maps").as_array()) {
        const auto& full = entry.as_object();
        json::object description;
        for (const char* key : {"id", "name", "roads", "buildings", "offices", "lootTypes"}) {
            description[key] = full.at(key);
        }
        map_descriptions_.emplace(std::string(full.at("id").as_string()), std::move(description));
    }
}
json::array JsonView::Render(const app::GetMapsResult& result) const {
    json::array maps;
    for (const auto& map : result.maps) {
        maps.push_back(json::object{{"id", map.id}, {"name", map.name}});
    }
    return maps;
}
json::object JsonView::Render(const app::GetMapDescResult& result) const {
    return map_descriptions_.at(*result.id);
}
json::object JsonView::Render(const app::JoinGameResult& result) const {
    return {{"authToken", *result.token}, {"playerId", *result.player_id}};
}
json::object JsonView::Render(const app::GetPlayersListResult& result) const {
    json::object players;
    for (const auto& player : result.players) {
        players[std::to_string(*player.id)] = json::object{{"name", player.name}};
    }
    return players;
}
json::object JsonView::Render(const app::GetGameStateResult& result) const {
    json::object players;
    for (const auto& player : result.players) {
        json::array bag;
        for (const auto& loot : player.bag) {
            bag.push_back(json::object{{"id", *loot.GetId()}, {"type", loot.GetProps().type}});
        }
        players[std::to_string(*player.id)] =
            json::object{{"pos", json::array{player.position.x, player.position.y}},
                         {"speed", json::array{player.speed.x, player.speed.y}},
                         {"dir", player.direction},
                         {"bag", std::move(bag)},
                         {"score", player.score}};
    }
    json::object lost_objects;
    for (const auto& loot : result.lost_objects) {
        lost_objects[std::to_string(*loot.GetId())] = json::object{
            {"type", loot.GetProps().type}, {"pos", json::array{loot.GetPos().x, loot.GetPos().y}}};
    }
    return {{"players", std::move(players)}, {"lostObjects", std::move(lost_objects)}};
}
json::array JsonView::Render(const app::GetGameRecordsResult& result) const {
    json::array records;
    for (const auto& record : result.records) {
        records.push_back(json::object{{"name", record.name},
                                       {"score", record.score},
                                       {"playTime", static_cast<double>(record.play_time_ms) / 1000.}});
    }
    return records;
}
} // namespace view
