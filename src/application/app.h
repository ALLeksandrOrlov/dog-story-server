#pragma once

#include "application/players.h"
#include "domain/retired_player.h"
#include <exception>
#include <functional>
#include <optional>
#include <string_view>

namespace app {
using model::Game;
using model::Map;

enum class ApplicationMode { Manual, Auto };

enum class ApplicationErrorReason {
    InvalidName,
    InvalidMap,
    InvalidToken,
    InvalidDirection,
    InvalidState,
    InvalidTimeDelta,
    InvalidMaxItems
};

class ApplicationError : public std::exception {
  public:
    ApplicationError(ApplicationErrorReason reason, const std::string& message)
        : reason_(reason), message_(message) {}

    const char* what() const noexcept override {

        return message_.c_str();
    }

    ApplicationErrorReason reason() const {

        return reason_;
    }

  private:
    ApplicationErrorReason reason_;
    std::string message_;
};

struct MapSummary {
    std::string id;
    std::string name;
};
struct GetMapsResult {
    std::vector<MapSummary> maps;
};
struct GetMapDescResult {
    model::Map::Id id{""};
};
struct JoinGameResult {
    Token token{""};
    Player::Id player_id{0};
};
struct PlayerSummary {
    Player::Id id{0};
    std::string name;
};
struct GetPlayersListResult {
    std::vector<PlayerSummary> players;
};
struct PlayerState {
    Player::Id id{0};
    model::Position position;
    geom::Vec2D speed;
    std::string direction;
    std::vector<model::Loot> bag;
    int score;
};
struct GetGameStateResult {
    std::vector<PlayerState> players;
    model::GameSession::Loots lost_objects;
};
struct GetGameRecordsResult {
    std::vector<domain::RetiredPlayer> records;
};

class Application {
  public:
    Application(Game& game, Players& players, ApplicationMode mode, bool is_random_spawn_point,
                std::function<void(int)> on_update, domain::RetiredPlayerRepository& players_repo);

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    GetMapsResult GetMaps();

    GetMapDescResult GetMapDesc(Map::Id map_id);

    JoinGameResult JoinGame(Map::Id map_id, std::string_view user_name);

    GetPlayersListResult GetPlayersList(Token token);

    GetGameStateResult GetGameState(Token token);

    void DoAction(Token token, std::string direction);

    void ManualTick(int time_delta);

    void AutoTick(int time_delta);

    GetGameRecordsResult GetGameRecords(std::optional<int> start, std::optional<int> max_items);

    static bool IsValidToken(Token token);

  private:
    void Tick(int time_delta, ApplicationMode expected_mode);

    Players::PlayerPtr RequirePlayer(Token token) const;

    void BindSession(GameSession& session);

    Game& game_;
    Players& players_;
    ApplicationMode mode_ = ApplicationMode::Manual;
    bool is_random_spawn_point_ = false;
    std::function<void(int)> on_update_;
    domain::RetiredPlayerRepository& players_repo_;
};

} // namespace app