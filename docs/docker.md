# Docker deployment

## Requirements

- Windows or macOS: Docker Desktop ([Windows installation](https://docs.docker.com/desktop/setup/install/windows-install/),
  [macOS installation](https://docs.docker.com/desktop/setup/install/mac-install/)), started and configured to run Linux containers.
- Linux: [Docker Engine](https://docs.docker.com/engine/install/) with the
  [Docker Compose plugin](https://docs.docker.com/compose/install/linux/) and a running Docker daemon.
- An Intel/AMD 64-bit (`amd64`) or ARM64 machine. Docker selects the native Linux
  base images, and Conan detects the container's architecture when building the
  dependencies. Apple Silicon uses ARM64. These are Linux container builds;
  Windows hosts must use Linux containers.
- Internet access for the first build to download images, build tools, and dependencies.
- A browser with WebGL support to play the game.

The first build compiles C++ dependencies and can take several minutes, depending
on the machine and connection. Subsequent builds reuse cached layers. The host
does not need a C++ toolchain, Python, Conan, or a PostgreSQL installation to run
the game.

Check that Docker is running and Compose is available:

```sh
docker info
docker compose version
```

## Resources

Building from source needs more resources than running the game. Measurements
below are from Linux ARM64 containers on Apple Silicon, without monitoring:

| Workload or image | Observed usage |
| --- | --- |
| Dependencies and application compiled from scratch, tests and Doxygen | Passed with a **2 GiB container memory limit**, swap disabled, and 2 CPUs; observed peak about 1.8 GiB including file cache |
| Game server and PostgreSQL | About 30–70 MiB of memory in local checks, including three players on separate maps |
| Runtime images: game + PostgreSQL | About 0.8 GB combined (0.17 GB + 0.64 GB as reported by Docker) |
| Build/test image, including compiler and dependencies | About 1.46 GB |

Allow roughly **4–5 GB of free Docker storage for the first source build**.
This is a planning estimate for images, build cache, and temporary dependency
files (about 1.4 GB observed during compilation), rather than the installed
game's size. Later starts reuse the images. Database records, logs, and additional
cached builds need more space over time. Check free space inside Docker's VM as
well as on the host.

The 2 GiB build limit applies to the compilation container, not the whole host.
Runtime memory was sampled with `docker stats`, which excludes inactive file
cache on Linux. These are small local workloads, not capacity limits. Docker's
engine or VM, the browser, and optional monitoring need additional memory;
Docker Desktop's own host requirements still apply. Image sizes and build usage
can differ on AMD64 and with updated base images.

## Build and play

Clone the repository, then enter its root directory:

```sh
git clone https://github.com/ALLeksandrOrlov/dog-story-server.git
cd dog-story-server
```

Alternatively, download and extract the repository ZIP. Open a terminal in the
directory containing `compose.yaml`; run all commands below from that directory.

Create `.env` from the example. On Windows with PowerShell:

```powershell
Copy-Item .env.example .env
```

On macOS or Linux:

```sh
cp .env.example .env
```

Open `.env` in a text editor and set `POSTGRES_PASSWORD` to your own
password. Use letters, digits, hyphens, and underscores so the value can be used
directly in the database URL. The `.env` file is excluded from Git and the Docker
build context. Keep it for subsequent starts of the same database.

Build and start the services with the same command on every host OS:

```sh
docker compose up -d --build
```

Open [localhost:8080](http://localhost:8080), enter a player name, and select a map.
Use the arrow keys to move, collect items, and take them to delivery points to score.
The game may need a moment to finish starting after the command returns; refresh
the page if necessary. By default, an inactive player retires after 15 seconds,
and the result appears on the leaderboard.

Optional Prometheus, Grafana, and metric exporter services are available through
a separate [monitoring profile](monitoring.md).

Compose starts two services: `game` and `db`. PostgreSQL must pass its readiness
check before the game starts. The game port is published only on
`127.0.0.1:8080`; the PostgreSQL port is not published.

If port 8080 is already in use, add this line to `.env`:

```dotenv
GAME_HTTP_PORT=8081
```

Run `docker compose up -d` again, then open
[localhost:8081](http://localhost:8081). This setting works the same way in
PowerShell and Unix shells.

## Status, restart, and saved data

Check service status and follow the game logs:

```sh
docker compose ps
docker compose logs -f game
```

Press Ctrl+C to stop following logs; the services keep running. If startup fails,
also inspect the database logs with `docker compose logs db`. A database password
error usually means `.env` no longer matches the password used to initialize the
existing database volume. A build failure reporting insufficient space requires
more free space in Docker's storage; host free space alone may not increase a
Docker Desktop VM's disk capacity.

Restart only the game, or stop the whole stack:

```sh
docker compose restart game
docker compose down
```

A normal `down` preserves the `database` and `snapshots` volumes. Start again
with `docker compose up -d` to reuse the saved records and active game state.
After changing source files, use `docker compose up -d --build` to rebuild.

Keep `POSTGRES_PASSWORD` unchanged while reusing an existing database, unless you
also change that database user's password in PostgreSQL. Updating `.env` alone
does not change a password stored in an initialized database.

**`docker compose down -v` permanently deletes this stack's database and snapshots.**
Use it only when you deliberately want to reset all local game data.

## Image contents

You can also build the image with `docker build -t dog-story:local .`.
ENTRYPOINT starts the server with the configuration and static files
under `/app`. A standalone `docker run` requires a PostgreSQL address in `GAME_DB_URL`.
Compose sets it automatically. The defaults enable a 50 ms tick, periodic saves,
and a final snapshot on normal shutdown.

The build stage installs a C++20 compiler, Conan, and CMake, and runs the full CTest suite.
The final image contains only the application, client, configuration, licenses, and
required system libraries. The process runs with UID/GID 10001.
Checksums verify the integrity of the test suite.

## Check a running stack

These optional checks require Python 3 on the host. On Windows, use `python`
instead of `python3` if that is how Python is installed:

```sh
python3 tests/docker_smoke.py
python3 tests/client_resource_check.py
```

These scripts use port 8080 by default and do not read `.env`. For a custom port,
also set `GAME_HTTP_PORT` in the terminal before running them: use
`$env:GAME_HTTP_PORT = '8081'` in PowerShell or `export GAME_HTTP_PORT=8081` on
macOS/Linux.

The first scenario creates test players with unique names, checks maps, movement,
authorization, and retirement with PostgreSQL persistence, and restarts the game
container twice to verify snapshots and records. Run it on a local or test deployment.
CI is configured to build and verify the Docker deployment on native AMD64 and
ARM64 Linux runners. This does not replace checking a particular host's Docker
installation and resources.

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
