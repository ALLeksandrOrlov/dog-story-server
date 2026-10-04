#pragma once
#include "application/app.h"
#include <boost/json.hpp>

namespace view {
class JsonView {
  public:
    explicit JsonView(const boost::json::value& config);
    boost::json::array Render(const app::GetMapsResult& result) const;
    boost::json::object Render(const app::GetMapDescResult& result) const;
    boost::json::object Render(const app::JoinGameResult& result) const;
    boost::json::object Render(const app::GetPlayersListResult& result) const;
    boost::json::object Render(const app::GetGameStateResult& result) const;
    boost::json::array Render(const app::GetGameRecordsResult& result) const;

  private:
    std::unordered_map<std::string, boost::json::object> map_descriptions_;
};
} // namespace view
