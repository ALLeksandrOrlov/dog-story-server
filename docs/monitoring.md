# Monitoring

An optional Docker Compose profile adds Prometheus, Grafana, Node Exporter, and a Python JSON log exporter.
The backend writes events as JSON. The exporter does not access game objects, the database,
or the Docker socket: it receives a separate log volume mounted read-only.

## Startup

Set `POSTGRES_PASSWORD` for the main stack and a separate `GRAFANA_ADMIN_PASSWORD` in `.env`.
Choose your own passwords; the project configuration has no default Grafana password. `.env` is excluded from Git.
If needed, set `GAME_HTTP_PORT`, `GRAFANA_PORT`, and `PROMETHEUS_PORT`; the defaults are 8080, 3000, and 9090.

```sh
docker compose -f compose.yaml -f compose.monitoring.yaml --profile monitoring up -d --build
```

- Game: <http://localhost:8080>.
- Grafana: <http://localhost:3000>, username `admin`, password from `.env`.
- Preconfigured dashboard: <http://localhost:3000/d/dog-story>.
- Prometheus and target status: <http://localhost:9090/targets>.

All published ports bind to `127.0.0.1`. Exporters are accessible only within the Compose network.
The Prometheus configuration and Grafana dashboard are included in their images, so the stack also works with
a Docker daemon in a Linux VM when the project's source directory is not mounted in that VM.

Stop the entire stack while preserving data:

```sh
docker compose -f compose.yaml -f compose.monitoring.yaml --profile monitoring down
```

Stop metric collection only, leaving the game running:

```sh
docker compose -f compose.yaml -f compose.monitoring.yaml --profile monitoring stop log-exporter prometheus grafana node-exporter
```

The standard `docker compose up -d --build` starts the game and PostgreSQL without monitoring. The override file
also enables the game's file log; the `monitoring` profile enables four services for collecting and displaying metrics.
The game depends only on PostgreSQL: unavailable monitoring services do not block startup or requests.

## Dashboard metrics

| Metric | Source and meaning |
| --- | --- |
| Requests per second | `dog_story_http_requests_total{status_class}`; API and static files, status classes 1xx–5xx |
| Mean latency and p95 | `dog_story_http_request_duration_seconds`; response preparation before writing to the socket |
| 4xx and 5xx ratio | Rate of responses in the given class divided by the rate of all responses |
| Handler failures | `dog_story_request_failures_total`; separate `request failed` events |
| Collection status | `up`, `dog_story_log_readable`, and log read/parse errors |
| CPU, RAM, disk | Node Exporter; system metrics from Docker's Linux host |

Latency has a source resolution of 1 ms and excludes network transmission of the response. Histogram buckets estimate p95.
Prometheus scrapes exporters every 5 seconds; rate charts require several scrapes before data appears.
With no requests, the percentile chart may show “no data”; this does not indicate a request handling failure.
`up` confirms exporter availability, not game API health.

Node Exporter reads `/proc`, `/sys`, and file system information through a read-only mount of the Linux host root,
with capabilities disabled. Only CPU, memory, file system, and disk I/O collectors are enabled.
**On macOS with Docker Desktop or Lima, these metrics describe the Linux VM, not the physical Mac.**
Nested mount points must be visible in the VM; `rslave` bind propagation exposes new mount points.

## Logs, rotation, and restarts

`GAME_LOG_FILE=/app/logs/game.log` enables an additional synchronous Boost.Log sink. Standard JSON output
remains enabled. Records are flushed after each event. Restarting the game appends to the active file;
at 10 MiB, the file is renamed to `game.log.00001`, and so on. Up to five archives totaling
50 MiB are retained, plus the active file. Individual records are not split, so a very long event may
exceed the threshold. A file logging failure is reported once to stderr and does not interrupt the game.

The exporter checks the active file and retained archives every 250 ms. It tracks offsets by inode,
so normal rename-based rotation does not duplicate records; it also drains multiple archives created between polls.
An incomplete line waits for a newline, including when a UTF-8 character is split across writes.
Malformed lines and lines longer than 64 KiB are skipped and increment `dog_story_log_parse_errors_total`.
Labels are limited to five HTTP status classes; URLs, tokens, IP addresses, and request contents are not included in metrics.

Counters live in exporter memory. Restarting the game does not reset them. At startup, the exporter skips existing
history to avoid presenting old requests as a new traffic spike. Restarting the exporter resets
counters; Prometheus `rate()`/`increase()` account for these resets. Requests logged before the exporter starts
or while it is stopped are not recovered. These are operational metrics, not an audit log or precise billing data.
If reading falls far behind writing and an archive has already been removed by retention limits, its unread events are also lost.
External `copytruncate` rotation is not used; the exporter supports Boost.Log's built-in rename-based rotation.

Prometheus retains data in its own volume for up to 7 days or 1 GB, whichever limit is reached first.
Grafana data uses a separate volume. `GRAFANA_ADMIN_PASSWORD` sets the password when Grafana first creates its database;
for an existing volume, change the password in Grafana's interface. `down` preserves volumes; `down -v` also deletes
the game database and snapshots, so do not use it for a normal shutdown.

## Validation and updates

The exporter build runs regression tests for old records, incomplete lines, malformed JSON, invalid
status codes and durations, rotation, multiple archives between polls, restarts, and line length limits.
The Prometheus build validates its configuration with `promtool`.

Use `tools/check_monitoring.py` to check the complete running profile; pass
`GRAFANA_ADMIN_PASSWORD` in the process environment. The script reads the same port variables
and checks requests, metrics, and the preconfigured dashboard's data source.

```sh
docker compose -f compose.yaml -f compose.monitoring.yaml --profile monitoring build log-exporter prometheus
```

Versions are pinned to official releases: [Prometheus 3.15.0](https://github.com/prometheus/prometheus/releases/tag/v3.15.0),
[Grafana 13.2.3](https://grafana.com/grafana/download/13.2.3),
[Node Exporter 1.12.1](https://github.com/prometheus/node_exporter/releases/tag/v1.12.1),
[Python 3.13.16](https://github.com/docker-library/official-images/blob/master/library/python),
[Prometheus client 0.26.0](https://pypi.org/project/prometheus-client/0.26.0/).
After changing a version or configuration, rebuild the profile and check `/targets` and the preconfigured dashboard.
