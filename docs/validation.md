# Testing

The Docker build runs CTest for the domain model, application use cases,
HTTP contract, configuration, and state persistence. Integration tests
use a real HTTP server and a separate PostgreSQL database.

## Build and unit tests

```sh
docker build -t dog-story:local .
```

The suite checks item generation, collisions, movement, authorization,
player registration and retirement, model invariants, numeric limits,
resource ownership, and exception behavior. Persistence checks cover
snapshots, restored relationships, and write failures.

For local build commands, see [development.md](development.md).

## API contract

The contract scenario has 121 steps covering methods, maps, authorization,
sessions, movement, time, records, static files, and recovery. It compares statuses,
headers, and JSON, normalizing random tokens. This scenario uses an in-memory
repository instead of a database.

Expected responses are defined in `tests/fixtures/api_contract.json` and
`tests/fixtures/api_contract_corrections.json`. The check compares entire responses
and verifies fixture integrity.

```sh
python3 tools/check_contract.py build/build/Release/contract_driver
```

## HTTP and PostgreSQL

The `test` profile runs scenarios in a container with a separate `game_test` database:

```sh
docker compose up -d db
docker compose exec -T db createdb -U game game_test
docker compose --profile test build test
docker compose --profile test run --rm test
```

Create the database once for each new volume. The scenarios insert records,
which is why they use a dedicated test database.

| Scenario | Coverage |
| --- | --- |
| `tools/run_http_integration.py` | Movement, item pickup and delivery, player retirement, record persistence, automatic time, snapshots, and restart |
| `tests/api_edge_check.py` | Invalid requests, boundary values, concurrent joins, and ticks |
| `tests/runtime_failure_check.py` | Process exit after a background snapshot write fails |
| `tests/postgres_review_check.py` | Long names, name validation, retirement, schema migration, and record persistence across restarts |
| `tests/original_client_http.py` | Client resources and game operations on all three maps |

`tools/run_http_integration.py` uses an isolated directory with a test HTML page.
The actual client is checked independently by `tests/original_client_http.py`
and `tests/client_resource_check.py`.

Check a running test stack:

```sh
python3 tests/docker_smoke.py
python3 tests/client_resource_check.py
```

The first scenario creates players and restarts the game container twice.
It requires a separate deployment with its own volumes and database.

## Integrity and code quality

```sh
python3 tools/verify_original_tests.py
python3 tools/verify_original_client.py
python3 tools/check_architecture.py
python3 tools/check_style.py
```

Checksums protect the test suite, fixtures, client assets, and map configuration
from accidental changes. The architecture check enforces dependency directions
between layers; the formatter applies the [style guide](codestyle.md).

The Docker build also checks Doxygen: undocumented classes, methods, and functions
are treated as errors. HTML and XML are generated from `.hpp` and `.cpp` files.

## Monitoring

Exporter tests run when its image is built. They cover JSON parsing,
incomplete lines, rotation, restarts, and recovery from read errors.

`tools/check_monitoring.py` sends successful and failing HTTP requests,
checks Prometheus counters and histograms, CPU/RAM/disk metrics,
the Grafana data source, and queries from the preconfigured dashboard.
For environment setup, see [monitoring.md](monitoring.md).

## Coverage limits

Functional checks do not replace load testing and do not lock down the random
sequence of generated items.
Consistency guarantees for the world, snapshots, and database are described in
[architecture.md](architecture.md).
