#pragma once
#include <functional>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>

#include "controller/api_controller.hpp"
#include "controller/static_file_controller.hpp"

namespace http_handler
{
/**
 * @brief Routes API and static file requests and contains exceptions at the HTTP boundary.
 * @details Created through shared_ptr: deferred calls retain the handler until the response is sent.
 * The API runs on the supplied strand; static files are handled in the socket's execution context.
 */
class RequestHandler : public std::enable_shared_from_this<RequestHandler>
{
public:
    /** @brief Receives diagnostic messages about internal errors. */
    using ErrorReporter = std::function<void(std::string_view)>;
    /** @brief Serial executor for application operations. */
    using Strand = net::strand<net::io_context::executor_type>;

    /** @brief Borrows Application and JsonView; both must outlive all pending requests. */
    RequestHandler(const std::string_view static_path, const Strand api_strand, app::Application& app,
            const view::JsonView& view, ErrorReporter report_error = {}):
            m_api(app, view),
            m_static_files(static_path),
            m_api_strand(api_strand),
            m_report_error(std::move(report_error))
    {
    }

    /** @brief Prevents copying a handler with pending operations. */
    RequestHandler(const RequestHandler&) = delete;
    /** @brief Prevents replacing handler dependencies after startup. */
    RequestHandler& operator=(const RequestHandler&) = delete;

    /**
     * @brief Passes the request to a controller and sends exactly one result.
     * @param request Ownership of the request is transferred to the deferred API operation.
     * @param send Response sender supporting string and file bodies.
     * @details An invalid URI yields 400; application and infrastructure exceptions yield 500.
     */
    template <typename Send> void operator()(StringRequest&& request, Send&& send)
    {
        const std::string_view target = request.target();

        try
        {
            // Separate the query before decoding, so encoded '?' remains part of a filename.
            std::string uri = DecodeURI(target.substr(0, target.find('?')));
            if (IsApiRequest(uri))
            {
                uri = DecodeURI(target);
                auto handle = [self = shared_from_this(), send, request = std::forward<decltype(request)>(request), uri]
                {
                    try
                    {
                        // Serialize game changes independently of the client socket's strand.
                        assert(self->m_api_strand.running_in_this_thread());
                        return send(self->m_api.Handle(request, uri));
                    }
                    catch(const std::exception& error)
                    {
                        send(self->ReportServerError(request, error.what()));
                    }
                };
                return net::dispatch(m_api_strand, handle);
            }
            // Return the result of the static file request.
            return std::visit(
                    [&send](auto&& result)
                    {
                        send(std::forward<decltype(result)>(result));
                    },
                    m_static_files.Handle(request, uri));
        }
        catch(const InvalidTarget&)
        {
            StringResponse response{http::status::bad_request, request.version()};
            response.set(http::field::content_type, "application/json");
            response.body() = R"({"code":"badRequest","message":"Invalid request target"})";
            response.prepare_payload();
            send(std::move(response));
        }
        catch(const std::exception& error)
        {
            send(ReportServerError(request, error.what()));
        }
    }

private:
    /** @brief URI syntax error, distinct from an internal processing failure. */
    class InvalidTarget : public std::invalid_argument
    {
    public:
        using std::invalid_argument::invalid_argument;
    };

    /** @brief Decodes percent escapes; rejects incomplete sequences and NUL. */
    static std::string DecodeURI(const std::string_view target);

    /** @brief Checks the first path segment for the API prefix. */
    static bool IsApiRequest(const std::string_view target);

    /** @brief Logs the cause and returns 500 without exposing internal details to the client. */
    StringResponse ReportServerError(const StringRequest& request, const std::string_view message) const;

    /** @brief Controller for game HTTP routes. */
    ApiController m_api;
    /** @brief Controller for files under the client root directory. */
    StaticFileController m_static_files;
    /** @brief Executor shared with the timer to protect game state. */
    Strand m_api_strand;
    /** @brief Optional receiver of diagnostic messages. */
    ErrorReporter m_report_error;
};

} // namespace http_handler
