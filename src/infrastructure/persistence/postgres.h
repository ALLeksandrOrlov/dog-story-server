#pragma once

#include <pqxx/connection>
#include <pqxx/pqxx>
#include <pqxx/transaction>
#include <vector>

#include "domain/retired_player.h"

namespace postgres {

class RetiredPlayerRepositoryImpl : public domain::RetiredPlayerRepository {
  public:
    explicit RetiredPlayerRepositoryImpl(pqxx::connection& connection) : connection_{connection} {}

    void SaveRetiredPlayer(const domain::RetiredPlayer& player) override;

    std::vector<domain::RetiredPlayer> GetRetiredPlayers(int offset, int limit) override;

  private:
    pqxx::connection& connection_;
};

class Database {
  public:
    explicit Database(pqxx::connection connection);

    RetiredPlayerRepositoryImpl& GetRetiredPlayer();

  private:
    pqxx::connection connection_;
    RetiredPlayerRepositoryImpl retired_player_{connection_};
};

} // namespace postgres