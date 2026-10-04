#include "common/platform.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "application/app.h"
#include "bootstrap/command_line.h"
#include "controller/request_handler.h"
#include "infrastructure/config/json_loader.h"
#include "infrastructure/http/http_server.h"
#include "infrastructure/http/run_workers.h"
#include "infrastructure/http/ticker.h"
#include "infrastructure/logging/logger.h"
#include "infrastructure/persistence/postgres.h"
#include "infrastructure/persistence/state_store.h"

namespace {

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace json = boost::json;

} // namespace

int main(int argc, const char* argv[]) {
    bootstrap::Args args{};
    try {
        auto args_opt = bootstrap::ParseCommandLine(argc, argv);
        if (!args_opt.has_value()) {
            return EXIT_SUCCESS;
        }
        args = args_opt.value();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    try {
        // 1. Загружаем карту из файла и построить модель игры
        json_loader::JsonLoader loader(args.config_file_path);
        model::Game game = loader.LoadGame();
        app::Players players;

        persistence::StateStore state_store(
            args.state_file_is_set ? args.state_file_path : "",
            args.save_state_period_is_set ? std::optional<int>(args.save_state_period) : std::nullopt);
        state_store.Restore(game, players);
        auto on_update_func = [&state_store, &game, &players](int delta) {
            state_store.OnTick(delta, game, players);
        };

        const auto* url = std::getenv("GAME_DB_URL");
        if (!url) {
            throw std::runtime_error("GAME_DB_URL" + " environment variable not found"s);
        }

        postgres::Database database(pqxx::connection{url});

        app::Application application(game, players, args.mode, args.randomize_spawn_points, on_update_func,
                                     database.GetRetiredPlayer());

        view::JsonView json_view(*loader.GetConfig());

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
                logger::PrintLog(json::value({{"code"s, 0}}), "server exited"sv);
            }
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        auto api_strand = net::make_strand(ioc);
        auto handler = std::make_shared<http_handler::RequestHandler>(
            args.www_root_path, api_strand, application, json_view, [](std::string_view message) {
                logger::PrintLog(json::object{{"text", message}}, "request failed");
            });

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        std::shared_ptr<ticker::Ticker> ticker;
        if (args.mode == app::ApplicationMode::Auto) {
            ticker = std::make_shared<ticker::Ticker>(
                api_strand, std::chrono::milliseconds(args.tick_period),
                [&application](std::chrono::milliseconds delta) { application.AutoTick(delta.count()); });
            ticker->Start();
        }

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        logger::PrintLog(json::value({{"port"s, port}, {"address"s, address.to_string()}}),
                         "server started"sv);

        // 6. Запускаем обработку асинхронных операций
        http_server::RunWorkers(ioc, num_threads);

        if (args.state_file_is_set) {
            state_store.Save(game, players);
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
