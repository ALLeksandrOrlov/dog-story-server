cmake_minimum_required(VERSION 3.25)
project(game_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Boost 1.86 REQUIRED COMPONENTS json log log_setup program_options serialization)
find_package(libpqxx CONFIG REQUIRED)

add_library(game_options INTERFACE)
target_include_directories(game_options INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(game_options INTERFACE BOOST_BEAST_USE_STD_STRING_VIEW)

add_library(game_domain
    src/domain/map.cpp src/domain/dog.cpp src/domain/game_session.cpp src/domain/game.cpp
    src/domain/collision_detector.cpp src/domain/loot_generator.cpp src/common/tagged_uuid.cpp)
target_link_libraries(game_domain PUBLIC game_options Boost::headers)

add_library(game_application src/application/app.cpp src/application/players.cpp)
target_link_libraries(game_application PUBLIC game_domain)

add_library(game_view src/view/json_view.cpp)
target_link_libraries(game_view PUBLIC game_application Boost::json)

add_library(game_controller
    src/controller/api_controller.cpp src/controller/request_handler.cpp
    src/controller/static_file_controller.cpp src/common/string_processing.cpp)
target_link_libraries(game_controller PUBLIC game_view Threads::Threads)

add_library(game_logging src/infrastructure/logging/logger.cpp)
target_link_libraries(game_logging PUBLIC game_options Boost::json Boost::log Boost::log_setup Threads::Threads)

add_library(game_transport src/infrastructure/http/http_server.cpp)
target_link_libraries(game_transport PUBLIC game_logging)

# Compatibility umbrella; transport and controllers build independently.
add_library(game_http INTERFACE)
target_link_libraries(game_http INTERFACE game_controller game_transport)

add_library(game_persistence src/infrastructure/persistence/state_store.cpp)
target_link_libraries(game_persistence PUBLIC game_application Boost::serialization)

add_library(game_config src/infrastructure/config/json_loader.cpp)
target_link_libraries(game_config PUBLIC game_domain Boost::json)

add_library(game_postgres src/infrastructure/persistence/postgres.cpp)
target_link_libraries(game_postgres PUBLIC game_domain libpqxx::pqxx)

add_executable(game_server src/main.cpp src/bootstrap/command_line.cpp)
target_link_libraries(game_server PRIVATE game_http game_config game_persistence game_postgres Boost::program_options)

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    include(Catch)
    add_executable(loot_generator_tests tests/loot_generator_tests.cpp)
    target_link_libraries(loot_generator_tests PRIVATE game_domain Catch2::Catch2WithMain)
    add_executable(collision_detection_tests tests/collision-detector-tests.cpp)
    target_link_libraries(collision_detection_tests PRIVATE game_domain Catch2::Catch2WithMain)
    catch_discover_tests(loot_generator_tests)
    catch_discover_tests(collision_detection_tests)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME upstream_tests_unchanged COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/verify_original_tests.py)

    add_executable(contract_driver tests/support/contract_driver.cpp)
    target_link_libraries(contract_driver PRIVATE game_http game_config game_persistence)
    add_test(NAME api_contract COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_contract.py $<TARGET_FILE:contract_driver>)

    add_executable(application_tests tests/application_tests.cpp)
    target_link_libraries(application_tests PRIVATE game_application game_view game_config game_persistence Catch2::Catch2WithMain)
    target_compile_definitions(application_tests PRIVATE TEST_CONFIG="${PROJECT_SOURCE_DIR}/tests/fixtures/contract.json")
    catch_discover_tests(application_tests)

    add_executable(bug_regression_tests tests/bug_regression_tests.cpp)
    target_link_libraries(bug_regression_tests PRIVATE game_http game_config game_persistence Catch2::Catch2WithMain)
    target_compile_definitions(bug_regression_tests PRIVATE TEST_CONFIG="${PROJECT_SOURCE_DIR}/tests/fixtures/contract.json")
    catch_discover_tests(bug_regression_tests)

    add_executable(static_query_tests tests/static_query_tests.cpp)
    target_link_libraries(static_query_tests PRIVATE game_http game_config Catch2::Catch2WithMain)
    target_compile_definitions(static_query_tests PRIVATE TEST_CONFIG="${PROJECT_SOURCE_DIR}/tests/fixtures/contract.json")
    catch_discover_tests(static_query_tests)

    add_executable(architecture_tests tests/architecture_tests.cpp)
    target_link_libraries(architecture_tests PRIVATE game_http game_config game_persistence Catch2::Catch2WithMain)
    target_compile_definitions(architecture_tests PRIVATE TEST_CONFIG="${PROJECT_SOURCE_DIR}/tests/fixtures/contract.json")
    catch_discover_tests(architecture_tests)
    add_test(NAME original_client_unchanged COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/verify_original_client.py)
    add_test(NAME architecture_boundaries COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_architecture.py)
endif()
