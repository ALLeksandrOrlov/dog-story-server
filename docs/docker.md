# Docker deployment

Run Docker Compose from the repository root for the standard deployment.

Optional Prometheus, Grafana, and metric exporter services are available through
a separate [monitoring profile](monitoring.md).

```sh
cp .env.example .env
# Set your own POSTGRES_PASSWORD in .env.
docker compose up -d --build
```

The `.env` file contains local settings and is excluded from Git and the Docker build context.

Compose starts two services: `game` and `db`. PostgreSQL must pass its readiness
check before the game starts. The game port is published only on
`127.0.0.1:8080`; the PostgreSQL port is not published. Use `GAME_HTTP_PORT`
to select a different game port, such as `8081`.

```sh
docker compose ps
docker compose logs -f game
docker compose restart game
docker compose down
```

A normal `down` preserves the `database` and `snapshots` volumes. Do not add `-v`
if you need to keep the data. For an existing database, do not change the password
in `.env` without making the corresponding password change in PostgreSQL.

You can also build the image with `docker build -t dog-story:local .`.
ENTRYPOINT starts the server with the configuration and static files
under `/app`. A standalone `docker run` requires a PostgreSQL address in `GAME_DB_URL`.
Compose sets it automatically. The defaults enable a 50 ms tick, periodic saves,
and a final snapshot on normal shutdown.

The build stage installs a C++20 compiler, Conan, and CMake, and runs the full CTest suite.
The final image contains only the application, client, configuration, licenses, and
required system libraries. The process runs with UID/GID 10001.
Checksums verify the integrity of the test suite.

Check a running stack:

```sh
python3 tests/docker_smoke.py
python3 tests/client_resource_check.py
```

The first scenario creates test players with unique names, checks maps, movement,
authorization, and retirement with PostgreSQL persistence, and restarts the game
container twice to verify snapshots and records. Run it on a local or test deployment.
CI builds and verifies the Docker deployment.

## Full API tests in Docker

The separate `test` profile uses the image's build stage and a dedicated `game_test`
database. It runs HTTP, retirement, snapshot, concurrent request, and client resource
loading scenarios.

```sh
docker compose up -d db
# Create once for this PostgreSQL volume:
docker compose exec -T db createdb -U game game_test
docker compose --profile test build test
docker compose --profile test run --rm test
```

This profile does not publish the game port on the host or use the main `game` database.
The same small test database can be reused; after many runs, older records may affect
checks of the first 100 entries.
