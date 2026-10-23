#include <algorithm>
#include <cctype>
#include <limits>

#include "application/app.hpp"

namespace app
{
using namespace std::literals;
namespace
{
/** @brief Authorization token length in hexadecimal characters. */
constexpr int token_length = 32;
} // namespace

Application::Application(Game& game, Players& players, const ApplicationMode mode, const bool is_random_spawn_point,
        const std::function<void(int)>& on_update, domain::RetiredPlayerRepository& players_repo,
        const TokenGenerator& token_generator):
        m_game(game),
        m_players(players),
        m_mode(mode),
        m_is_random_spawn_point(is_random_spawn_point),
        m_on_update(on_update),
        m_players_repo(players_repo),
        m_token_generator(token_generator)
{
    if (!m_token_generator)
    {
        throw std::invalid_argument("Token generator must be provided");
    }
    for(const auto& session : m_game.GetSessions())
    {
        BindSession(*m_game.FindSession(session.GetId()));
    }
}

GetMapsResult Application::GetMaps() const
{
    GetMapsResult result;
    for(const auto& map : m_game.GetMaps())
    {
        result.maps.push_back({*map.GetId(), map.GetName()});
    }
    return result;
}

GetMapDescResult Application::GetMapDesc(const Map::Id& map_id) const
{
    if (m_game.FindMap(map_id) == nullptr)
    {
        throw ApplicationError(ApplicationErrorReason::InvalidMap, "");
    }
    return {map_id};
}

JoinGameResult Application::JoinGame(const Map::Id& map_id, const std::string_view user_name)
{
    if (user_name.empty() || user_name.find('\0') != std::string_view::npos)
    {
        throw ApplicationError(ApplicationErrorReason::InvalidName, "");
    }
    const auto* map = m_game.FindMap(map_id);
    if (!map)
    {
        throw ApplicationError(ApplicationErrorReason::InvalidMap, "");
    }
    if (m_players.GetNextPlayerId() >= std::numeric_limits<int>::max())
    {
        throw std::overflow_error("Player identifiers are exhausted");
    }
    const Token player_token = GenerateUniqueToken();
    auto* session = m_game.FindSession(map_id);
    if (!session)
    {
        const auto session_id = GameSession::Id(m_game.GetNextSessionId());
        auto new_session = GameSession(session_id, map, m_game.GetLootGeneratorConfig(), m_game.GetDogRetirementTime());
        BindSession(new_session);
        m_game.AddSession(std::move(new_session));
        session = m_game.FindSession(map_id);
    }
    const auto first_point = map->GetRoads().front().GetStart();
    const model::Position spawn_point = (m_is_random_spawn_point)
            ? map->GetRandomPoint()
            : model::Position{static_cast<double>(first_point.x), static_cast<double>(first_point.y)};
    const auto dog_id = Dog::Id(session->GetNextDogId());
    const auto player_id = Player::Id(m_players.GetNextPlayerId());
    session->AddDog(Dog(dog_id, std::string(user_name), spawn_point, map->GetSpeed(), "U", map->GetBagCapacity(), 0));
    try
    {
        m_players.AddPlayer({player_id, player_token, session, session->FindDog(dog_id)});
    }
    catch(...)
    {
        session->RemoveDog(dog_id);
        throw;
    }
    return {player_token, player_id};
}

GetPlayersListResult Application::GetPlayersList(const Token& token) const
{
    auto player = RequirePlayer(token);
    GetPlayersListResult result;
    for(const auto& session_player : m_players.GetSessionPlayersList(*player->GetSession()))
    {
        result.players.push_back({session_player->GetId(), session_player->GetName()});
    }
    return result;
}

GetGameStateResult Application::GetGameState(const Token& token) const
{
    auto player = RequirePlayer(token);
    GetGameStateResult result;
    for(const auto& session_player : m_players.GetSessionPlayersList(*player->GetSession()))
    {
        const auto dog = session_player->GetDog();
        result.players.push_back({session_player->GetId(), dog->GetPosition(), dog->GetSpeed(), dog->GetDirection(),
                dog->GetLootList(), dog->GetScore()});
    }
    result.lost_objects = player->GetSession()->GetLoots();
    return result;
}

void Application::DoAction(const Token& token, const std::string& direction)
{
    auto player = RequirePlayer(token);

    if (!Dog::IsValidDirection(direction))
    {
        throw ApplicationError(ApplicationErrorReason::InvalidDirection, ""s);
    }

    GameSession::DogPtr dog = player->GetDog();
    dog->SetDirection(direction);
}

void Application::ManualTick(const int time_delta)
{
    Tick(time_delta, ApplicationMode::Manual);
}

void Application::AutoTick(const int time_delta)
{
    Tick(time_delta, ApplicationMode::Auto);
}

void Application::Tick(const int time_delta, const ApplicationMode expected_mode)
{
    if (m_mode != expected_mode)
    {
        throw ApplicationError(ApplicationErrorReason::InvalidState, "");
    }
    try
    {
        m_game.UpdateState(time_delta);
    }
    catch(const model::InvalidTimeDelta& error)
    {
        throw ApplicationError(ApplicationErrorReason::InvalidTimeDelta, error.what());
    }
    if (m_on_update)
    {
        m_on_update(time_delta);
    }
}

GetGameRecordsResult Application::GetGameRecords(const std::optional<int> start, const std::optional<int> max_items)
{
    constexpr int max_items_th = 100;
    if ((max_items && (*max_items < 0 || *max_items > max_items_th)) || (start && *start < 0))
    {
        throw ApplicationError(ApplicationErrorReason::InvalidMaxItems, ""s);
    }

    const int offset = start.value_or(0);
    const int limit = max_items.value_or(max_items_th);

    return {m_players_repo.GetRetiredPlayers(offset, limit)};
}

bool Application::IsValidToken(const Token& token)
{
    if ((*token).size() != token_length)
    {
        return false;
    }

    bool has_invalid_symbols = std::any_of((*token).begin(), (*token).end(),
            [](unsigned char ch)
            {
                return !std::isxdigit(ch);
            });
    return !has_invalid_symbols;
}

void Application::BindSession(GameSession& session)
{
    session.SetDogRetirementHandler(
            [&game = m_game, &players = m_players, &repository = m_players_repo](
                    GameSession::Id session_id, Dog::Id dog_id)
            {
                auto dog = game.FindSession(session_id)->FindDog(dog_id);
                domain::RetiredPlayer record{
                        domain::RetiredPlayerId::New().ToString(), dog->GetName(), dog->GetScore(), dog->GetPlayTime()};
                repository.SaveRetiredPlayer(record);
                players.RemovePlayer(session_id, dog_id);
            });
}

Players::PlayerPtr Application::RequirePlayer(const Token& token) const
{
    auto player = m_players.FindByToken(token);
    if (!player)
    {
        throw ApplicationError(ApplicationErrorReason::InvalidToken, "");
    }
    return player;
}

Token Application::GenerateUniqueToken() const
{
    constexpr int maximum_attempts = 16;
    for(int attempt = 0; attempt < maximum_attempts; ++attempt)
    {
        const Token token = m_token_generator();
        if (!IsValidToken(token))
        {
            throw std::runtime_error("Token generator returned an invalid token");
        }
        if (!m_players.FindByToken(token))
        {
            return token;
        }
    }
    throw std::runtime_error("Token generator could not produce a unique token");
}

} // namespace app
