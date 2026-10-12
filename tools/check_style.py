"""Check or format .cpp and .hpp files in src; exclude test sources."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "src"
FORMATTER_VERSION = "18.1.8"


def include_categories(path):
    """Keep each source's own header after headers from the same component."""
    relative = path.relative_to(SOURCE_ROOT)
    categories = []
    if path.suffix == ".cpp":
        header = relative.with_suffix(".hpp").as_posix()
        local_header = relative.with_suffix(".hpp").name
        categories.append({
            "Regex": '^"(' + re.escape(header) + "|" + re.escape(local_header) + ')"$',
            "Priority": 5,
        })
    if relative.parent != Path("."):
        component = re.escape(relative.parent.as_posix())
        categories.append({"Regex": '^"' + component + "/", "Priority": 4})
    categories.extend([
        {"Regex": '^"[^/]+"$', "Priority": 4},
        {"Regex": "^<(boost|pqxx)/", "Priority": 2},
        {"Regex": "^<", "Priority": 1},
        {"Regex": '^"', "Priority": 3},
    ])
    return categories


def format_source(formatter, path):
    """Use .clang-format with include groups resolved for this source file."""
    style = {
        "BasedOnStyle": "InheritParentConfig",
        "IncludeCategories": include_categories(path),
    }
    original = path.read_bytes().decode("utf-8")
    result = subprocess.run(
        [formatter, "--style=" + json.dumps(style), "--assume-filename=" + str(path)],
        input=original,
        capture_output=True,
        text=True,
        check=True,
        encoding="utf-8",
    )
    return original, result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="format src/**/*.cpp and src/**/*.hpp")
    parser.add_argument("--clang-format", default=os.environ.get("CLANG_FORMAT", "clang-format"))
    args = parser.parse_args()
    version = subprocess.run(
        [args.clang_format, "--version"], capture_output=True, text=True, check=True
    ).stdout
    if not re.search(r"\b" + re.escape(FORMATTER_VERSION) + r"\b", version):
        parser.error("clang-format " + FORMATTER_VERSION + " is required; found " + version.strip())

    files = sorted(path for path in SOURCE_ROOT.rglob("*") if path.suffix in {".cpp", ".hpp"})
    changed = []
    for path in files:
        original, formatted = format_source(args.clang_format, path)
        if original == formatted:
            continue
        changed.append(path)
        if args.fix:
            path.write_bytes(formatted.encode("utf-8"))
        else:
            print("Formatting required: " + str(path.relative_to(ROOT)), file=sys.stderr)

    if changed and not args.fix:
        print("Run tools/check_style.py --fix to apply the project style.", file=sys.stderr)
        return 1
    print(f"Style {'updated' if args.fix else 'verified'}: {len(files)} production files; "
          f"{len(changed)} changed. Tests excluded.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, UnicodeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr, file=sys.stderr)
        raise SystemExit(1)
