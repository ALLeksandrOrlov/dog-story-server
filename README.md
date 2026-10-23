# Dog Story — C++ Game Server

[Quick start](#quick-start) · [Architecture](#structure) · [API](docs/api.md) · [Tests](docs/validation.md)

A multiplayer game about dogs collecting items. Players travel along roads,
pick up items, and take them to delivery points. The server calculates movement,
collisions, and scores; PostgreSQL stores the leaderboard of completed games.

The backend is written in C++20. It includes a Three.js browser client and three maps.
Game results are stored in PostgreSQL through a repository, while active sessions
are restored from file snapshots.

## Screenshots

| Login and map selection | Playing on Map 3 |
| --- | --- |
| ![Login and map selection screen](docs/images/login.jpg) | ![Map 3 game world](docs/images/gameplay.jpg) |

## Features

- REST API for maps, joining games, movement, state, and records.
- Players isolated by game session.
- Manual and automatic game time.
- Periodic state snapshots and recovery after a restart.
- Asynchronous HTTP with Boost.Beast / Asio; game logic serialized through a strand.
- Structured JSON request and response logs.

## Quick start

Requires Docker and Docker Compose:

```sh
cp .env.example .env
# Set your own URL-safe POSTGRES_PASSWORD.
docker compose up -d --build
```

Open [localhost:8080](http://localhost:8080), enter a name, and select a map.
With the default settings, an inactive player leaves the game after 15 seconds;
their result then appears on the leaderboard.

PostgreSQL is accessible only within the Compose network. Database data and state
snapshots use separate volumes. Stop the services with `docker compose down`.

## Docker

[Deployment and testing details](docs/docker.md).

Build and start the server and PostgreSQL:

```sh
docker compose up -d --build
```

Check service status and logs:

```sh
docker compose ps
docker compose logs -f game
```

Stop the services while preserving data with `docker compose down`. Starting them
again reuses the same database and snapshot volumes. Keep the password in `.env`
unchanged for an existing database; this file is excluded from Git and the image.
Use `GAME_HTTP_PORT` to choose a different local port, for example
`GAME_HTTP_PORT=8081 docker compose up -d`.

You can also build the image separately:

```sh
docker build -t dog-story:local .
```

Its entry point runs `/app/game_server` with the configuration at `/app/data/config.json`
and the client at `/app/static`. A standalone `docker run` requires `GAME_DB_URL`
pointing to a PostgreSQL instance accessible from the container and port 8080 published.
Automatic game time and snapshots in `/app/state` are enabled by default.

The application is built in a separate Docker stage, which also runs CTest. The final
image contains the executable, configuration, client, and licenses, and runs as an
unprivileged user. Deployment does not require a compiler or Conan on the host.

A [local C++ build](docs/development.md) is available for development.
Use Docker for deployment.

An optional [monitoring profile](docs/monitoring.md) provides Prometheus, Grafana,
and a preconfigured dashboard for requests and resource usage. The main stack does
not require these services.

## Structure

| Directory | Responsibility |
| --- | --- |
| `src/domain` | Maps, player dogs, sessions, movement, item collection, and scores |
| `src/application` | Joining games, tokens, commands, queries, and player retirement |
| `src/controller` | Routing, HTTP validation, API, and static files |
| `src/view` | Converting operation results to JSON |
| `src/infrastructure` | HTTP transport, configuration, PostgreSQL, snapshots, and logs |
| `src/bootstrap` | Startup options; `main.cpp` wires the components together |
| `static` | Dog Story browser client, libraries, and 3D models |

In this server-side MVC design, the model consists of the domain and application
operations, the view is JSON, and the controller is the HTTP API. See the
[architecture](docs/architecture.md), [API](docs/api.md), and [testing](docs/validation.md) documentation.

## Development

C++ conventions are described in the [style guide](docs/codestyle.md) and checked
automatically. C++ headers use the `.hpp` extension; namespaces group components
by purpose. Doxygen comments describe responsibilities, object ownership, and
method constraints.

```sh
python3 tools/check_style.py        # Check C++ style
python3 tools/check_style.py --fix  # Apply formatting
python3 tools/verify_original_tests.py
```

Tests cover item generation, collisions, game sessions, layer boundaries,
state persistence and recovery, and the HTTP API. Integration scenarios run with
PostgreSQL in Docker; see the [instructions](docs/docker.md).

Doxygen documentation is checked when building the Docker image. For a local build
with Doxygen installed, run `cmake --build build/build/Release --target docs`.
HTML output is written to `build/docs/html/index.html`.

## State persistence

Snapshots contain player positions, directions, bags, scores, and tokens.
Current velocity and time counters are reset during recovery:
the client must send a new command to resume movement.
Completed game results are stored separately in PostgreSQL.

Third-party notices: [NOTICE](NOTICE).
