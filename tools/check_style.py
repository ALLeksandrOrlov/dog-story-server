"""Format project sources; original regression tests are intentionally excluded."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--fix", action="store_true")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
files = sorted(str(p) for p in (root / "src").rglob("*") if p.suffix in (".cpp", ".h"))
files += sorted(str(p) for p in (root / "tests/support").rglob("*") if p.suffix in (".cpp", ".h"))
files += [str(root / "tests/application_tests.cpp"), str(root / "tests/bug_regression_tests.cpp"), str(root / "tests/architecture_tests.cpp"), str(root / "tests/static_query_tests.cpp")]
flags = ["-i"] if args.fix else ["--dry-run", "--Werror"]
subprocess.run(["clang-format", *flags, *files], check=True)
