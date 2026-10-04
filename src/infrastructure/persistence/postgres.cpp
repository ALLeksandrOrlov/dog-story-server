#include "infrastructure/persistence/postgres.h"

namespace postgres {

using pqxx::operator""_zv;
using namespace domain;

void RetiredPlayerRepositoryImpl::SaveRetiredPlayer(const RetiredPlayer& player) {
    pqxx::work work(connection_);
    work.exec_prepared("insert_player", player.id, player.name, player.score, player.play_time_ms);
    work.commit();
}

std::vector<RetiredPlayer> RetiredPlayerRepositoryImpl::GetRetiredPlayers(int offset, int limit) {
    pqxx::work work(connection_);
    pqxx::result res = work.exec("SELECT id, name, score, play_time_ms "
                                 "FROM retired_players "
                                 "ORDER BY score DESC, play_time_ms ASC, name ASC "
                                 "LIMIT " +
                                 std::to_string(limit) + " OFFSET " + std::to_string(offset));

    std::vector<RetiredPlayer> players;
    for (const auto& row : res) {
        RetiredPlayer player;
        player.id = row[0].as<std::string>();
        player.name = row[1].as<std::string>();
        player.score = row[2].as<int>();
        player.play_time_ms = row[3].as<int>();
        players.push_back(player);
    }

    return players;
}

Database::Database(pqxx::connection connection) : connection_(std::move(connection)) {
    pqxx::work work{connection_};
    work.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
            id UUID PRIMARY KEY,
            name VARCHAR(255) NOT NULL,
            score INT NOT NULL,
            play_time_ms INT NOT NULL);
            )"_zv);
    work.exec(
        R"(CREATE INDEX IF NOT EXISTS idx_score_playtime_name ON retired_players (score DESC, play_time_ms ASC, name ASC);)"_zv);
    work.commit();
    connection_.prepare("insert_player", R"(
            INSERT INTO retired_players (id, name, score, play_time_ms)
            VALUES ($1, $2, $3, $4)
        )");
}

RetiredPlayerRepositoryImpl& Database::GetRetiredPlayer() {
    return retired_player_;
}

} // namespace postgres