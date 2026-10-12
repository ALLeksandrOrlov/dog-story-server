"""Check production include boundaries and external library dependencies."""
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
        if path.suffix not in {'.hpp', '.cpp'}:
            continue
        for line, text in enumerate(path.read_text().splitlines(), 1):
            match = re.match(r'\s*#\s*include\s*(["<])([^">]+)[">]', text)
            if not match:
                continue
            delimiter, target = match.groups()
            location = f'{path.relative_to(source)}:{line}'
            if delimiter == '"':
                if (not (source / target).is_file() or target.split('/')[0] not in dependencies
                        or target.endswith('.h')):
                    errors.append(f'{location}: forbidden include {target}')
            elif layer in {'domain', 'application'} and target.startswith(
                    ('pqxx/', 'boost/asio', 'boost/beast', 'boost/json', 'boost/serialization', 'filesystem')):
                errors.append(f'{location}: infrastructure dependency {target} in inner layer')
if errors:
    raise SystemExit('\n'.join(errors))
print('Architecture boundaries passed: includes and external dependencies.')
