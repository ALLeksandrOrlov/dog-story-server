"""Verify client resources against runtime and source manifests with explicit asset normalization."""
import hashlib
import json
from pathlib import Path

# Exact UI labels normalized before comparing against the source manifest.
UI_LABELS = {
    "static/index.html": {
        "Player name": "\u0418\u043c\u044f \u0438\u0433\u0440\u043e\u043a\u0430",
        "Join game": "\u0412\u0445\u043e\u0434"
    },
    "static/hall_of_fame.html": {
        "New game": "\u041d\u043e\u0432\u0430\u044f \u0438\u0433\u0440\u0430",
        "Loading...": "\u0417\u0430\u0433\u0440\u0443\u0437\u043a\u0430...",
        "Name": "\u0418\u043c\u044f",
        "Score": "\u041e\u0447\u043a\u0438",
        "Play time": "\u0412\u0440\u0435\u043c\u044f \u0432 \u0438\u0433\u0440\u0435"
    },
    "static/load_simulator.html": {
        "This page generates load on the server...": "\u042d\u0442\u0430 \u0441\u0442\u0440\u0430\u043d\u0438\u0446\u0430 \u0441\u043e\u0437\u0434\u0430\u0451\u0442 \u043d\u0430\u0433\u0440\u0443\u0437\u043a\u0443 \u043d\u0430 \u0441\u0435\u0440\u0432\u0435\u0440...",
        "Requests per second (1 to 100):": "\u0427\u0430\u0441\u0442\u043e\u0442\u0430 \u0437\u0430\u043f\u0440\u043e\u0441\u043e\u0432 \u0432 \u0441\u0435\u043a\u0443\u043d\u0434\u0443 (\u043e\u0442 1 \u0434\u043e 100):"
    }
}

root = Path(__file__).resolve().parents[1]
original = json.loads((root / 'tests/original-client.sha256.json').read_text())
runtime = json.loads((root / 'tests/client-runtime.sha256.json').read_text())
added = {'static/js/jquery-3.6.0.min.js', 'static/assets/key.obj', 'static/assets/wallet.obj'}
if set(runtime) != set(original) | added:
    raise SystemExit('Unexpected changes in the client file set')
actual = {str(p.relative_to(root)) for p in (root / 'static').rglob('*') if p.is_file()}
if actual != {name for name in runtime if name.startswith('static/')}:
    raise SystemExit('Static directory does not match its manifest')
for name, expected in runtime.items():
    if hashlib.sha256((root / name).read_bytes()).hexdigest() != expected:
        raise SystemExit(f'Runtime asset changed: {name}')
for name, expected in original.items():
    body = (root / name).read_bytes()
    for english, source in UI_LABELS.get(name, {}).items():
        body = body.replace(english.encode('utf-8'), source.encode('utf-8'))
    if name.endswith('.html'):
        body = body.replace(b'/js/jquery-3.6.0.min.js',
                            b'https://ajax.googleapis.com/ajax/libs/jquery/3.6.0/jquery.min.js')
    if hashlib.sha256(body).hexdigest() != expected:
        raise SystemExit(f'Client source checksum mismatch after asset normalization: {name}')
print('Verified client resources and map configuration against both manifests.')
