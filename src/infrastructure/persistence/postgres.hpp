#pragma once

#include <memory>
#include <string_view>

#include "domain/retired_player.hpp"

namespace postgres
{
/**
 * @brief Owns the PostgreSQL connection and the repository of completed games.
 * @details SQL and driver types are hidden in the implementation. All access is
 * sequential on the game strand. The repository remains valid until Database is destroyed.
 */
class Database
{
public:
    /** @brief Opens the connection and prepares the schema; propagates driver errors to the caller. */
    explicit Database(const std::string_view connection_url);
    /** @brief Closes the connection after destroying the repository. */
    ~Database();
    /** @brief The connection has a single owner. */
    Database(const Database&) = delete;
    /** @brief Disables moves so borrowed references retain the lifetime of their owner. */
    Database(Database&&) = delete;
    /** @brief Disables copying the connection. */
    Database& operator=(const Database&) = delete;
    /** @brief Disables moving the connection. */
    Database& operator=(Database&&) = delete;

    /** @brief Returns a borrowed result storage port without transferring ownership. */
    domain::RetiredPlayerRepository& GetRetiredPlayer();

private:
    /** @brief Hides the driver, connection, and SQL adapter from the public interface. */
    class Impl;
    /** @brief Sole owner of the hidden connection and its associated repository. */
    std::unique_ptr<Impl> m_impl;
};
} // namespace postgres
