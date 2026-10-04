#include "application/app.h"
#include <algorithm>
#include <cctype>

namespace app {
using namespace std::literals;
namespace {
constexpr int kTokenSize = 32;
}

Application::Application(Game& game, Players& players, ApplicationMode mode, bool is_random_spawn_point,
                         std::function<void(int)> on_update, domain::RetiredPlayerRepository& players_repo)
    : game_(game), players_(players), mode_(mode), is_random_spawn_point_(is_random_spawn_point),
      on_update_(on_update), players_repo_(players_repo) {
    auto& sessions = game_.GetSessions();
    for (auto& session : sessions) {
        BindSession(session);
    }
}

GetMapsResult Application::GetMaps() {
    GetMapsResult result;
    for (const auto& map : game_.GetMaps()) {
        result.maps.push_back({*map.GetId(), map.GetName()});
    }
    return result;
}

GetMapDescResult Application::GetMapDesc(Map::Id map_id) {
    if (game_.FindMap(map_id) == nullptr) {
        throw ApplicationError(ApplicationErrorReason::InvalidMap, "");
    }
    return {std::move(map_id)};
}

JoinGameResult Application::JoinGame(Map::Id map_id, std::string_view user_name) {
    using namespace std::literals;
    if (user_name.empty()) {
        throw ApplicationError(ApplicationErrorReason::InvalidName, ""s);
    }

    const model::Map* map = game_.FindMap(map_id);
    if (map == nullptr) {
        throw ApplicationError(ApplicationErrorReason::InvalidMap, ""s);
    }

    if (game_.FindSession(map_id) == nullptr) {
        auto session_id = GameSession::Id(game_.GetNextSessionId());
        game_.AddSession(
            GameSession(session_id, map, game_.GetLootGeneratorConfig(), game_.GetDogRetirementTime()));
        model::GameSession* session_ptr = game_.FindSession(map_id);
        BindSession(*session_ptr);
    }

    model::GameSession* session_ptr = game_.FindSession(map_id);

    model::Position spawn_point;
    if (is_random_spawn_point_) {
        spawn_point = map->GetRandomPoint();
    } else {
        model::Point road_point = map->GetRoads()[0].GetStart();
        spawn_point = {static_cast<double>(road_point.x), static_cast<double>(road_point.y)};
    }

    double dog_speed = map->GetSpeed();
    auto dog_id = Dog::Id(session_ptr->GetNextDogId());
    session_ptr->AddDog(
        Dog(dog_id, std::string(user_name), spawn_point, dog_speed, "U"s, map->GetBagCapacity(), 0));
    auto dog_ptr = session_ptr->FindDog(dog_id);
    auto player_id = Player::Id(players_.GetNextPlayerId());
    Token player_token = RandomToken::Generate();
    players_.AddPlayer({player_id, player_token, session_ptr, dog_ptr});
    auto player_ptr = players_.FindByToken(player_token);

    return {player_token, player_ptr->GetId()};
}

GetPlayersListResult Application::GetPlayersList(Token token) {
    auto player = RequirePlayer(token);
    GetPlayersListResult result;
    for (const auto& session_player : players_.GetSessionPlayersList(*player->GetSession())) {
        result.players.push_back({session_player->GetId(), session_player->GetName()});
    }
    return result;
}

GetGameStateResult Application::GetGameState(Token token) {
    auto player = RequirePlayer(token);
    GetGameStateResult result;
    for (const auto& session_player : players_.GetSessionPlayersList(*player->GetSession())) {
        const auto dog = session_player->GetDog();
        result.players.push_back({session_player->GetId(), dog->GetPosition(), dog->GetSpeed(),
                                  dog->GetDirection(), dog->GetLootList(), dog->GetScore()});
    }
    result.lost_objects = player->GetSession()->GetLoots();
    return result;
}

void Application::DoAction(Token token, std::string direction) {
    auto player = RequirePlayer(token);

    if (!Dog::IsValidDirection(direction)) {
        throw ApplicationError(ApplicationErrorReason::InvalidDirection, ""s);
    }

    GameSession::DogPtr dog = player->GetDog();
    dog->SetDirection(direction);
}

void Application::ManualTick(int time_delta) {
    Tick(time_delta, ApplicationMode::Manual);
}

void Application::AutoTick(int time_delta) {
    Tick(time_delta, ApplicationMode::Auto);
}

void Application::Tick(int time_delta, ApplicationMode expected_mode) {
    if (mode_ != expected_mode)
        throw ApplicationError(ApplicationErrorReason::InvalidState, "");
    try {
        game_.UpdateState(time_delta);
    } catch (const model::InvalidTimeDelta& error) {
        throw ApplicationError(ApplicationErrorReason::InvalidTimeDelta, error.what());
    }
    if (on_update_)
        on_update_(time_delta);
}

GetGameRecordsResult Application::GetGameRecords(std::optional<int> start, std::optional<int> max_items) {
    const int max_items_th = 100;
    if ((max_items && (*max_items < 0 || *max_items > max_items_th)) || (start && *start < 0)) {
        throw ApplicationError(ApplicationErrorReason::InvalidMaxItems, ""s);
    }

    int offset = start.has_value() ? *start : 0;
    int limit = max_items.has_value() ? *max_items : max_items_th;

    return {players_repo_.GetRetiredPlayers(offset, limit)};
}

bool Application::IsValidToken(Token token) {
    if ((*token).size() != kTokenSize) {
        return false;
    }

    bool has_invalid_symbols =
        std::any_of((*token).begin(), (*token).end(), [](unsigned char ch) { return !std::isxdigit(ch); });
    return !has_invalid_symbols;
}

void Application::BindSession(GameSession& session) {
    session.SetDogRetirementHandler([&game = game_, &players = players_, &repository = players_repo_](
                                        GameSession::Id session_id, Dog::Id dog_id) {
        auto dog = game.FindSession(session_id)->FindDog(dog_id);
        domain::RetiredPlayer record{domain::RetiredPlayerId::New().ToString(), dog->GetName(),
                                     dog->GetScore(), dog->GetPlayTime()};
        repository.SaveRetiredPlayer(record);
        players.RemovePlayer(session_id, dog_id);
    });
}

Players::PlayerPtr Application::RequirePlayer(Token token) const {
    auto player = players_.FindByToken(token);
    if (!player) {
        throw ApplicationError(ApplicationErrorReason::InvalidToken, "");
    }
    return player;
}

} // namespace app