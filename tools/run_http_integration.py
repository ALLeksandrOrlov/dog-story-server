"""Run unchanged backend integration tests with a dedicated static-file fixture.

These older scripts assert the literal 'Courtyard' in the served HTML. Their
HTTP/DB scenarios are independent of the production client. Copying their exact
bytes into an isolated tree lets them test static serving without changing the
original frontend or editing the preserved test files.
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', type=Path)
parser.add_argument('scripts', nargs='*')
args = parser.parse_args()
if any(name not in {'http_smoke.py', 'api_live_check.py'} for name in args.scripts):
    parser.error('Supported scripts: http_smoke.py, api_live_check.py')
root = Path(__file__).resolve().parents[1]
binary = args.binary.resolve()
with tempfile.TemporaryDirectory(prefix='game-http-integration-') as directory:
    isolated = Path(directory)
    shutil.copytree(root / 'tests/fixtures/http-static', isolated / 'static')
    (isolated / 'tests/fixtures').mkdir(parents=True)
    shutil.copy2(root / 'tests/fixtures/contract.json', isolated / 'tests/fixtures/contract.json')
    try:
        for name in args.scripts or ['http_smoke.py', 'api_live_check.py']:
            shutil.copy2(root / 'tests' / name, isolated / 'tests' / name)
            subprocess.run([sys.executable, str(isolated / 'tests' / name), str(binary)], check=True)
    finally:
        output = isolated / '.cache/api-check'
        if output.exists():
            shutil.copytree(output, root / '.cache/api-check', dirs_exist_ok=True)
