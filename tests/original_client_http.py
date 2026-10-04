"""Serve the minimally repaired original client and exercise its unchanged API calls."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
base = 'http://127.0.0.1:8080'
config = json.loads((root / 'data/config.json').read_text())
manifest = json.loads((root / 'tests/client-runtime.sha256.json').read_text())


def api(path, data=None, token=None):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    request = urllib.request.Request(base + path, headers=headers,
        data=None if data is None else json.dumps(data).encode())
    with urllib.request.urlopen(request, timeout=5) as response:
        return json.load(response)


with tempfile.TemporaryDirectory(prefix='original-client-http-') as directory:
    with (Path(directory) / 'server.log').open('w') as log:
        process = subprocess.Popen([str(binary), '-c', str(root / 'data/config.json'),
            '-w', str(root / 'static')], env={**os.environ, 'GAME_DB_URL': os.environ['GAME_TEST_DB_URL']},
            stdout=log, stderr=log)
        try:
            for _ in range(100):
                if process.poll() is not None:
                    raise RuntimeError('Server exited before startup')
                try:
                    maps = api('/api/v1/maps')
                    break
                except OSError:
                    time.sleep(.05)
            else:
                raise RuntimeError('Startup timeout')
            assert maps == [{'id': m['id'], 'name': m['name']} for m in config['maps']]
            count = 0
            for name, expected in manifest.items():
                if not name.startswith('static/'):
                    continue
                path = urllib.parse.quote(name[len('static/'):])
                with urllib.request.urlopen(base + '/' + path, timeout=5) as response:
                    assert hashlib.sha256(response.read()).hexdigest() == expected, name
                count += 1
            for entry in config['maps']:
                map_id = entry['id']
                description = api('/api/v1/maps/' + map_id)
                assert description == {key: entry[key] for key in ['id', 'name', 'roads', 'buildings', 'offices', 'lootTypes']}
                joined = api('/api/v1/game/join', {'mapId': map_id, 'userName': 'Original client check'})
                assert set(joined) == {'authToken', 'playerId'}
                token = joined['authToken']
                player = str(joined['playerId'])
                assert api('/api/v1/game/players', token=token)[player]['name'] == 'Original client check'
                assert api('/api/v1/game/player/action', {'move': 'R'}, token) == {}
                api('/api/v1/game/tick', {'timeDelta': 50})
                state = api('/api/v1/game/state', token=token)
                assert set(state) == {'players', 'lostObjects'}
                assert set(state['players'][player]) == {'pos', 'speed', 'dir', 'bag', 'score'}
                assert api('/api/v1/game/player/action', {'move': ''}, token) == {}
            assert isinstance(api('/api/v1/game/records?start=0&maxItems=100'), list)
            subprocess.run([sys.executable, str(root / 'tests/client_resource_check.py')], check=True)
            print(f'PASS: {count} client files match the runtime manifest; original API exercised on all {len(maps)} maps.')
        finally:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
            try:
                code = process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise
            assert code == 0, code
