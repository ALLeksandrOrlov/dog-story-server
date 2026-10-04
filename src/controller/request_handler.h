#pragma once
#include "controller/api_controller.h"
#include "controller/static_file_controller.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <functional>

namespace http_handler {
class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
    class InvalidTarget : public std::invalid_argument {
      public:
        using std::invalid_argument::invalid_argument;
    };

  public:
    using ErrorReporter = std::function<void(std::string_view)>;
    using Strand = net::strand<net::io_context::executor_type>;

    RequestHandler(std::string_view static_path, Strand api_strand, app::Application& app,
                   const view::JsonView& view, ErrorReporter report_error = {})
        : api_(app, view), static_files_(static_path), api_strand_(api_strand),
          report_error_(std::move(report_error)) {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target = req.target();

        try {
            // Separate the query before decoding, so encoded '?' remains part of a filename.
            std::string uri = DecodeURI(target.substr(0, target.find('?')));
            if (IsApiRequest(uri)) {
                uri = DecodeURI(target);
                auto handle = [self = shared_from_this(), send, req = std::forward<decltype(req)>(req), uri] {
                    try {
                        // Этот assert не выстрелит, так как лямбда-функция будет выполняться внутри strand
                        assert(self->api_strand_.running_in_this_thread());
                        return send(self->api_.Handle(req, uri));
                    } catch (std::exception& e) {
                        send(self->ReportServerError(req, e.what()));
                    }
                };
                return net::dispatch(api_strand_, handle);
            }
            // Возвращаем результат обработки запроса к файлу
            return std::visit([&send](auto&& result) { send(std::forward<decltype(result)>(result)); },
                              static_files_.Handle(req, uri));
        } catch (const InvalidTarget&) {
            StringResponse response{http::status::bad_request, req.version()};
            response.set(http::field::content_type, "application/json");
            response.body() = R"({"code":"badRequest","message":"Invalid request target"})";
            response.prepare_payload();
            send(std::move(response));
        } catch (std::exception& e) {
            send(ReportServerError(req, e.what()));
        }
    }

  private:
    static std::string DecodeURI(std::string_view target);

    static bool IsApiRequest(std::string_view target);

    StringResponse ReportServerError(const StringRequest& req, std::string_view message) const;

    ApiController api_;
    StaticFileController static_files_;
    Strand api_strand_;
    ErrorReporter report_error_;
};

} // namespace http_handler
