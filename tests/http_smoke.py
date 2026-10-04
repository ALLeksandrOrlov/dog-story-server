"""Run against a dedicated disposable PostgreSQL database via GAME_TEST_DB_URL."""
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

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
url = os.environ["GAME_TEST_DB_URL"]
name = "smoke-" + uuid.uuid4().hex


def request(path, body=None, token=None, expected=200):
    headers = {}
    if token:
        headers["Authorization"] = "Bearer " + token
    if body is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request("http://127.0.0.1:8080" + path,
                                 data=None if body is None else json.dumps(body).encode(), headers=headers)
    try:
        response = urllib.request.urlopen(req, timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        assert response.status == expected, (path, response.status, expected)
        data = response.read()
        if response.headers.get_content_type() == "application/json":
            return json.loads(data)
        return data


with tempfile.TemporaryDirectory(prefix="game-http-test-") as directory:
    snapshot = Path(directory) / "snapshot"
    log_path = Path(directory) / "server.log"
    process = None
    log = None

    def start():
        global process, log
        log = log_path.open("w")
        process = subprocess.Popen([str(binary), "-c", str(root / "tests/fixtures/contract.json"),
                                    "-w", str(root / "static"), "--state-file", str(snapshot),
                                    "--save-state-period", "100"],
                                   env={**os.environ, "GAME_DB_URL": url}, stdout=log, stderr=log)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("Server startup failed: " + log_path.read_text())
            if '"message":"server started"' in log_path.read_text():
                return
            time.sleep(0.05)
        raise RuntimeError("Timed out waiting for server startup")

    def stop():
        global process, log
        if process is not None:
            process.send_signal(signal.SIGTERM)
            try:
                code = process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise
            process = None
            assert code == 0, "Server failed during shutdown"
        if log is not None:
            log.close()
            log = None

    try:
        start()
        assert len(request("/api/v1/maps")) == 2
        assert b"Courtyard" in request("/")
        joined = request("/api/v1/game/join", {"mapId": "square", "userName": name})
        token = joined["authToken"]
        player_id = str(joined["playerId"])
        request("/api/v1/game/player/action", {"move": "R"}, token)
        request("/api/v1/game/tick", {"timeDelta": 500})
        assert request("/api/v1/game/state", token=token)["players"][player_id]["pos"] == [1, 0]
        assert snapshot.exists()
        stop()
        start()
        restored = request("/api/v1/game/state", token=token)["players"][player_id]
        assert restored["pos"] == [1, 0]
        assert restored["speed"] == [0, 0]  # Existing snapshot behavior.
        request("/api/v1/game/tick", {"timeDelta": 3000})
        assert request("/api/v1/game/state", token=token, expected=401)["code"] == "unknownToken"
        records = request("/api/v1/game/records")
        record = next(item for item in records if item["name"] == name)
        assert record["score"] == 0 and record["playTime"] == 3.0
        stop()
        print("HTTP + PostgreSQL + periodic save + shutdown + restart + retirement: passed.")
    finally:
        stop()
