"""Compare API responses with the recorded baseline, or record from an original build."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "tests"))
from contract_scenarios import scenarios

parser = argparse.ArgumentParser()
parser.add_argument("driver", type=Path)
parser.add_argument("--record", action="store_true")
parser.add_argument("--baseline", action="store_true", help="Check the historical implementation without corrections")
args = parser.parse_args()
expected_path = root / "tests/fixtures/api_contract.json"
observed = {}
for mode, requests in scenarios().items():
    command = [str(args.driver.resolve()), str(root / "tests/fixtures/contract.json"), str(root / "tests/fixtures/static")]
    if mode == "auto":
        command.append("auto")
    run = subprocess.run(command, input="".join(json.dumps(r) + "\n" for r in requests),
                         text=True, capture_output=True, check=True, timeout=30)
    responses = [json.loads(line) for line in run.stdout.splitlines()]
    if len(responses) != len(requests):
        raise SystemExit(f"Incomplete responses in {mode}: {len(responses)} / {len(requests)}")
    observed[mode] = responses
if args.record:
    expected_path.write_text(json.dumps(observed, ensure_ascii=False, indent=2) + "\n")
else:
    expected = json.loads(expected_path.read_text())
    corrections = []
    if not args.baseline:
        update = json.loads((root / "tests/fixtures/api_contract_corrections.json").read_text())
        if hashlib.sha256(expected_path.read_bytes()).hexdigest() != update["baseline_sha256"]:
            raise SystemExit("Historical baseline changed; review the correction manifest")
        corrections = update["corrections"]
        seen = set()
        for correction in corrections:
            mode, index = correction["mode"], correction["index"]
            if (mode, index) in seen:
                raise SystemExit("Duplicate contract correction")
            seen.add((mode, index))
            if scenarios()[mode][index] != correction["request"]:
                raise SystemExit("Corrected scenario changed; review its explicit expected response")
            expected[mode][index] = correction["expected"]
    if expected.keys() != observed.keys():
        raise SystemExit("Scenario groups changed")
    for mode, responses in observed.items():
        if len(expected[mode]) != len(responses):
            raise SystemExit(f"Scenario count changed: {mode}")
        for i, (before, after) in enumerate(zip(expected[mode], responses)):
            if before != after:
                print(f"Contract mismatch {mode} #{i}: {scenarios()[mode][i]}")
                print("Expected:", before)
                print("Actual:  ", after)
                raise SystemExit(1)
    print(f"Matched {sum(map(len, observed.values()))} contract steps ({len(corrections)} explicit bug-fix expectations; historical baseline unchanged).")
