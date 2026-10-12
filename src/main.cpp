#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>

#include "application/app.hpp"
#include "bootstrap/command_line.hpp"
#include "common/platform.hpp"
#include "controller/request_handler.hpp"
#include "infrastructure/config/json_loader.hpp"
#include "infrastructure/http/http_server.hpp"
#include "infrastructure/http/run_workers.hpp"
#include "infrastructure/http/ticker.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/persistence/postgres.hpp"
#include "infrastructure/persistence/state_store.hpp"

namespace
{

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace json = boost::json;

} // namespace

/** @brief Собирает зависимости, запускает HTTP-обработчики и сохраняет состояние при штатной остановке. */
int main(const int argc, char* argv[])
{
    bootstrap::Args args{};
    try
    {
        auto args_opt = bootstrap::ParseCommandLine(argc, argv);
        if (!args_opt.has_value())
        {
            return EXIT_SUCCESS;
        }
        args = args_opt.value();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        // Восстановление выполняется до регистрации обработчиков и открытия HTTP-порта.
        json_loader::JsonLoader loader(args.config_file_path);
        model::Game game = loader.LoadGame();
        app::Players players;

        persistence::StateStore state_store((args.state_file_is_set) ? args.state_file_path : "",
                (args.save_state_period_is_set) ? std::optional<int>(args.save_state_period) : std::nullopt);
        state_store.Restore(game, players);
        auto on_update_func = [&state_store, &game, &players](const int delta)
        {
            state_store.OnTick(delta, game, players);
        };

        const auto* url = std::getenv("GAME_DB_URL");
        if (!url)
        {
            throw std::runtime_error("GAME_DB_URL" + " environment variable not found"s);
        }

        postgres::Database database(url);

        app::Application application(
                game, players, args.mode, args.randomize_spawn_points, on_update_func, database.GetRetiredPlayer());

        view::JsonView json_view(loader.GetConfiguration());

        const unsigned num_threads = std::max(1U, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);

        // Штатный сигнал завершает обработчики до последней записи снимка.
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(
                [&ioc](const sys::error_code& ec, [[maybe_unused]] const int signal_number)
                {
                    if (!ec)
                    {
                        ioc.stop();
                        logger::PrintLog(json::value({{"code"s, 0}}), "server exited"sv);
                    }
                });

        // API и таймер разделяют один strand, защищающий домен и соединение с БД.
        auto api_strand = net::make_strand(ioc);
        auto handler =
                std::make_shared<http_handler::RequestHandler>(args.www_root_path, api_strand, application, json_view,
                        [](const std::string_view message)
                        {
                            logger::PrintLog(json::object{{"text", message}}, "request failed");
                        });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port},
                [&handler](auto&& req, auto&& send)
                {
                    (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
                });
        std::shared_ptr<ticker::Ticker> ticker;
        if (args.mode == app::ApplicationMode::Auto)
        {
            ticker = std::make_shared<ticker::Ticker>(api_strand, std::chrono::milliseconds(args.tick_period),
                    [&application](const std::chrono::milliseconds delta)
                    {
                        if (delta.count() > std::numeric_limits<int>::max())
                        {
                            throw std::overflow_error("Tick duration exceeds the supported millisecond range");
                        }
                        application.AutoTick(static_cast<int>(delta.count()));
                    });
            ticker->Start();
        }

        // Событие готовности используется HTTP-проверками и средствами наблюдения за сервером.
        logger::PrintLog(json::value({{"port"s, port}, {"address"s, address.to_string()}}), "server started"sv);

        http_server::RunWorkers(ioc, num_threads);

        if (args.state_file_is_set)
        {
            state_store.Save(game, players);
        }
    }
    catch(const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
