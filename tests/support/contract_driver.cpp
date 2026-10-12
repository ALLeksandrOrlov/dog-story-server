// Exercise the same public request-handler interface against either implementation.
#ifdef GAME_BASELINE
#include "app.h"
#include "app_serialization.h"
#include "json_loader.h"
#include "model_serialization.h"
#include "request_handler.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#else
#include "application/app.hpp"
#include "controller/request_handler.hpp"
#include "infrastructure/config/json_loader.hpp"
#include "infrastructure/persistence/state_store.hpp"
#include "view/json_view.hpp"
#endif
#include "memory_repository.h"
#include <boost/json.hpp>
#include <iostream>
#include <type_traits>

int main(int argc, char* argv[]) {
    namespace json = boost::json;
    namespace http = boost::beast::http;
    if (argc < 3)
        return 2;
    json_loader::JsonLoader loader(argv[1]);
    auto game = loader.LoadGame();
    app::Players players;
    MemoryRepository repository;
    const auto mode = argc > 3 ? app::ApplicationMode::Auto : app::ApplicationMode::Manual;
#ifdef GAME_BASELINE
    auto make_application = [&] {
        return std::make_unique<app::Application>(game, players, mode, false, loader,
                                                  std::function<void(int)>{}, repository);
    };
#else
    auto make_application = [&] {
        return std::make_unique<app::Application>(game, players, mode, false, std::function<void(int)>{},
                                                  repository);
    };
    view::JsonView view(loader.GetConfiguration());
#endif
    auto application = make_application();
    boost::asio::io_context context;
    auto strand = boost::asio::make_strand(context);
#ifdef GAME_BASELINE
    auto make_handler = [&] {
        return std::make_shared<http_handler::RequestHandler>(argv[2], strand, *application);
    };
#else
    auto make_handler = [&] {
        return std::make_shared<http_handler::RequestHandler>(argv[2], strand, *application, view);
    };
#endif
    auto handler = make_handler();
    std::vector<std::string> tokens;
    std::string line;
    while (std::getline(std::cin, line)) {
        auto input = json::parse(line).as_object();
        if (input.contains("snapshot")) {
#ifdef GAME_BASELINE
            std::stringstream buffer;
            {
                boost::archive::text_oarchive output(buffer);
                std::vector<serialization::GameSessionRepr> sessions;
                for (const auto& session : game.GetSessions())
                    sessions.emplace_back(session);
                serialization::PlayersRepr saved_players(players);
                output << sessions << saved_players;
            }
            game = loader.LoadGame();
            players = {};
            {
                boost::archive::text_iarchive input_archive(buffer);
                std::vector<serialization::GameSessionRepr> sessions;
                serialization::PlayersRepr saved_players;
                input_archive >> sessions >> saved_players;
                for (const auto& session : sessions)
                    game.AddSession(session.Restore(game));
                players = saved_players.Restore(game);
            }
#else
            auto path = std::filesystem::temp_directory_path() /
                        ("contract-" + domain::RetiredPlayerId::New().ToString());
            persistence::StateStore snapshots(path, {});
            snapshots.Save(game, players);
            game = loader.LoadGame();
            players = {};
            snapshots.Restore(game, players);
            std::filesystem::remove(path);
#endif
            handler.reset();
            application = make_application();
            handler = make_handler();
            std::cout << "{}" << std::endl;
            continue;
        }
        if (input.contains("autoTick")) {
            application->AutoTick(static_cast<int>(input.at("autoTick").as_int64()));
            std::cout << "{}" << std::endl;
            continue;
        }
        http_handler::StringRequest request{http::string_to_verb(input.at("method").as_string()),
                                            input.at("target").as_string(), 11};
        if (input.contains("body"))
            request.body() = input.at("body").as_string();
        if (input.contains("headers")) {
            for (const auto& header : input.at("headers").as_object()) {
                std::string value(header.value().as_string());
                if (value.starts_with("Bearer @"))
                    value = "Bearer " + tokens.at(std::stoul(value.substr(8)));
                request.set(header.key(), value);
            }
        }
        request.prepare_payload();
        json::object output;
        bool sent = false;
        (*handler)(std::move(request), [&](auto&& response) {
            sent = true;
            output["status"] = response.result_int();
            json::object headers;
            for (const auto& header : response.base())
                headers[header.name_string()] = header.value();
            output["headers"] = std::move(headers);
            if constexpr (std::is_same_v<std::decay_t<decltype(response.body())>, std::string>) {
                boost::system::error_code error;
                json::value body = json::parse(response.body(), error);
                if (error)
                    body = response.body();
                if (body.is_object() && body.as_object().contains("authToken")) {
                    auto& token = body.as_object().at("authToken");
                    tokens.emplace_back(token.as_string());
                    token = "@" + std::to_string(tokens.size() - 1);
                }
                output["body"] = std::move(body);
            } else {
                output["fileSize"] = response.body().size();
            }
        });
        context.run();
        context.restart();
        if (!sent)
            throw std::runtime_error("Request was not dispatched");
        std::cout << json::serialize(output) << std::endl;
    }
}
