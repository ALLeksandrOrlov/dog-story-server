#pragma once
#include "application/app.h"
#include "controller/http_types.h"
#include "view/json_view.h"
namespace http_handler {
class ApiController {
  public:
    ApiController(app::Application& app, const view::JsonView& view) : app_(app), view_(view) {}
    ApiRequestResult Handle(const StringRequest& req, std::string_view uri);

  private:
    ApiRequestType GetReqType(std::string_view target);

    ApiRequestResult MapsListReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult MapDescReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult JoinGameReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult PlayersListReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult GameStateReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult ActionReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult TickReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult GameRecordsReqHandler(const StringRequest& req, std::string_view uri);
    ApiRequestResult InvalidReqHandler(const StringRequest& req, std::string_view uri);

    app::Application& app_;
    const view::JsonView& view_;
};
} // namespace http_handler
