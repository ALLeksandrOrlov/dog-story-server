FROM python:3.11-slim-bookworm AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ make git pkg-config \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY requirements-build.txt conanfile.txt ./
RUN pip install --no-cache-dir -r requirements-build.txt \
    && conan profile detect \
    && conan install . --output-folder=build --build=missing \
       -s build_type=Release -s compiler.cppstd=20 -c tools.build:jobs=2
RUN apt-get update && apt-get install -y --no-install-recommends doxygen \
    && rm -rf /var/lib/apt/lists/*
COPY CMakeLists.txt Doxyfile ./
COPY src/ src/
COPY tests/ tests/
COPY tools/ tools/
# CTest verifies client runtime assets and C++ code.
COPY data/ data/
COPY static/ static/
RUN cmake -S . -B build/build/Release -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_TOOLCHAIN_FILE=/app/build/build/Release/generators/conan_toolchain.cmake \
    && cmake --build build/build/Release -j 2 \
    && ctest --test-dir build/build/Release --output-on-failure \
    && cmake --build build/build/Release --target docs

FROM debian:bookworm-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 game \
    && useradd --uid 10001 --gid game --no-create-home game \
    && mkdir -p /app/state /app/logs && chown game:game /app/state /app/logs
WORKDIR /app
COPY --from=build /app/build/build/Release/game_server ./game_server
COPY data/ data/
COPY static/ static/
COPY NOTICE ./NOTICE
COPY third_party/licenses/ third_party/licenses/
USER game
EXPOSE 8080
ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static"]
CMD ["--tick-period", "50", "--state-file", "/app/state/game.state", "--save-state-period", "5000"]
