#pragma once

#include "application/app.hpp"
#include "view/json_view.hpp"

#include "controller/http_types.hpp"

namespace http_handler
{
/**
 * @brief Validates HTTP requests and maps application results to API responses.
 * @details Stores no game state. Application and JsonView are borrowed and must outlive
 * this controller. All Handle calls run sequentially on the game strand.
 */
class ApiController
{
public:
    /** @brief Connects application operations to the immutable JSON view. */
    ApiController(app::Application& application, const view::JsonView& view):
            m_application(application),
            m_view(view)
    {
    }

    /**
     * @brief Executes a route and builds an HTTP response with the specified fields and status.
     * @param request Request containing a JSON body and authorization headers.
     * @param uri Decoded URI, including query parameters.
     * @throws std::exception Infrastructure failure; the outer handler converts it to HTTP 500.
     */
    ApiRequestResult Handle(const StringRequest& request, const std::string_view uri) const;

private:
    /** @brief Borrowed facade for game operations. */
    app::Application& m_application;
    /** @brief Borrowed immutable JSON view. */
    const view::JsonView& m_view;

    /** @brief Classifies a path without executing application logic. */
    static ApiRequestType ClassifyRequest(const std::string_view target);
    /** @brief Returns the list of available maps. */
    ApiRequestResult HandleMapsList(const StringRequest& request, const std::string_view uri) const;
    /** @brief Returns a map description or an unknown identifier response. */
    ApiRequestResult HandleMapDescription(const StringRequest& request, const std::string_view uri) const;
    /** @brief Validates the name and map, then registers the player. */
    ApiRequestResult HandleJoinGame(const StringRequest& request, const std::string_view uri) const;
    /** @brief Returns players in the session identified by the request token. */
    ApiRequestResult HandlePlayersList(const StringRequest& request, const std::string_view uri) const;
    /** @brief Returns a snapshot of players and items in the authorized session. */
    ApiRequestResult HandleGameState(const StringRequest& request, const std::string_view uri) const;
    /** @brief Passes a validated movement command to Application. */
    ApiRequestResult HandleAction(const StringRequest& request, const std::string_view uri) const;
    /** @brief Validates the duration and executes a manual game tick. */
    ApiRequestResult HandleTick(const StringRequest& request, const std::string_view uri) const;
    /** @brief Parses pagination parameters and returns the leaderboard. */
    ApiRequestResult HandleGameRecords(const StringRequest& request, const std::string_view uri) const;
    /** @brief Builds a response for an unknown route or invalid parameters. */
    static ApiRequestResult HandleInvalidRequest(const StringRequest& request, const std::string_view uri);
};
} // namespace http_handler
