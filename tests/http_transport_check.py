"""Check HTTP framing and connection lifetime against a running Docker stack."""

import json
import os
import socket
import unittest
import urllib.parse
import urllib.request


PORT = int(os.environ.get('GAME_HTTP_PORT', '8080'))
BASE_URL = f'http://127.0.0.1:{PORT}'


class HttpTransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with urllib.request.urlopen(BASE_URL + '/api/v1/maps', timeout=5) as response:
            maps = json.load(response)
        cls.map_path = '/api/v1/maps/' + urllib.parse.quote(maps[0]['id'], safe='')

    def connect(self):
        connection = socket.create_connection(('127.0.0.1', PORT), timeout=5)
        self.addCleanup(connection.close)
        wire = connection.makefile('rb')
        self.addCleanup(wire.close)
        return connection, wire

    def read_headers(self, wire, version=b'HTTP/1.1'):
        status = wire.readline(65536)
        self.assertTrue(status.startswith(version + b' '),
                        f'Unexpected response boundary: {status[:160]!r}')
        headers = {}
        for _ in range(100):
            line = wire.readline(65536)
            if line == b'\r\n':
                return int(status.split()[1]), headers
            self.assertTrue(line, 'Connection closed before the end of headers')
            key, value = line.decode('latin-1').split(':', 1)
            headers[key.lower()] = value.strip()
        self.fail('Response headers did not terminate')

    def test_head_does_not_insert_a_body_before_the_next_response(self):
        # Exercise JSON, file, static error, and API method-error bodies.
        for path in [self.map_path, '/', '/missing-transport-fixture', '/api/v1/maps']:
            with self.subTest(path=path):
                connection, wire = self.connect()
                connection.sendall(
                    f'HEAD {path} HTTP/1.1\r\nHost: localhost\r\n\r\n'.encode()
                    + b'GET /api/v1/maps HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
                self.read_headers(wire)
                status, headers = self.read_headers(wire)
                self.assertEqual(status, 200)
                self.assertIsInstance(json.loads(wire.read(int(headers['content-length']))), list)
                self.assertEqual(wire.read(1), b'', 'Connection: close was ignored')

    def test_head_preserves_the_get_representation_length(self):
        for path in [self.map_path, '/']:
            with self.subTest(path=path):
                connection, wire = self.connect()
                connection.sendall(f'GET {path} HTTP/1.1\r\nHost: localhost\r\n\r\n'.encode())
                status, headers = self.read_headers(wire)
                self.assertEqual(status, 200)
                length = int(headers['content-length'])
                self.assertGreater(length, 0)
                self.assertEqual(len(wire.read(length)), length)
                connection.sendall(
                    f'HEAD {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n'.encode())
                status, head_headers = self.read_headers(wire)
                self.assertEqual(status, 200)
                self.assertEqual(int(head_headers['content-length']), length)
                self.assertEqual(wire.read(1), b'', 'HEAD sent a response body')

    def test_explicit_close_ends_the_connection_after_the_response(self):
        connection, wire = self.connect()
        connection.sendall(b'GET /api/v1/maps HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        status, headers = self.read_headers(wire)
        self.assertEqual(status, 200)
        wire.read(int(headers['content-length']))
        self.assertEqual(wire.read(1), b'')

    def test_http_10_uses_the_requested_version_and_closes_by_default(self):
        for path in ['/api/v1/maps', '/']:
            with self.subTest(path=path):
                connection, wire = self.connect()
                connection.sendall(f'GET {path} HTTP/1.0\r\nHost: localhost\r\n\r\n'.encode())
                status, headers = self.read_headers(wire, b'HTTP/1.0')
                self.assertEqual(status, 200)
                wire.read(int(headers['content-length']))
                self.assertEqual(wire.read(1), b'')


if __name__ == '__main__':
    unittest.main()
