"""Check existing regression tests byte-for-byte, including their line endings."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
for manifest_name in ("upstream.sha256.json", "preserved.sha256.json", "review-preserved.sha256.json"):
    manifest = json.loads((root / "tests" / manifest_name).read_text())
    for name, expected in manifest.items():
        actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"Existing test changed: {name}")
    print(f"Verified {len(manifest)} unchanged files from {manifest_name}.")
