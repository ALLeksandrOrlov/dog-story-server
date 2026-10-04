"""Exercise real HTTP endpoints against a dedicated local PostgreSQL database.

Writes diagnostic results without modifying the baseline contract or original tests.
GAME_TEST_DB_URL must point to a disposable test database; records are appended.
"""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve()
OUTPUT = ROOT / '.cache/api-check'
OUTPUT.mkdir(parents=True, exist_ok=True)
DB_URL = os.environ['GAME_TEST_DB_URL']
BASE_CONFIG = json.loads((ROOT / 'tests/fixtures/contract.json').read_text())
RESULTS = []
REQUESTS = 0


def check(name, condition, detail=None):
    result = {'check': name, 'passed': bool(condition)}
    if detail is not None:
        result['detail'] = detail
    RESULTS.append(result)
    print(('PASS' if condition else 'FAIL') + ': ' + name, flush=True)


def api(path, body=None, token=None, method=None, raw=None):
    global REQUESTS
    REQUESTS += 1
    headers = {}
    if token is not None:
        headers['Authorization'] = 'Bearer ' + token
    if body is not None or raw is not None:
        headers['Content-Type'] = 'application/json'
    data = raw if raw is not None else (None if body is None else json.dumps(body).encode())
    req = urllib.request.Request('http://127.0.0.1:8080' + path, data=data, headers=headers, method=method)
    try:
        response = urllib.request.urlopen(req, timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        data = response.read()
        try:
            value = json.loads(data)
        except (ValueError, UnicodeDecodeError):
            value = data.decode(errors='replace')
        return response.status, value


class Server:
    def __init__(self, name, config, automatic=False, snapshot=False):
        self.name = name
        self.config = config
        self.automatic = automatic
        self.snapshot = snapshot
        self.process = None
        self.log = None

    def __enter__(self):
        self.start()
        return self

    def start(self):
        config_path = OUTPUT / (self.name + '.json')
        config_path.write_text(json.dumps(self.config))
        self.log = (OUTPUT / (self.name + '.log')).open('a')
        args = [str(BINARY), '-c', str(config_path), '-w', str(ROOT / 'static')]
        if self.automatic:
            args += ['-t', '50']
        if self.snapshot:
            args += ['--state-file', str(OUTPUT / (self.name + '.state')), '--save-state-period', '100']
        self.process = subprocess.Popen(args, env={**os.environ, 'GAME_DB_URL': DB_URL}, stdout=self.log, stderr=self.log)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    raise RuntimeError('Server failed to start: ' + self.name)
                try:
                    if api('/api/v1/maps')[0] == 200:
                        return
                except (OSError, urllib.error.URLError):
                    pass
                time.sleep(0.05)
            raise RuntimeError('Server startup timeout')
        except BaseException:
            self.stop()
            raise

    def stop(self):
        if self.process is not None:
            if self.process.poll() is None:
                self.process.send_signal(signal.SIGTERM)
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                raise
            code = self.process.returncode
            self.process = None
            self.log.close()
            if code != 0:
                raise RuntimeError('Server failed: ' + self.name)

    def __exit__(self, *_):
        self.stop()


def join(name, map_id='square'):
    status, result = api('/api/v1/game/join', {'mapId': map_id, 'userName': name})
    if status != 200:
        raise RuntimeError(f'Cannot join: {status} {result}')
    return result['authToken'], str(result['playerId'])


def state(token):
    return api('/api/v1/game/state', token=token)


def move(token, direction):
    return api('/api/v1/game/player/action', {'move': direction}, token)


def tick(ms):
    return api('/api/v1/game/tick', {'timeDelta': ms})


suffix = uuid.uuid4().hex[:10]
config = {**BASE_CONFIG, 'dogRetirementTime': 60.0}
with Server('http-' + suffix, config):
    check('GET maps', api('/api/v1/maps')[0] == 200)
    check('GET map description', api('/api/v1/maps/square')[1]['id'] == 'square')
    check('Unknown map: 404', api('/api/v1/maps/missing')[0] == 404)
    check('Wrong method: 405', api('/api/v1/maps', method='DELETE')[0] == 405)
    check('Static client', 'Courtyard' in api('/')[1])
    check('Empty player name: 400', api('/api/v1/game/join', {'mapId': 'square', 'userName': ''})[0] == 400)
    check('Malformed join JSON: 400', api('/api/v1/game/join', raw=b'{broken')[0] == 400)
    check('No token: 401', state(None)[0] == 401)
    check('Malformed token: invalidToken', state('oops')[1]['code'] == 'invalidToken')
    check('Unknown token: unknownToken', state('f' * 32)[1]['code'] == 'unknownToken')
    alice, alice_id = join('Alice-' + suffix)
    bob, _ = join('Bob-' + suffix)
    other, _ = join('Other-' + suffix, 'other')
    check('Unique authorization tokens', len({alice, bob, other}) == 3 and len(alice) == 32)
    check('Same-map players share session', len(api('/api/v1/game/players', token=alice)[1]) == 2)
    check('Other-map session is isolated', len(api('/api/v1/game/players', token=other)[1]) == 1)
    move(alice, 'R')
    tick(1000)
    check('Movement: 2 units in 1 second', state(alice)[1]['players'][alice_id]['pos'] == [2, 0])
    move(alice, '')
    tick(500)
    check('Stop preserves position', state(alice)[1]['players'][alice_id]['pos'] == [2, 0])
    move(alice, 'U')
    tick(1000)
    player = state(alice)[1]['players'][alice_id]
    check('Road boundary stops movement', player['pos'] == [2, -0.4] and player['speed'] == [0, 0])
    check('Invalid direction: 400', move(alice, 'X')[0] == 400)
    check('Invalid tick JSON: 400', api('/api/v1/game/tick', {'timeDelta': '100'})[0] == 400)
    check('Records limit >100: 400', api('/api/v1/game/records?maxItems=101')[0] == 400)
    check('Unknown route: 400', api('/api/v1/no-such-endpoint')[0] == 400)

with Server('retirement-' + suffix, BASE_CONFIG):
    active_name, idle_name = 'Active-' + suffix, 'Idle-' + suffix
    active, active_id = join(active_name)
    idle, idle_id = join(idle_name)
    move(active, 'R')
    tick(3000)
    active_status, active_state = state(active)
    idle_status, idle_state = state(idle)
    records = api('/api/v1/game/records')[1]
    check('Only idle player retires', active_status == 200 and idle_status == 401,
          {'activePlayerStatus': active_status, 'idlePlayerStatus': idle_status})
    names = [record['name'] for record in records]
    check('Database records the idle player', idle_name in names and active_name not in names,
          {'activeRecorded': active_name in names, 'idleRecorded': idle_name in names})

with Server('snapshot-' + suffix, config, snapshot=True) as server:
    token, player_id = join('Saved-' + suffix)
    move(token, 'R')
    tick(500)
    check('Periodic snapshot created', (OUTPUT / (server.name + '.state')).exists())
    server.stop()
    server.start()
    status, restored = state(token)
    check('Token and position survive restart', status == 200 and restored['players'][player_id]['pos'] == [1, 0])
    check('Snapshot retains original zero-velocity behavior', restored['players'][player_id]['speed'] == [0, 0])

loot_config = {**BASE_CONFIG, 'dogRetirementTime': 60.0,
               'lootGeneratorConfig': {'period': 1.0, 'probability': 1.0},
               'maps': [{**BASE_CONFIG['maps'][0], 'roads': [{'x0': 0, 'y0': 0, 'x1': 10}]}]}
with Server('scoring-' + suffix, loot_config):
    token, player_id = join('Delivery-' + suffix)
    tick(1000)
    check('Loot is generated', len(state(token)[1]['lostObjects']) == 1)
    move(token, 'R')
    tick(5000)
    player = state(token)[1]['players'][player_id]
    check('Collect and deliver parcels earns score', player['score'] == 10 and player['bag'] == [], player)

with Server('automatic-' + suffix, config, automatic=True):
    token, player_id = join('Automatic-' + suffix)
    move(token, 'R')
    deadline = time.monotonic() + 3
    moved = False
    while time.monotonic() < deadline:
        if state(token)[1]['players'][player_id]['pos'][0] > 0:
            moved = True
            break
        time.sleep(0.05)
    check('Automatic clock advances movement', moved)
    check('Automatic mode rejects manual tick', tick(50)[0] == 400)

report = {'requests': REQUESTS, 'checks': len(RESULTS),
          'passed': sum(result['passed'] for result in RESULTS), 'results': RESULTS}
(OUTPUT / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(f"{report['passed']}/{report['checks']} checks passed; {REQUESTS} HTTP requests.")
print('Report:', OUTPUT / 'report.json')
sys.exit(0 if report['passed'] == report['checks'] else 1)
