"""Exercise the running Compose stack, including snapshot and database persistence.

Requires the default automatic clock and original config (15 second retirement).
Creates uniquely named test players and restarts only this project's game service.
"""
import json
import os
from pathlib import Path
import subprocess
import time
import urllib.error
import urllib.request
import uuid

root = Path(__file__).resolve().parents[1]
base = 'http://127.0.0.1:' + os.environ.get('GAME_HTTP_PORT', '8080')
checks = 0


def check(condition, message):
    global checks
    if not condition:
        raise AssertionError(message)
    checks += 1


def request(path, data=None, token=None):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(base + path, headers=headers,
        data=None if data is None else json.dumps(data).encode())
    try:
        response = urllib.request.urlopen(req, timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read()
        if response.headers.get_content_type() == 'application/json':
            body = json.loads(body)
        return response.status, body


def ready():
    for _ in range(100):
        try:
            if request('/api/v1/maps')[0] == 200:
                return
        except OSError:
            pass
        time.sleep(.2)
    raise RuntimeError('Container did not become ready')


def restart():
    subprocess.run(['docker', 'compose', 'restart', 'game'], cwd=root, check=True)
    ready()


ready()
status, maps = request('/api/v1/maps')
check(status == 200 and [m['id'] for m in maps] == ['map1', 'town', 'map3'], 'Original maps')
check(request('/?')[0] == 200, 'Query on homepage')
check(request('/js/jquery-3.6.0.min.js')[0] == 200, 'Local jQuery')
for resource in ['/assets/key.obj', '/assets/wallet.obj']:
    check(request(resource)[0] == 200, 'Missing model ' + resource)
prefix = 'docker-' + uuid.uuid4().hex[:12]
joined = []
for index, entry in enumerate(maps):
    status, player = request('/api/v1/game/join', {'mapId': entry['id'], 'userName': prefix + '-' + str(index)})
    check(status == 200, 'Join ' + entry['id'])
    joined.append(player)
check(len({p['authToken'] for p in joined}) == 3, 'Unique tokens')
for index, player in enumerate(joined):
    status, players = request('/api/v1/game/players', token=player['authToken'])
    names = {p['name'] for p in players.values()}
    check(status == 200 and prefix + '-' + str(index) in names and
          not any(prefix + '-' + str(other) in names for other in range(3) if other != index),
          'Session isolation')
alice = joined[0]
token = alice['authToken']
player_id = str(alice['playerId'])
check(request('/api/v1/game/player/action', {'move': 'R'}, token)[0] == 200, 'Start movement')
time.sleep(.3)
state = request('/api/v1/game/state', token=token)[1]['players'][player_id]
check(state['pos'][0] > 0 and state['speed'] == [4, 0], 'Automatic clock and original map speed')
check(request('/api/v1/game/player/action', {'move': ''}, token)[0] == 200, 'Stop movement')
position = request('/api/v1/game/state', token=token)[1]['players'][player_id]['pos']
check(request('/api/v1/game/player/action', {'move': 'bad'}, token)[0] == 400, 'Invalid movement')
check(request('/api/v1/game/tick', {'timeDelta': 50})[0] == 400, 'Manual tick rejected in auto mode')
check(request('/api/v1/game/state')[0] == 401, 'Authorization required')
restart()
status, restored = request('/api/v1/game/state', token=token)
check(status == 200 and restored['players'][player_id]['pos'] == position, 'Snapshot and token survive container restart')
for _ in range(100):
    if request('/api/v1/game/state', token=token)[0] == 401:
        break
    time.sleep(.2)
else:
    raise AssertionError('Idle player did not retire')
records = request('/api/v1/game/records')[1]
record = next((r for r in records if r['name'] == prefix + '-0'), None)
check(record is not None and record['playTime'] >= 15, 'Retirement persisted in PostgreSQL')
restart()
check(any(r['name'] == prefix + '-0' for r in request('/api/v1/game/records')[1]), 'Records survive another server restart')
print(f'PASS: {checks} Docker deployment checks, original maps, real PostgreSQL and two container restarts.')
