#include "controller/api_controller.h"
#include "common/string_processing.h"
#include <limits>

namespace http_handler {
using namespace app;
namespace {
StringResponse MakeResponse(const StringRequest& req, http::status status, std::string body,
                            std::string_view allow = {}, bool no_cache = true) {
    StringResponse response{status, req.version()};
    response.set(http::field::content_type, "application/json");
    if (no_cache)
        response.set(http::field::cache_control, "no-cache");
    if (!allow.empty())
        response.set(http::field::allow, allow);
    response.body() = std::move(body);
    response.prepare_payload();
    return response;
}
StringResponse Error(const StringRequest& req, http::status status, std::string_view code,
                     std::string_view message, std::string_view allow = {}, bool no_cache = true) {
    return MakeResponse(req, status, json::serialize(json::object{{"code", code}, {"message", message}}),
                        allow, no_cache);
}
StringResponse InvalidMethod(const StringRequest& req, std::string_view allow,
                             std::string_view message = "Invalid method") {
    return Error(req, http::status::method_not_allowed, "invalidMethod", message, allow);
}
StringResponse UnknownToken(const StringRequest& req) {
    return Error(req, http::status::unauthorized, "unknownToken", "Player token has not been found");
}
std::optional<Token> ReadToken(const StringRequest& req) {
    auto value = req.find(http::field::authorization);
    if (value == req.end())
        return std::nullopt;
    auto parts = Split(value->value(), ' ');
    if (parts.size() != 2 || parts[0] != "Bearer")
        return std::nullopt;
    Token token{std::string(parts[1])};
    if (!Application::IsValidToken(token))
        return std::nullopt;
    return token;
}
bool HasJsonContentType(const StringRequest& req) {
    auto value = req.find(http::field::content_type);
    return value != req.end() && value->value() == "application/json";
}
bool IsReadMethod(const StringRequest& req) {
    return req.method() == http::verb::get || req.method() == http::verb::head;
}
} // namespace

ApiRequestResult ApiController::Handle(const StringRequest& req, std::string_view uri) {
    switch (GetReqType(uri)) {
    case ApiRequestType::MapsList:
        return MapsListReqHandler(req, uri);
    case ApiRequestType::MapDescription:
        return MapDescReqHandler(req, uri);
    case ApiRequestType::JoinGame:
        return JoinGameReqHandler(req, uri);
    case ApiRequestType::PlayersList:
        return PlayersListReqHandler(req, uri);
    case ApiRequestType::GameState:
        return GameStateReqHandler(req, uri);
    case ApiRequestType::Action:
        return ActionReqHandler(req, uri);
    case ApiRequestType::Tick:
        return TickReqHandler(req, uri);
    case ApiRequestType::GameRecords:
        return GameRecordsReqHandler(req, uri);
    default:
        return InvalidReqHandler(req, uri);
    }
}

ApiRequestType ApiController::GetReqType(std::string_view target) {
    const auto parts = Split(target, '/');
    if (parts.size() < 3 || parts[0] != "api" || parts[1] != "v1")
        return ApiRequestType::Invalid;
    if (parts[2] == "maps") {
        if (parts.size() == 3)
            return ApiRequestType::MapsList;
        if (parts.size() == 4)
            return ApiRequestType::MapDescription;
    }
    if (parts[2] != "game")
        return ApiRequestType::Invalid;
    if (parts.size() == 5 && parts[3] == "player" && parts[4] == "action")
        return ApiRequestType::Action;
    if (parts.size() != 4)
        return ApiRequestType::Invalid;
    if (parts[3] == "join")
        return ApiRequestType::JoinGame;
    if (parts[3] == "players")
        return ApiRequestType::PlayersList;
    if (parts[3] == "state")
        return ApiRequestType::GameState;
    if (parts[3] == "tick")
        return ApiRequestType::Tick;
    if (parts[3] == "records" || parts[3].starts_with("records?"))
        return ApiRequestType::GameRecords;
    return ApiRequestType::Invalid;
}

ApiRequestResult ApiController::MapsListReqHandler(const StringRequest& req, std::string_view) {
    if (req.method() != http::verb::get) {
        // Preserve this endpoint's original plain-text error body and GET-only contract.
        return MakeResponse(req, http::status::method_not_allowed, "Invalid method", "GET");
    }
    return MakeResponse(req, http::status::ok, json::serialize(view_.Render(app_.GetMaps())));
}

ApiRequestResult ApiController::MapDescReqHandler(const StringRequest& req, std::string_view uri) {
    if (!IsReadMethod(req))
        return InvalidMethod(req, "GET, HEAD");
    try {
        const auto parts = Split(uri, '/');
        auto result = app_.GetMapDesc(model::Map::Id(std::string(parts.back())));
        return MakeResponse(req, http::status::ok, json::serialize(view_.Render(result)));
    } catch (const ApplicationError& e) {
        if (e.reason() != ApplicationErrorReason::InvalidMap)
            throw;
        return Error(req, http::status::not_found, "mapNotFound", "Map not found");
    }
}

ApiRequestResult ApiController::JoinGameReqHandler(const StringRequest& req, std::string_view) {
    if (req.method() != http::verb::post)
        return InvalidMethod(req, "POST", "Only POST method is expected");
    model::Map::Id map_id{""};
    std::string user_name;
    try {
        const auto body = json::parse(req.body()).as_object();
        *map_id = body.at("mapId").as_string();
        user_name = body.at("userName").as_string();
    } catch (const std::exception&) {
        return Error(req, http::status::bad_request, "invalidArgument", "Join game request parse error");
    }
    try {
        return MakeResponse(req, http::status::ok,
                            json::serialize(view_.Render(app_.JoinGame(map_id, user_name))));
    } catch (const ApplicationError& e) {
        if (e.reason() == ApplicationErrorReason::InvalidName) {
            return Error(req, http::status::bad_request, "invalidArgument", "Invalid name");
        }
        if (e.reason() == ApplicationErrorReason::InvalidMap) {
            return Error(req, http::status::not_found, "mapNotFound", "Map not found");
        }
        throw;
    }
}

ApiRequestResult ApiController::PlayersListReqHandler(const StringRequest& req, std::string_view) {
    if (!IsReadMethod(req))
        return InvalidMethod(req, "GET, HEAD");
    const auto token = ReadToken(req);
    if (!token)
        return Error(req, http::status::unauthorized, "invalidToken", "Authorization header is missing");
    try {
        return MakeResponse(req, http::status::ok,
                            json::serialize(view_.Render(app_.GetPlayersList(*token))));
    } catch (const ApplicationError& e) {
        if (e.reason() != ApplicationErrorReason::InvalidToken)
            throw;
        return UnknownToken(req);
    }
}

ApiRequestResult ApiController::GameStateReqHandler(const StringRequest& req, std::string_view) {
    if (!IsReadMethod(req))
        return InvalidMethod(req, "GET, HEAD");
    const auto token = ReadToken(req);
    if (!token)
        return Error(req, http::status::unauthorized, "invalidToken", "Authorization header is required");
    try {
        return MakeResponse(req, http::status::ok, json::serialize(view_.Render(app_.GetGameState(*token))));
    } catch (const ApplicationError& e) {
        if (e.reason() != ApplicationErrorReason::InvalidToken)
            throw;
        return UnknownToken(req);
    }
}

ApiRequestResult ApiController::ActionReqHandler(const StringRequest& req, std::string_view) {
    if (req.method() != http::verb::post)
        return InvalidMethod(req, "POST");
    if (!HasJsonContentType(req))
        return Error(req, http::status::unauthorized, "invalidToken", "Invalid content type");
    const auto token = ReadToken(req);
    if (!token)
        return Error(req, http::status::unauthorized, "invalidToken", "Authorization header is required");
    std::string direction;
    try {
        direction = json::parse(req.body()).as_object().at("move").as_string();
    } catch (const std::exception&) {
        return Error(req, http::status::bad_request, "invalidArgument", "Failed to parse action");
    }
    try {
        app_.DoAction(*token, direction);
    } catch (const ApplicationError& e) {
        if (e.reason() == ApplicationErrorReason::InvalidToken)
            return UnknownToken(req);
        if (e.reason() == ApplicationErrorReason::InvalidDirection) {
            return Error(req, http::status::bad_request, "invalidArgument", "Failed to parse action");
        }
        throw;
    }
    return MakeResponse(req, http::status::ok, "{}");
}

ApiRequestResult ApiController::TickReqHandler(const StringRequest& req, std::string_view uri) {
    if (req.method() != http::verb::post)
        return InvalidMethod(req, "POST");
    if (!HasJsonContentType(req))
        return Error(req, http::status::unauthorized, "invalidToken", "Invalid content type");
    int delta;
    try {
        const auto value = json::parse(req.body()).as_object().at("timeDelta").as_int64();
        if (value < 0 || value > std::numeric_limits<int>::max()) {
            throw std::invalid_argument("Invalid time delta");
        }
        delta = static_cast<int>(value);
    } catch (const std::exception&) {
        return Error(req, http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON");
    }
    try {
        app_.ManualTick(delta);
    } catch (const ApplicationError& e) {
        if (e.reason() == ApplicationErrorReason::InvalidTimeDelta) {
            return Error(req, http::status::bad_request, "invalidArgument", "Invalid time delta");
        }
        if (e.reason() != ApplicationErrorReason::InvalidState)
            throw;
        return InvalidReqHandler(req, uri);
    }
    return MakeResponse(req, http::status::ok, "{}");
}

ApiRequestResult ApiController::GameRecordsReqHandler(const StringRequest& req, std::string_view uri) {
    if (req.method() != http::verb::get)
        return InvalidMethod(req, "GET, HEAD");
    std::optional<int> start;
    std::optional<int> max_items;
    const auto path = Split(uri, '/');
    const auto query = Split(path.at(3), '?');
    if (query.size() == 2) {
        for (auto item : Split(query[1], '&')) {
            const auto pair = Split(item, '=');
            if (pair.size() != 2)
                continue;
            try {
                if (pair[0] == "start")
                    start = std::stoi(std::string(pair[1]));
                else if (pair[0] == "maxItems")
                    max_items = std::stoi(std::string(pair[1]));
            } catch (const std::exception&) {
                // Existing clients get default pagination for unparseable values.
            }
        }
    }
    try {
        return MakeResponse(req, http::status::ok,
                            json::serialize(view_.Render(app_.GetGameRecords(start, max_items))));
    } catch (const ApplicationError& e) {
        if (e.reason() != ApplicationErrorReason::InvalidMaxItems)
            throw;
        return InvalidReqHandler(req, uri);
    }
}

ApiRequestResult ApiController::InvalidReqHandler(const StringRequest& req, std::string_view) {
    return Error(req, http::status::bad_request, "badRequest", "Bad request", {}, false);
}
} // namespace http_handler
