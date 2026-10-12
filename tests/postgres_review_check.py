"""Verify long player names survive retirement, schema migration and a restart.

Run against the dedicated GAME_TEST_DB_URL database, with no other game server
in this container. Existing records are preserved. The database may already use
the legacy VARCHAR(255) column and the index containing the full player name.
"""
import base64
from contextlib import contextmanager
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import uuid
import zlib

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
database_url = os.environ['GAME_TEST_DB_URL']
base_url = 'http://127.0.0.1:8080'
checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


def request(path, body=None, token=None, expected=200):
    headers = {'Content-Type': 'application/json'}
    if token is not None:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(base_url + path, headers=headers,
        data=None if body is None else json.dumps(body).encode())
    try:
        response = urllib.request.urlopen(req, timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        data = response.read()
        assert response.status == expected, (path, response.status, expected, data[:200])
        return json.loads(data)


def records():
    result = []
    while True:
        page = request(f'/api/v1/game/records?start={len(result)}&maxItems=100')
        result.extend(page)
        if len(page) < 100:
            return result


@contextmanager
def running_server(config_path, snapshot_path, log_path):
    with log_path.open('w') as log:
        process = subprocess.Popen(
            [str(binary), '-c', str(config_path), '-w', str(root / 'static'),
             '--tick-period', '20', '--state-file', str(snapshot_path)],
            env={**os.environ, 'GAME_DB_URL': database_url}, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError('Server startup failed: ' + log_path.read_text())
                if '"message":"server started"' in log_path.read_text():
                    break
                time.sleep(0.02)
            else:
                raise RuntimeError('Server startup timeout: ' + log_path.read_text())
            yield process
        finally:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
            try:
                code = process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise
            if code != 0:
                raise RuntimeError('Server exited unsuccessfully: ' + log_path.read_text())


with tempfile.TemporaryDirectory(prefix='game-postgres-review-') as directory:
    temp = Path(directory)
    config = json.loads((root / 'tests/fixtures/contract.json').read_text())
    config['dogRetirementTime'] = 1.0
    config['lootGeneratorConfig'] = {'period': 1, 'probability': 0}
    config_path = temp / 'config.json'
    config_path.write_text(json.dumps(config))
    snapshot_path = temp / 'state'

    # A repeated letter compresses well enough to hide PostgreSQL's index limit.
    long_name = 'postgres-review-Пёс-🐕-' + uuid.uuid4().hex + '-' + base64.b64encode(os.urandom(3072)).decode()
    check(len(long_name) > 4000, 'Test name does not exceed the legacy length limit')
    check(len(zlib.compress(long_name.encode())) > 3000, 'Test name is too compressible for the index regression')

    with running_server(config_path, snapshot_path, temp / 'first.log') as process:
        previous = records()
        invalid = request('/api/v1/game/join', {'mapId': 'square', 'userName': 'Alice\0Bob'}, expected=400)
        check(invalid == {'code': 'invalidArgument', 'message': 'Invalid name'},
              'NUL name did not use the existing invalid-name response')
        joined = request('/api/v1/game/join', {'mapId': 'square', 'userName': long_name})
        check(joined['playerId'] == 0, 'Rejected NUL name allocated a player identity')
        check(len(joined['authToken']) == 32, 'Join did not return the unchanged token format')
        token = joined['authToken']
        player_id = str(joined['playerId'])
        session_players = request('/api/v1/game/players', token=token)
        check(len(session_players) == 1, 'Rejected NUL name left an extra player in the session')
        check(session_players[player_id]['name'] == long_name, 'Long name was truncated before retirement')
        deadline = time.monotonic() + 5
        saved = None
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('Retirement terminated the server: ' + (temp / 'first.log').read_text())
            saved = next((item for item in records() if item['name'] == long_name), None)
            if saved is not None:
                break
            time.sleep(0.02)
        check(saved is not None, 'Long name did not reach the records repository')
        check(saved['score'] == 0 and saved['playTime'] >= 1.0, 'Persisted retirement result is invalid')
        check(request('/api/v1/game/state', token=token, expected=401)['code'] == 'unknownToken',
              'Retirement did not invalidate authorization')
        check(len(request('/api/v1/maps')) == len(config['maps']), 'Server stopped responding after retirement')
        after_retirement = records()
        check(all(item in after_retirement for item in previous), 'Schema migration lost existing records')

    with running_server(config_path, snapshot_path, temp / 'second.log') as process:
        restored_records = records()
        check(saved in restored_records, 'Long name or record changed after restart')
        check(all(item in restored_records for item in previous), 'Restart lost existing records')
        check(process.poll() is None and len(request('/api/v1/maps')) == len(config['maps']),
              'Server did not remain available after restart')
        check(request('/api/v1/game/state', token=token, expected=401)['code'] == 'unknownToken',
              'Restart resurrected a retired player')

print(f'PASS: {checks} PostgreSQL long-name, retirement, migration-preservation and restart checks.')
