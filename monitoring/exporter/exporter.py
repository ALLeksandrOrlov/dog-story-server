"""Convert newly appended game JSON log records to bounded Prometheus metrics."""

from dataclasses import dataclass, field
import json
import math
import os
from pathlib import Path
import signal
import threading
import time

from prometheus_client import CollectorRegistry, Counter, Gauge, Histogram, start_http_server


MAX_LINE_BYTES = 64 * 1024
READ_BYTES_PER_FILE = 1024 * 1024
POLL_SECONDS = 0.25


class GameMetrics:
    """Own one exporter's counters; restarting the exporter starts a new series epoch."""

    def __init__(self, registry):
        self.requests = Counter(
            "dog_story_http_requests_total", "Completed HTTP responses read from the log.",
            ["status_class"], registry=registry,
        )
        for status_class in ("1xx", "2xx", "3xx", "4xx", "5xx"):
            self.requests.labels(status_class)
        self.duration = Histogram(
            "dog_story_http_request_duration_seconds", "Response preparation time, excluding socket writes.",
            buckets=(0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10), registry=registry,
        )
        self.failures = Counter("dog_story_request_failures_total", "Request failures reported by the server.",
                                registry=registry)
        self.lines = Counter("dog_story_log_lines_total", "Complete log lines consumed since exporter startup.",
                             registry=registry)
        self.parse_errors = Counter("dog_story_log_parse_errors_total", "Malformed or oversized log lines skipped.",
                                    registry=registry)
        self.read_errors = Counter("dog_story_log_read_errors_total", "Log filesystem operations that failed.",
                                   registry=registry)
        self.readable = Gauge("dog_story_log_readable", "Whether the active log was readable during the last poll.",
                              registry=registry)
        self.last_poll = Gauge("dog_story_log_last_poll_timestamp_seconds", "Unix time of the last completed poll.",
                               registry=registry)

    def consume(self, line):
        """Validate one complete UTF-8 record; never derive labels from user-controlled strings."""
        self.lines.inc()
        try:
            record = json.loads(line)
            if not isinstance(record, dict) or not isinstance(record.get("message"), str):
                raise ValueError("Expected a log event")
            if record["message"] == "request failed":
                self.failures.inc()
            if record["message"] != "response sent":
                return
            data = record["data"]
            if not isinstance(data, dict):
                raise ValueError("Expected event data")
            code = data["code"]
            if type(code) is not int or not 100 <= code <= 599:
                raise ValueError("Invalid HTTP status")
            raw_duration = data["response_time"]
            if isinstance(raw_duration, bool) or not isinstance(raw_duration, (str, int, float)):
                raise ValueError("Invalid duration type")
            duration = float(raw_duration) / 1000.0
            if not math.isfinite(duration) or duration < 0:
                raise ValueError("Invalid duration")
        except (ValueError, TypeError, KeyError, UnicodeError, OverflowError, RecursionError):
            self.parse_errors.inc()
            return
        self.requests.labels(f"{code // 100}xx").inc()
        self.duration.observe(duration)


@dataclass
class FileCursor:
    """Track a file by inode across rename, preserving an incomplete trailing record."""

    offset: int = 0
    pending: bytearray = field(default_factory=bytearray)
    discard_until_newline: bool = False

    def accept(self, chunk, metrics):
        """Split bounded chunks into records, discarding oversized lines exactly once."""
        for part in chunk.splitlines(keepends=True):
            # Only LF terminates a JSONL record; splitlines also recognizes CR.
            if self.discard_until_newline:
                if part.endswith(b"\n"):
                    self.discard_until_newline = False
                continue
            self.pending.extend(part)
            if len(self.pending) > MAX_LINE_BYTES:
                metrics.lines.inc()
                metrics.parse_errors.inc()
                self.pending.clear()
                self.discard_until_newline = not part.endswith(b"\n")
            elif part.endswith(b"\n"):
                metrics.consume(bytes(self.pending))
                self.pending.clear()


class LogTailer:
    """Tail the active file and numbered rotation archives without replaying startup history."""

    def __init__(self, path, metrics):
        self.path = Path(path)
        self.metrics = metrics
        self.cursors = {}
        self.initialized = False
        self._previous_seen = set()

    def _paths(self):
        """Discover only this sink's archives; unrelated files are never read."""
        prefix = self.path.name + "."
        paths = [self.path]
        paths.extend(sorted(
            path for path in self.path.parent.iterdir()
            if path.name.startswith(prefix) and path.name[len(prefix):].isdigit()
        ))
        return paths

    def poll(self):
        """Read at most one MiB per file per poll, including records awaiting rotation."""
        seen = set()
        active_readable = False
        scan_succeeded = True
        try:
            paths = self._paths()
        except FileNotFoundError:
            paths = []
            scan_succeeded = False
        except OSError:
            self.metrics.read_errors.inc()
            paths = []
            scan_succeeded = False
        for path in paths:
            try:
                with path.open("rb") as source:
                    info = os.fstat(source.fileno())
                    key = (info.st_dev, info.st_ino)
                    if key in seen:
                        continue
                    seen.add(key)
                    cursor = self.cursors.get(key)
                    if cursor is None:
                        cursor = FileCursor()
                        if not self.initialized:
                            cursor.offset = info.st_size
                            if info.st_size:
                                source.seek(-1, os.SEEK_END)
                                cursor.discard_until_newline = source.read(1) != b"\n"
                        self.cursors[key] = cursor
                    elif info.st_size < cursor.offset:
                        cursor = FileCursor()
                        self.cursors[key] = cursor
                    source.seek(cursor.offset)
                    chunk = source.read(READ_BYTES_PER_FILE)
                    cursor.offset += len(chunk)
                    cursor.accept(chunk, self.metrics)
                    if path == self.path:
                        active_readable = True
            except FileNotFoundError:
                # Rotation can rename a file between discovery and open.
                scan_succeeded = False
                continue
            except OSError:
                self.metrics.read_errors.inc()
                scan_succeeded = False
        # Keep one previous poll's missing inodes to cover a rename during discovery.
        # An incomplete scan does not prove deletion: keep offsets through I/O failures.
        if scan_succeeded:
            self.cursors = {key: value for key, value in self.cursors.items() if key in seen or key in self._previous_seen}
            self._previous_seen = seen
        self.initialized = True
        self.metrics.readable.set(active_readable)
        self.metrics.last_poll.set(time.time())


def main():
    """Serve scrape requests concurrently while polling logs until SIGTERM or SIGINT."""
    registry = CollectorRegistry()
    metrics = GameMetrics(registry)
    tailer = LogTailer(os.environ.get("GAME_LOG_FILE", "/logs/game.log"), metrics)
    stopping = threading.Event()
    signal.signal(signal.SIGTERM, lambda *_: stopping.set())
    signal.signal(signal.SIGINT, lambda *_: stopping.set())
    # Establish startup offsets before exposing metrics: historical lines are not traffic.
    tailer.poll()
    server, thread = start_http_server(9200, registry=registry)
    try:
        while not stopping.wait(POLL_SECONDS):
            tailer.poll()
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()
