"""HTTP regression and concurrency checks; requires a dedicated GAME_TEST_DB_URL."""
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
checks = 0
requests = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


def api(path, data=None, token=None):
    global requests
    requests += 1
    headers = {'Content-Type': 'application/json'}
    if token is not None:
        headers['Authorization'] = 'Bearer ' + token
    request = urllib.request.Request('http://127.0.0.1:8080' + path, headers=headers,
                                     data=None if data is None else json.dumps(data).encode())
    try:
        response = urllib.request.urlopen(request, timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read()
        try:
            body = json.loads(body)
        except ValueError:
            body = body.decode(errors='replace')
        return response.status, body


with tempfile.TemporaryDirectory(prefix='game-api-edge-') as directory:
    temp = Path(directory)
    config = json.loads((root / 'tests/fixtures/contract.json').read_text())
    config['dogRetirementTime'] = 60
    config['defaultDogSpeed'] = 3
    config['lootGeneratorConfig'] = {'period': 1, 'probability': 0}
    config_file = temp / 'config.json'
    config_file.write_text(json.dumps(config))
    public = temp / 'public'
    public.mkdir()
    (temp / 'private.txt').write_text('private-content-marker')
    (public / 'index.html').symlink_to(temp / 'private.txt')
    with (temp / 'server.log').open('w') as log:
        process = subprocess.Popen([str(binary), '-c', str(config_file), '-w', str(public)],
                                   env={**os.environ, 'GAME_DB_URL': os.environ['GAME_TEST_DB_URL']},
                                   stdout=log, stderr=log)
        try:
            for _ in range(100):
                if process.poll() is not None:
                    raise RuntimeError((temp / 'server.log').read_text())
                try:
                    if api('/api/v1/maps')[0] == 200:
                        break
                except OSError:
                    pass
                time.sleep(0.05)
            else:
                raise RuntimeError('Server startup timeout')
            check(api('/')[0] == 400, 'Directory index symlink escaped static root')
            check(api('/../private.txt')[0] == 400, 'Parent path escaped static root')
            check(api('/%2e%2e/private.txt')[0] == 400, 'Encoded parent path escaped static root')
            for target in ['/api/v1/maps/%', '/api/v1/maps/%0', '/api/v1/maps/%GG',
                           '/api/v1/maps/%00', '/api/v1/maps/%2z']:
                check(api(target)[0] == 400, 'Malformed request target was accepted: ' + target)
            check(api('/api/v1/maps/%73quare')[0] == 200, 'Valid URL decoding broke')
            token = api('/api/v1/game/join', {'mapId': 'square', 'userName': 'Edge'})[1]['authToken']
            before = api('/api/v1/game/state', token=token)[1]
            for delta in [-1, -2147483649, 2147483648, 4294967296, 1.5, '10', None, True]:
                check(api('/api/v1/game/tick', {'timeDelta': delta})[0] == 400, 'Invalid tick accepted')
                check(api('/api/v1/game/state', token=token)[1] == before, 'Rejected tick changed state')
            for query in ['start=-1', 'maxItems=-1', 'maxItems=101']:
                check(api('/api/v1/game/records?' + query)[0] == 400, 'Invalid pagination accepted')
            check(api('/api/v1/game/records-not-an-endpoint')[0] == 400, 'Records prefix routed as endpoint')

            def join(i):
                return api('/api/v1/game/join', {'mapId': 'square', 'userName': f'Concurrent-{i}'})
            with ThreadPoolExecutor(max_workers=12) as pool:
                joined = list(pool.map(join, range(48)))
            check(all(status == 200 for status, _ in joined), 'Concurrent joins failed')
            check(len({body['playerId'] for _, body in joined}) == 48, 'Duplicate player IDs')
            check(len({body['authToken'] for _, body in joined}) == 48, 'Duplicate player tokens')
            check(len(api('/api/v1/game/players', token=token)[1]) == 49, 'Concurrent joins lost players')
            api('/api/v1/game/player/action', {'move': 'R'}, token)
            def advance(_):
                return api('/api/v1/game/tick', {'timeDelta': 10})
            with ThreadPoolExecutor(max_workers=12) as pool:
                ticks = list(pool.map(advance, range(40)))
            check(all(status == 200 for status, _ in ticks), 'Concurrent ticks failed')
            position = api('/api/v1/game/state', token=token)[1]['players']['0']['pos']
            check(abs(position[0] - 1.2) < 1e-8, 'Concurrent ticks lost updates')

            # Also verify HTTP framing over one persistent connection.
            with socket.create_connection(('127.0.0.1', 8080), timeout=5) as connection:
                connection.sendall(b'GET /api/v1/maps HTTP/1.1\r\nHost: localhost\r\n\r\n')
                wire = connection.makefile('rb')
                check(b'200' in wire.readline(), 'Raw HTTP request failed')
                headers = {}
                while True:
                    line = wire.readline()
                    if line == b'\r\n':
                        break
                    key, value = line.decode().split(':', 1)
                    headers[key.lower()] = value.strip()
                body = wire.read(int(headers['content-length']))
                check(len(json.loads(body)) == 2, 'HTTP content length does not frame JSON')
            print(f'PASS: {checks} edge/concurrency checks, {requests} HTTP requests.')
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
                raise RuntimeError((temp / 'server.log').read_text())
