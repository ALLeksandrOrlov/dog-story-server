#include "infrastructure/persistence/state_store.h"
#include "infrastructure/persistence/app_serialization.h"
#include "infrastructure/persistence/model_serialization.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <fstream>
#include <type_traits>

namespace persistence {
StateStore::StateStore(std::filesystem::path path, std::optional<int> period_ms)
    : path_(std::move(path)), period_ms_(period_ms) {}
void StateStore::Restore(model::Game& game, app::Players& players) const {
    if (path_.empty() || !std::filesystem::exists(path_))
        return;
    std::ifstream input(path_);
    boost::archive::text_iarchive archive(input);
    std::vector<serialization::GameSessionRepr> sessions;
    serialization::PlayersRepr restored_players;
    archive >> sessions >> restored_players;
    if (!game.GetSessions().empty() || !players.GetPlayersList().empty())
        throw std::logic_error("Restore requires a game without active sessions");
    model::Game staged_game;
    for (const auto& map : game.GetMaps())
        staged_game.AddMap(map);
    staged_game.SetLootGeneratorConfig(game.GetLootGeneratorConfig());
    staged_game.SetDogRetirementTime(game.GetDogRetirementTime());
    for (const auto& session : sessions)
        staged_game.AddSession(session.Restore(staged_game));
    auto staged_players = restored_players.Restore(staged_game);
    static_assert(std::is_nothrow_move_assignable_v<model::Game>);
    static_assert(std::is_nothrow_move_assignable_v<app::Players>);
    game = std::move(staged_game);
    players = std::move(staged_players);
}
void StateStore::Save(const model::Game& game, const app::Players& players) const {
    if (path_.empty())
        return;
    auto temporary = path_;
    temporary += ".tmp";
    {
        std::ofstream output(temporary);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        boost::archive::text_oarchive archive(output);
        std::vector<serialization::GameSessionRepr> sessions;
        for (const auto& session : game.GetSessions())
            sessions.emplace_back(session);
        serialization::PlayersRepr saved_players(players);
        archive << sessions << saved_players;
        output.flush();
    }
    std::filesystem::rename(temporary, path_);
}
void StateStore::OnTick(int delta_ms, model::Game& game, const app::Players& players) {
    if (path_.empty() || !period_ms_)
        return;
    elapsed_ += std::chrono::milliseconds(delta_ms);
    // Preserve the original strict threshold and game-time-based scheduling.
    if ((elapsed_ - last_save_).count() > *period_ms_) {
        Save(game, players);
        last_save_ = elapsed_;
    }
}
} // namespace persistence
