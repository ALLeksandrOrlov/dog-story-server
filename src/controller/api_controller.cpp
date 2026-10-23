#include <limits>

#include "common/string_processing.hpp"

#include "controller/api_controller.hpp"

namespace http_handler
{
using namespace app;
namespace
{
/** @brief Builds a JSON response with an explicit body length. */
StringResponse MakeResponse(const StringRequest& request, const http::status status, std::string body,
        const std::string_view allow = {}, const bool no_cache = true)
{
    StringResponse response{status, request.version()};
    response.set(http::field::content_type, "application/json");
    if (no_cache)
    {
        response.set(http::field::cache_control, "no-cache");
    }
    if (!allow.empty())
    {
        response.set(http::field::allow, allow);
    }
    response.body() = std::move(body);
    response.prepare_payload();
    return response;
}
/** @brief Serializes the stable error fields code and message. */
StringResponse MakeErrorResponse(const StringRequest& request, const http::status status, const std::string_view code,
        const std::string_view message, const std::string_view allow = {}, const bool no_cache = true)
{
    return MakeResponse(
            request, status, json::serialize(json::object{{"code", code}, {"message", message}}), allow, no_cache);
}
/** @brief Returns HTTP 405 and the allowed methods. */
StringResponse MakeInvalidMethodResponse(
        const StringRequest& request, const std::string_view allow, const std::string_view message = "Invalid method")
{
    return MakeErrorResponse(request, http::status::method_not_allowed, "invalidMethod", message, allow);
}
/** @brief Returns HTTP 401 for a syntactically valid but unknown token. */
StringResponse MakeUnknownTokenResponse(const StringRequest& request)
{
    return MakeErrorResponse(request, http::status::unauthorized, "unknownToken", "Player token has not been found");
}
/** @brief Parses the Bearer header; a missing or malformed value yields nullopt. */
std::optional<Token> ReadToken(const StringRequest& request)
{
    const auto value = request.find(http::field::authorization);
    if (value == request.end())
    {
        return std::nullopt;
    }
    const auto parts = util::Split(value->value(), ' ');
    if (parts.size() != 2 || parts[0] != "Bearer")
    {
        return std::nullopt;
    }
    Token token{std::string(parts[1])};
    if (!Application::IsValidToken(token))
    {
        return std::nullopt;
    }
    return token;
}
/** @brief Checks the exact Content-Type required by the contract. */
bool HasJsonContentType(const StringRequest& request)
{
    const auto value = request.find(http::field::content_type);
    return value != request.end() && value->value() == "application/json";
}
/** @brief Checks whether the read method is allowed for the route. */
bool IsReadMethod(const StringRequest& request)
{
    return request.method() == http::verb::get || request.method() == http::verb::head;
}
} // namespace

ApiRequestResult ApiController::Handle(const StringRequest& request, const std::string_view uri) const
{
    ApiRequestResult response;
    switch(ClassifyRequest(uri))
    {
        case ApiRequestType::MapsList:
            response = HandleMapsList(request, uri);
            break;
        case ApiRequestType::MapDescription:
            response = HandleMapDescription(request, uri);
            break;
        case ApiRequestType::JoinGame:
            response = HandleJoinGame(request, uri);
            break;
        case ApiRequestType::PlayersList:
            response = HandlePlayersList(request, uri);
            break;
        case ApiRequestType::GameState:
            response = HandleGameState(request, uri);
            break;
        case ApiRequestType::Action:
            response = HandleAction(request, uri);
            break;
        case ApiRequestType::Tick:
            response = HandleTick(request, uri);
            break;
        case ApiRequestType::GameRecords:
            response = HandleGameRecords(request, uri);
            break;
        default:
            response = HandleInvalidRequest(request, uri);
            break;
    }
    return response;
}

ApiRequestType ApiController::ClassifyRequest(const std::string_view target)
{
    const auto parts = util::Split(target, '/');
    if (parts.size() < 3 || parts[0] != "api" || parts[1] != "v1")
    {
        return ApiRequestType::Invalid;
    }
    if (parts[2] == "maps")
    {
        if (parts.size() == 3)
        {
            return ApiRequestType::MapsList;
        }
        if (parts.size() == 4)
        {
            return ApiRequestType::MapDescription;
        }
    }
    if (parts[2] != "game")
    {
        return ApiRequestType::Invalid;
    }
    if (parts.size() == 5 && parts[3] == "player" && parts[4] == "action")
    {
        return ApiRequestType::Action;
    }
    if (parts.size() != 4)
    {
        return ApiRequestType::Invalid;
    }
    if (parts[3] == "join")
    {
        return ApiRequestType::JoinGame;
    }
    if (parts[3] == "players")
    {
        return ApiRequestType::PlayersList;
    }
    if (parts[3] == "state")
    {
        return ApiRequestType::GameState;
    }
    if (parts[3] == "tick")
    {
        return ApiRequestType::Tick;
    }
    if (parts[3] == "records" || parts[3].starts_with("records?"))
    {
        return ApiRequestType::GameRecords;
    }
    return ApiRequestType::Invalid;
}

ApiRequestResult ApiController::HandleMapsList(const StringRequest& request, std::string_view) const
{
    if (request.method() != http::verb::get)
    {
        // This endpoint returns a plain-text error body and GET-only contract.
        return MakeResponse(request, http::status::method_not_allowed, "Invalid method", "GET");
    }
    return MakeResponse(request, http::status::ok, json::serialize(m_view.Render(m_application.GetMaps())));
}

ApiRequestResult ApiController::HandleMapDescription(const StringRequest& request, const std::string_view uri) const
{
    if (!IsReadMethod(request))
    {
        return MakeInvalidMethodResponse(request, "GET, HEAD");
    }
    try
    {
        const auto parts = util::Split(uri, '/');
        const auto result = m_application.GetMapDesc(model::Map::Id(std::string(parts.back())));
        return MakeResponse(request, http::status::ok, json::serialize(m_view.Render(result)));
    }
    catch(const ApplicationError& e)
    {
        if (e.GetReason() != ApplicationErrorReason::InvalidMap)
        {
            throw;
        }
        return MakeErrorResponse(request, http::status::not_found, "mapNotFound", "Map not found");
    }
}

ApiRequestResult ApiController::HandleJoinGame(const StringRequest& request, std::string_view) const
{
    if (request.method() != http::verb::post)
    {
        return MakeInvalidMethodResponse(request, "POST", "Only POST method is expected");
    }
    model::Map::Id map_id{""};
    std::string user_name;
    try
    {
        const auto body = json::parse(request.body()).as_object();
        *map_id = body.at("mapId").as_string();
        user_name = body.at("userName").as_string();
    }
    catch(const std::exception&)
    {
        return MakeErrorResponse(
                request, http::status::bad_request, "invalidArgument", "Join game request parse error");
    }
    try
    {
        return MakeResponse(
                request, http::status::ok, json::serialize(m_view.Render(m_application.JoinGame(map_id, user_name))));
    }
    catch(const ApplicationError& e)
    {
        if (e.GetReason() == ApplicationErrorReason::InvalidName)
        {
            return MakeErrorResponse(request, http::status::bad_request, "invalidArgument", "Invalid name");
        }
        if (e.GetReason() == ApplicationErrorReason::InvalidMap)
        {
            return MakeErrorResponse(request, http::status::not_found, "mapNotFound", "Map not found");
        }
        throw;
    }
}

ApiRequestResult ApiController::HandlePlayersList(const StringRequest& request, std::string_view) const
{
    if (!IsReadMethod(request))
    {
        return MakeInvalidMethodResponse(request, "GET, HEAD");
    }
    const auto token = ReadToken(request);
    if (!token)
    {
        return MakeErrorResponse(
                request, http::status::unauthorized, "invalidToken", "Authorization header is missing");
    }
    try
    {
        return MakeResponse(
                request, http::status::ok, json::serialize(m_view.Render(m_application.GetPlayersList(*token))));
    }
    catch(const ApplicationError& e)
    {
        if (e.GetReason() != ApplicationErrorReason::InvalidToken)
        {
            throw;
        }
        return MakeUnknownTokenResponse(request);
    }
}

ApiRequestResult ApiController::HandleGameState(const StringRequest& request, std::string_view) const
{
    if (!IsReadMethod(request))
    {
        return MakeInvalidMethodResponse(request, "GET, HEAD");
    }
    const auto token = ReadToken(request);
    if (!token)
    {
        return MakeErrorResponse(
                request, http::status::unauthorized, "invalidToken", "Authorization header is required");
    }
    try
    {
        return MakeResponse(
                request, http::status::ok, json::serialize(m_view.Render(m_application.GetGameState(*token))));
    }
    catch(const ApplicationError& e)
    {
        if (e.GetReason() != ApplicationErrorReason::InvalidToken)
        {
            throw;
        }
        return MakeUnknownTokenResponse(request);
    }
}

ApiRequestResult ApiController::HandleAction(const StringRequest& request, std::string_view) const
{
    if (request.method() != http::verb::post)
    {
        return MakeInvalidMethodResponse(request, "POST");
    }
    if (!HasJsonContentType(request))
    {
        return MakeErrorResponse(request, http::status::unauthorized, "invalidToken", "Invalid content type");
    }
    const auto token = ReadToken(request);
    if (!token)
    {
        return MakeErrorResponse(
                request, http::status::unauthorized, "invalidToken", "Authorization header is required");
    }
    std::string direction;
    try
    {
        direction = json::parse(request.body()).as_object().at("move").as_string();
    }
    catch(const std::exception&)
    {
        return MakeErrorResponse(request, http::status::bad_request, "invalidArgument", "Failed to parse action");
    }
    try
    {
        m_application.DoAction(*token, direction);
    }
    catch(const ApplicationError& e)
    {
        if (e.GetReason() == ApplicationErrorReason::InvalidToken)
        {
            return MakeUnknownTokenResponse(request);
        }
        if (e.GetReason() == ApplicationErrorReason::InvalidDirection)
        {
            return MakeErrorResponse(request, http::status::bad_request, "invalidArgument", "Failed to parse action");
        }
        throw;
    }
    return MakeResponse(request, http::status::ok, "{}");
}

ApiRequestResult ApiController::HandleTick(const StringRequest& request, const std::string_view uri) const
{
    if (request.method() != http::verb::post)
    {
        return MakeInvalidMethodResponse(request, "POST");
    }
    if (!HasJsonContentType(request))
    {
        return MakeErrorResponse(request, http::status::unauthorized, "invalidToken", "Invalid content type");
    }
    int delta = 0;
    try
    {
        const auto value = json::parse(request.body()).as_object().at("timeDelta").as_int64();
        if (value < 0 || value > std::numeric_limits<int>::max())
        {
            throw std::invalid_argument("Invalid time delta");
        }
        delta = static_cast<int>(value);
    }
    catch(const std::exception&)
    {
        return MakeErrorResponse(
                request, http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON");
    }
    try
    {
        m_application.ManualTick(delta);
    }
    catch(const ApplicationError& e)
    {
        if (e.GetReason() == ApplicationErrorReason::InvalidTimeDelta)
        {
            return MakeErrorResponse(request, http::status::bad_request, "invalidArgument", "Invalid time delta");
        }
        if (e.GetReason() != ApplicationErrorReason::InvalidState)
        {
            throw;
        }
        return HandleInvalidRequest(request, uri);
    }
    return MakeResponse(request, http::status::ok, "{}");
}

ApiRequestResult ApiController::HandleGameRecords(const StringRequest& request, const std::string_view uri) const
{
    if (request.method() != http::verb::get)
    {
        return MakeInvalidMethodResponse(request, "GET, HEAD");
    }
    std::optional<int> start;
    std::optional<int> max_items;
    const auto path = util::Split(uri, '/');
    const auto query = util::Split(path.at(3), '?');
    if (query.size() == 2)
    {
        for(auto item : util::Split(query[1], '&'))
        {
            const auto pair = util::Split(item, '=');
            if (pair.size() != 2)
            {
                continue;
            }
            try
            {
                if (pair[0] == "start")
                {
                    start = std::stoi(std::string(pair[1]));
                }
                else if (pair[0] == "maxItems")
                {
                    max_items = std::stoi(std::string(pair[1]));
                }
            }
            catch(const std::exception&)
            {
                // Existing clients get default pagination for unparseable values.
            }
        }
    }
    try
    {
        return MakeResponse(request, http::status::ok,
                json::serialize(m_view.Render(m_application.GetGameRecords(start, max_items))));
    }
    catch(const ApplicationError& e)
    {
        if (e.GetReason() != ApplicationErrorReason::InvalidMaxItems)
        {
            throw;
        }
        return HandleInvalidRequest(request, uri);
    }
}

ApiRequestResult ApiController::HandleInvalidRequest(const StringRequest& request, std::string_view)
{
    return MakeErrorResponse(request, http::status::bad_request, "badRequest", "Bad request", {}, false);
}
} // namespace http_handler
