"""Regression checks for log ingestion, validation and restart/rotation semantics."""

import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from prometheus_client import CollectorRegistry, generate_latest

from exporter import FileCursor, GameMetrics, LogTailer, MAX_LINE_BYTES


def response(code=200, milliseconds="15", **extra):
    """Build a representative response record using the server's existing schema."""
    return (json.dumps({"message": "response sent", "data": {
        "code": code, "response_time": milliseconds, "content_type": "application/json", **extra,
    }}, ensure_ascii=False) + "\n").encode()


class ExporterTests(unittest.TestCase):
    """Exercise observable metrics instead of implementation-specific cursor layouts."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "game.log"
        self.registry = CollectorRegistry()
        self.metrics = GameMetrics(self.registry)
        self.tailer = LogTailer(self.path, self.metrics)

    def sample(self, name, labels=None):
        return self.registry.get_sample_value("dog_story_" + name, labels or {})

    def append(self, data):
        with self.path.open("ab") as stream:
            stream.write(data)

    def test_existing_history_is_skipped_and_new_responses_are_counted(self):
        self.path.write_bytes(response())
        self.tailer.poll()
        self.append(response(404, 25))
        self.tailer.poll()
        self.assertEqual(self.sample("http_requests_total", {"status_class": "2xx"}), 0)
        self.assertEqual(self.sample("http_requests_total", {"status_class": "4xx"}), 1)
        self.assertAlmostEqual(self.sample("http_request_duration_seconds_sum"), 0.025)
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)

    def test_missing_file_can_appear_later(self):
        self.tailer.poll()
        self.assertEqual(self.sample("log_readable"), 0)
        self.append(response())
        self.tailer.poll()
        self.assertEqual(self.sample("log_readable"), 1)
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)

    def test_incomplete_utf8_line_waits_until_completed(self):
        self.tailer.poll()
        line = response(note="собака")
        split = line.index("с".encode()) + 1
        self.append(line[:split])
        self.tailer.poll()
        self.assertEqual(self.sample("log_lines_total"), 0)
        self.append(line[split:])
        self.tailer.poll()
        self.assertEqual(self.sample("log_parse_errors_total"), 0)
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)

    def test_startup_partial_line_is_not_replayed(self):
        line = response()
        self.path.write_bytes(line[:20])
        self.tailer.poll()
        self.append(line[20:] + line)
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)
        self.assertEqual(self.sample("log_parse_errors_total"), 0)

    def test_rotation_drains_old_inode_and_reads_new_active_file_once(self):
        self.tailer.poll()
        self.append(response())
        self.tailer.poll()
        self.append(response(404))
        self.path.rename(self.path.with_name("game.log.00001"))
        self.append(response(503))
        self.tailer.poll()
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 3)
        for label in ("2xx", "4xx", "5xx"):
            self.assertEqual(self.sample("http_requests_total", {"status_class": label}), 1)

    def test_multiple_rotations_between_polls_are_not_lost(self):
        self.tailer.poll()
        for number in range(3):
            self.append(response())
            self.path.rename(self.path.with_name(f"game.log.{number:05}"))
        self.append(response())
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 4)

    def test_truncation_clears_stale_offset(self):
        self.path.write_bytes(response() * 3)
        self.tailer.poll()
        self.path.write_bytes(response())
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)

    def test_malformed_responses_never_pollute_latency_or_labels(self):
        self.tailer.poll()
        bad_lines = [b"not JSON\n", b"[]\n", b"\xff\n", b'{"message":"response sent"}\n']
        bad_lines.extend(response(code) for code in (-1, 600, True, "200"))
        bad_lines.extend(response(milliseconds=value) for value in (-1, "nan", "inf", True, [], "1e999"))
        self.append(b"".join(bad_lines) + response())
        self.tailer.poll()
        self.assertEqual(self.sample("log_parse_errors_total"), len(bad_lines))
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)
        self.assertNotIn(b"content_type=", generate_latest(self.registry))

    def test_oversized_line_is_discarded_once_without_losing_next_record(self):
        cursor = FileCursor()
        cursor.accept(b"x" * MAX_LINE_BYTES, self.metrics)
        cursor.accept(b"more data", self.metrics)
        cursor.accept(b"discarded tail\n" + response(), self.metrics)
        self.assertEqual(self.sample("log_parse_errors_total"), 1)
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)

    def test_deep_json_and_huge_numeric_duration_do_not_stop_ingestion(self):
        self.tailer.poll()
        self.append(b"[" * 2000 + b"0" + b"]" * 2000 + b"\n")
        self.append(response(milliseconds=10 ** 1000) + response())
        self.tailer.poll()
        self.assertEqual(self.sample("log_parse_errors_total"), 2)
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 1)

    def test_exporter_restart_resets_counters_and_skips_prior_history(self):
        self.tailer.poll()
        self.append(response())
        self.tailer.poll()
        registry = CollectorRegistry()
        metrics = GameMetrics(registry)
        restarted = LogTailer(self.path, metrics)
        restarted.poll()
        self.assertEqual(registry.get_sample_value("dog_story_http_request_duration_seconds_count"), 0)
        self.append(response())
        restarted.poll()
        self.assertEqual(registry.get_sample_value("dog_story_http_request_duration_seconds_count"), 1)

    def test_directory_read_failure_preserves_offsets_through_recovery(self):
        self.tailer.poll()
        self.append(response())
        self.tailer.poll()
        with patch.object(Path, "iterdir", side_effect=PermissionError("temporary failure")):
            self.tailer.poll()
            self.tailer.poll()
        self.append(response())
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 2)
        self.assertEqual(self.sample("log_read_errors_total"), 2)

    def test_file_read_failure_preserves_offsets_through_recovery(self):
        self.tailer.poll()
        self.append(response())
        self.tailer.poll()
        with patch.object(Path, "open", side_effect=PermissionError("temporary failure")):
            self.tailer.poll()
            self.tailer.poll()
        self.append(response())
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 2)
        self.assertEqual(self.sample("log_read_errors_total"), 2)

    def test_other_events_are_ignored_except_application_failures(self):
        self.tailer.poll()
        self.append(b'{"message":"server started","data":{}}\n'
                    b'{"message":"request failed","data":{"text":"failure"}}\n')
        self.tailer.poll()
        self.assertEqual(self.sample("request_failures_total"), 1)
        self.assertEqual(self.sample("log_parse_errors_total"), 0)
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 0)

    def test_unrelated_files_are_not_consumed(self):
        self.tailer.poll()
        self.path.with_name("game.log.backup").write_bytes(response())
        self.path.with_name("other.log.00001").write_bytes(response())
        self.tailer.poll()
        self.assertEqual(self.sample("http_request_duration_seconds_count"), 0)


if __name__ == "__main__":
    unittest.main()
