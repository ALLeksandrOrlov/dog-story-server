# Локальная сборка для разработки

Нужны компилятор C++20, Python 3.9+, Make и PostgreSQL.
CI настроен на Ubuntu 22.04.

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

Зависимости: Boost 1.86, libpqxx 7.7.4, Catch2 3.1.0. CMake закреплён на 3.27.9,
поскольку сборка этой версии libpqxx использует модуль, удалённый в новых CMake.
Conan создаёт локальные настройки сборки; `CMakeUserPresets.json` не хранится в Git.

Создайте отдельную базу и пользователя PostgreSQL, затем запустите сервер:

```sh
export GAME_DB_URL='postgresql://USER:PASSWORD@localhost:5432/DATABASE'
mkdir -p .cache/state
./build/build/Release/game_server \
  --config-file data/config.json --www-root static \
  --tick-period 50 --state-file .cache/state/game.state --save-state-period 5000
```

Сервер слушает порт 8080. Таблица рекордов создаётся при запуске.
Если убрать `--tick-period`, время продвигается только запросами `POST /api/v1/game/tick`;
браузерный клиент рассчитан на автоматическое время.
`--randomize-spawn-points` включает случайную точку появления.
Все параметры доступны через `--help`.
