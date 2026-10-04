"""Verify the original client, permitting only the pinned local dependency repair."""
import hashlib
import json
from pathlib import Path

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
    if name.endswith('.html'):
        body = body.replace(b'/js/jquery-3.6.0.min.js',
                            b'https://ajax.googleapis.com/ajax/libs/jquery/3.6.0/jquery.min.js')
    if hashlib.sha256(body).hexdigest() != expected:
        raise SystemExit(f'Original client changed beyond the local jQuery URL: {name}')
print('Original client/config preserved except pinned local jQuery URLs; three restored runtime assets verified.')
