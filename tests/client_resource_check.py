"""Validate served client resources, query handling and all map model dependencies.

Uses HTTP only; does not claim a rendered-browser or WebGL check.
Run against the local demo server after restarting the current build.
"""
from html.parser import HTMLParser
import hashlib
import json
import os
from pathlib import Path
import urllib.parse
import urllib.request

root = Path(__file__).resolve().parents[1]
base = 'http://127.0.0.1:' + os.environ.get('GAME_HTTP_PORT', '8080') + '/'
checks = 0


def fetch(path):
    with urllib.request.urlopen(base + path.lstrip('/'), timeout=10) as response:
        assert response.status == 200, path
        return response.read(), response.headers.get_content_type()


def same(left, right):
    global checks
    assert fetch(left)[0] == fetch(right)[0], (left, right)
    checks += 1


class Scripts(HTMLParser):
    def __init__(self):
        super().__init__()
        self.sources = []

    def handle_starttag(self, tag, attrs):
        if tag == 'script':
            source = dict(attrs).get('src')
            if source:
                self.sources.append(source)


for left, right in [('/', '/?'), ('/', '/?map=town&v=1'),
                    ('/index.html', '/index.html?cache=1'),
                    ('/js/jquery-3.6.0.min.js', '/js/jquery-3.6.0.min.js?v=3.6.0')]:
    same(left, right)
for page in ['index.html', 'game.html', 'hall_of_fame.html']:
    body, content_type = fetch(page)
    assert content_type == 'text/html'
    parser = Scripts()
    parser.feed(body.decode())
    for source in parser.sources:
        assert not urllib.parse.urlparse(source).netloc, (page, 'External script', source)
        content, mime = fetch(source)
        assert mime in {'application/javascript', 'text/javascript'}, (source, mime)
        assert content, source
        checks += 1
    assert parser.sources.count('/js/jquery-3.6.0.min.js') == 1
    checks += 1
maps = json.loads(fetch('api/v1/maps')[0])
for entry in maps:
    description = json.loads(fetch('api/v1/maps/' + entry['id'])[0])
    for loot in description['lootTypes']:
        data, _ = fetch(loot['file'])
        assert b'\nv ' in data and b'\nf ' in data, loot['file']
        checks += 1
manifest = json.loads((root / 'tests/client-runtime.sha256.json').read_text())
for name, expected in manifest.items():
    if name.startswith('static/'):
        data, _ = fetch(urllib.parse.quote(name[len('static/'):]))
        assert hashlib.sha256(data).hexdigest() == expected, name
        checks += 1
print(f'PASS: {checks} HTTP client/resource checks; {len(maps)} maps; no external JavaScript required.')
