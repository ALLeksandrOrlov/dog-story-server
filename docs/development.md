# Local development build

Requires a C++20 compiler, Python 3.9+, Make, and PostgreSQL.
CI runs on Ubuntu 22.04.

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements-dev.txt
conan profile detect
conan install . --output-folder=build --build=missing \
  -s build_type=Release -s compiler.cppstd=20
cmake --preset conan-release
cmake --build --preset conan-release -j 4
ctest --test-dir build/build/Release --output-on-failure
```

Dependencies: Boost 1.86, libpqxx 7.7.4, and Catch2 3.1.0. CMake is pinned to 3.27.9
because this version of libpqxx uses a module removed from newer CMake releases.
Conan generates local build settings; `CMakeUserPresets.json` is not tracked in Git.

Create a separate PostgreSQL database and user, then start the server:

```sh
export GAME_DB_URL='postgresql://USER:PASSWORD@localhost:5432/DATABASE'
mkdir -p .cache/state
./build/build/Release/game_server \
  --config-file data/config.json --www-root static \
  --tick-period 50 --state-file .cache/state/game.state --save-state-period 5000
```

The server listens on port 8080. The leaderboard table is created at startup.
Without `--tick-period`, time advances only through `POST /api/v1/game/tick` requests;
the browser client expects automatic game time.
`--randomize-spawn-points` enables random spawn positions.
Use `--help` to see all options.
