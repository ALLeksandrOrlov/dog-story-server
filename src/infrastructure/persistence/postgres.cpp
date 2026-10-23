#include <stdexcept>
#include <string>
#include <vector>

#include <pqxx/pqxx>

#include "infrastructure/persistence/postgres.hpp"

namespace postgres
{
namespace
{
/** @brief Implements the leaderboard port using a borrowed PostgreSQL connection accessed sequentially. */
class PostgresRetiredPlayerRepository final : public domain::RetiredPlayerRepository
{
public:
    /** @brief Borrows a connection that must outlive the repository. */
    explicit PostgresRetiredPlayerRepository(pqxx::connection& connection):
            m_connection(connection)
    {
    }

    /** @brief Saves one result in an atomic SQL transaction. */
    void SaveRetiredPlayer(const domain::RetiredPlayer& player) override
    {
        pqxx::work transaction(m_connection);
        transaction.exec_prepared("insert_player", player.id, player.name, player.score, player.play_time_ms);
        transaction.commit();
    }

    /** @brief Reads a page in leaderboard order; rejects negative pagination bounds. */
    std::vector<domain::RetiredPlayer> GetRetiredPlayers(const int offset, const int limit) override
    {
        if (offset < 0 || limit < 0)
        {
            throw std::invalid_argument("Record pagination must be nonnegative");
        }
        pqxx::read_transaction transaction(m_connection);
        const auto rows = transaction.exec_params("SELECT id, name, score, play_time_ms FROM retired_players "
                                                  "ORDER BY score DESC, play_time_ms ASC, name ASC LIMIT $1 OFFSET $2",
                limit, offset);
        std::vector<domain::RetiredPlayer> players;
        players.reserve(rows.size());
        for(const auto& row : rows)
        {
            players.emplace_back(domain::RetiredPlayer{
                    row[0].as<std::string>(), row[1].as<std::string>(), row[2].as<int>(), row[3].as<int>()});
        }
        return players;
    }

private:
    /** @brief Borrowed connection accessed sequentially; owned by Database::Impl. */
    pqxx::connection& m_connection;
};
} // namespace

/** @brief Declares the connection before the repository to ensure correct destruction order. */
class Database::Impl
{
public:
    /** @brief Creates the schema and prepared statement before exposing the repository. */
    explicit Impl(const std::string_view connection_url):
            m_connection(std::string(connection_url)),
            m_repository(m_connection)
    {
        pqxx::work transaction(m_connection);
        transaction.exec("CREATE TABLE IF NOT EXISTS retired_players ("
                         "id UUID PRIMARY KEY, name TEXT NOT NULL, score INT NOT NULL, play_time_ms INT NOT NULL)");
        // The API accepts unlimited name lengths; widening the column preserves existing records.
        transaction.exec("ALTER TABLE retired_players ALTER COLUMN name TYPE TEXT");
        // Long names may exceed B-tree limits; ORDER BY still applies the final name ordering.
        transaction.exec("DROP INDEX IF EXISTS idx_score_playtime_name");
        transaction.exec("CREATE INDEX IF NOT EXISTS idx_score_playtime "
                         "ON retired_players (score DESC, play_time_ms ASC)");
        transaction.commit();
        m_connection.prepare(
                "insert_player", "INSERT INTO retired_players (id, name, score, play_time_ms) VALUES ($1, $2, $3, $4)");
    }

    /** @brief Returns an adapter with the same lifetime as the Database implementation. */
    domain::RetiredPlayerRepository& GetRepository()
    {
        return m_repository;
    }

private:
    /** @brief Owned PostgreSQL connection declared before its dependent adapter. */
    pqxx::connection m_connection;
    /** @brief SQL adapter using this implementation's connection. */
    PostgresRetiredPlayerRepository m_repository;
};

Database::Database(const std::string_view connection_url):
        m_impl(std::make_unique<Impl>(connection_url))
{
}

Database::~Database() = default;

domain::RetiredPlayerRepository& Database::GetRetiredPlayer()
{
    return m_impl->GetRepository();
}
} // namespace postgres
