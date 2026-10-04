"""Check fatal background errors exit cleanly; uses an isolated PostgreSQL database."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='game-runtime-failure-') as directory:
    # A missing parent makes periodic snapshot writing fail inside an Asio worker.
    snapshot = Path(directory) / 'missing' / 'state'
    result = subprocess.run(
        [str(binary), '-c', str(root / 'tests/fixtures/contract.json'),
         '-w', str(root / 'static'), '-t', '10', '--state-file', str(snapshot),
         '--save-state-period', '5'],
        env={**os.environ, 'GAME_DB_URL': os.environ['GAME_TEST_DB_URL']},
        capture_output=True, text=True, timeout=15,
    )
    assert '"message":"server started"' in result.stdout, result.stdout + result.stderr
    assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
    assert not snapshot.exists()
    assert 'AddressSanitizer' not in result.stderr and 'runtime error:' not in result.stderr, result.stderr
    print('PASS: background snapshot failure exits with code 1, without abort or sanitizer errors.')
