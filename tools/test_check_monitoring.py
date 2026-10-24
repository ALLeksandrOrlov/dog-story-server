"""Exercise datasource readiness against transient and persistent HTTP failures."""

from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import threading
import unittest
from unittest.mock import patch

import check_monitoring


class DatasourceReadinessTests(unittest.TestCase):
    def setUp(self):
        self.responses = []
        self.requests = []
        case = self

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                case.requests.append((self.path, self.headers.get("Authorization")))
                status, body = case.responses[min(len(case.requests) - 1, len(case.responses) - 1)]
                data = json.dumps(body).encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, *_):
                pass

        self.server = HTTPServer(("127.0.0.1", 0), Handler)
        self.worker = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.worker.start()
        self.addCleanup(self.stop_server)
        endpoint = "http://127.0.0.1:" + str(self.server.server_port)
        patcher = patch.object(check_monitoring, "GRAFANA", endpoint)
        patcher.start()
        self.addCleanup(patcher.stop)

    def stop_server(self):
        self.server.shutdown()
        self.worker.join()
        self.server.server_close()

    def test_waits_for_plugin_registration(self):
        self.responses = [(404, {"message": "plugin not registered"}), (200, {"status": "OK"})]

        check_monitoring.wait_for_datasource("dog-story-prometheus", "Basic dGVzdDp0ZXN0", timeout=5)

        self.assertEqual(self.requests, [
            ("/api/datasources/uid/dog-story-prometheus/health", "Basic dGVzdDp0ZXN0"),
            ("/api/datasources/uid/dog-story-prometheus/health", "Basic dGVzdDp0ZXN0"),
        ])

    def test_waits_for_healthy_upstream(self):
        self.responses = [(200, {"status": "ERROR"}), (200, {"status": "OK"})]

        check_monitoring.wait_for_datasource("dog-story-prometheus", "Basic dGVzdDp0ZXN0", timeout=5)

        self.assertEqual(len(self.requests), 2)

    def test_persistent_failure_keeps_http_diagnostics(self):
        self.responses = [(404, {"message": "plugin not registered"})]

        with self.assertRaisesRegex(AssertionError, "Timed out: Grafana datasource") as failure:
            check_monitoring.wait_for_datasource("dog-story-prometheus", "Basic dGVzdDp0ZXN0", timeout=0.1)

        self.assertIsInstance(failure.exception.__cause__, AssertionError)
        self.assertIn("404", str(failure.exception.__cause__))
        self.assertIn("plugin not registered", str(failure.exception.__cause__))


if __name__ == "__main__":
    unittest.main()
