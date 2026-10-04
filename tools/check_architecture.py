"""Enforce direct include boundaries for the inner layers of the server."""
from pathlib import Path
import re

source = Path(__file__).resolve().parents[1] / 'src'
allowed = {
    'common': {'common'},
    'domain': {'domain', 'common'},
    'application': {'application', 'domain', 'common'},
    'view': {'view', 'application', 'domain', 'common'},
    'controller': {'controller', 'view', 'application', 'domain', 'common'},
}
errors = []
for layer, dependencies in allowed.items():
    for path in (source / layer).rglob('*'):
        if path.suffix not in {'.h', '.cpp'}:
            continue
        for line, text in enumerate(path.read_text().splitlines(), 1):
            match = re.match(r'\s*#\s*include\s*"([^"]+)"', text)
            if not match:
                continue
            target = match[1]
            resolved = source / target
            if not resolved.is_file() or target.split('/')[0] not in dependencies:
                errors.append(f'{path.relative_to(source)}:{line}: forbidden include {target}')
if errors:
    raise SystemExit('\n'.join(errors))
print('Architecture include boundaries: passed (common, domain, application, view, controller).')
