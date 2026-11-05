#include <cstdlib>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <pqxx/pqxx>

#include "infrastructure/persistence/postgres.hpp"

namespace
{
/** @brief Deletes only the unique result created by this database integration test. */
class TestResult
{
public:
    /** @brief Records the isolated database and newly allocated result identity. */
    explicit TestResult(const std::string& url):
            m_url(url),
            m_id(domain::RetiredPlayerId::New().ToString())
    {
    }

    /** @brief Removes the test result without throwing during stack unwinding. */
    ~TestResult()
    {
        try
        {
            pqxx::connection connection(m_url);
            pqxx::work transaction(connection);
            transaction.exec_params("DELETE FROM retired_players WHERE id = $1", m_id);
            transaction.commit();
        }
        catch(...)
        {
        }
    }

    /** @brief Returns the test result UUID. */
    const std::string& GetId() const
    {
        return m_id;
    }

private:
    /** @brief Dedicated integration database connection string. */
    std::string m_url;
    /** @brief Identity that cannot collide with existing game results. */
    std::string m_id;
};
} // namespace

TEST_CASE("PostgreSQL accepts repeated result saves and retains the first committed values")
{
    const char* url = std::getenv("GAME_TEST_DB_URL");
    REQUIRE(url != nullptr);
    postgres::Database database(url);
    TestResult result(url);
    auto& repository = database.GetRetiredPlayer();
    const domain::RetiredPlayer first{result.GetId(), "Retry test", 10, 1000};
    repository.SaveRetiredPlayer(first);
    REQUIRE_NOTHROW(repository.SaveRetiredPlayer(first));
    REQUIRE_NOTHROW(repository.SaveRetiredPlayer({result.GetId(), "Replayed state", 20, 2000}));

    pqxx::connection connection(url);
    pqxx::read_transaction transaction(connection);
    const auto rows = transaction.exec_params(
            "SELECT name, score, play_time_ms FROM retired_players WHERE id = $1", result.GetId());
    REQUIRE(rows.size() == 1);
    REQUIRE(rows.front()[0].as<std::string>() == first.name);
    REQUIRE(rows.front()[1].as<int>() == first.score);
    REQUIRE(rows.front()[2].as<int>() == first.play_time_ms);
}
