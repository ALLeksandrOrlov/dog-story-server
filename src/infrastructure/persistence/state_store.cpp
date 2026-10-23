#include <fstream>
#include <type_traits>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "infrastructure/persistence/app_serialization.hpp"
#include "infrastructure/persistence/model_serialization.hpp"

#include "infrastructure/persistence/state_store.hpp"

namespace persistence
{
StateStore::StateStore(std::filesystem::path path, const std::optional<int> period_ms):
        m_path(std::move(path)),
        m_period_ms(period_ms)
{
    if (period_ms && *period_ms <= 0)
    {
        throw std::invalid_argument("Snapshot period must be positive");
    }
}
void StateStore::Restore(model::Game& game, app::Players& players) const
{
    if (m_path.empty() || !std::filesystem::exists(m_path))
    {
        return;
    }
    std::ifstream input(m_path);
    boost::archive::text_iarchive archive(input);
    std::vector<serialization::GameSessionRepr> sessions;
    serialization::PlayersRepr restored_players;
    archive >> sessions >> restored_players;
    if (!game.GetSessions().empty() || !players.GetPlayersList().empty())
    {
        throw std::logic_error("Restore requires a game without active sessions");
    }
    model::Game staged_game;
    for(const auto& map : game.GetMaps())
    {
        staged_game.AddMap(map);
    }
    staged_game.SetLootGeneratorConfig(game.GetLootGeneratorConfig());
    staged_game.SetDogRetirementTime(game.GetDogRetirementTime());
    for(const auto& session : sessions)
    {
        staged_game.AddSession(session.Restore(staged_game));
    }
    auto staged_players = restored_players.Restore(staged_game);
    static_assert(std::is_nothrow_move_assignable_v<model::Game>);
    static_assert(std::is_nothrow_move_assignable_v<app::Players>);
    game = std::move(staged_game);
    players = std::move(staged_players);
}
void StateStore::Save(const model::Game& game, const app::Players& players) const
{
    if (m_path.empty())
    {
        return;
    }
    auto temporary = m_path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        boost::archive::text_oarchive archive(output);
        std::vector<serialization::GameSessionRepr> sessions;
        for(const auto& session : game.GetSessions())
        {
            sessions.emplace_back(session);
        }
        serialization::PlayersRepr saved_players(players);
        archive << sessions << saved_players;
        output.flush();
    }
    std::filesystem::rename(temporary, m_path);
}
void StateStore::OnTick(const int delta_ms, const model::Game& game, const app::Players& players)
{
    if (m_path.empty() || !m_period_ms)
    {
        return;
    }
    if (delta_ms < 0)
    {
        throw std::invalid_argument("Snapshot time delta must be nonnegative");
    }
    m_elapsed += std::chrono::milliseconds(delta_ms);
    // Save the snapshot once the configured interval is exceeded.
    if ((m_elapsed - m_last_save).count() > *m_period_ms)
    {
        Save(game, players);
        m_last_save = m_elapsed;
    }
}
} // namespace persistence
