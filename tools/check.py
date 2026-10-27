#!/usr/bin/env python3
"""Bounded source, package metadata and formatting checks for the PS5 port."""
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
owned = sorted((root / 'src/prism').glob('*.cpp')) + sorted((root / 'src/prism').glob('*.hpp'))
owned += [root / 'src/main.cpp', root / 'host/preview.cpp', root / 'tests/tests.cpp']
subprocess.run(['clang-format-18', '--dry-run', '--Werror', *map(str, owned)], check=True)
for path in owned:
    text = path.read_text()
    assert 'SPDX-License-Identifier: GPL-3.0-or-later' in text, path
    assert not re.search(r'TODO|FIXME', text), path
param = json.loads((root / 'sce_sys/param.json').read_text())
assert param['titleId'] == 'PPSA99483'
assert param['localizedParameters']['en-US']['titleName'] == 'Prism'
assert re.fullmatch(r'UP9000-PPSA99483_00-[A-Z0-9]{16}', param['contentId'])
assert param['applicationCategoryType'] == 65536
subprocess.run(['bash', str(root / 'tools/validate-assets.sh')], check=True)
for path in (root / 'tools').glob('*.sh'):
    subprocess.run(['bash', '-n', str(path)], check=True)
example = json.loads((root / 'prism.conf.example').read_text())
assert example['host'] == '192.0.2.20'
assert example['token'] == 'REPLACE_WITH_A_HOME_ASSISTANT_LONG_LIVED_TOKEN'
assert not (root / 'dist/PPSA99483/prism.conf').exists(), 'credentials must never be packaged'
print('PS5 format, source, metadata, assets and shell checks passed')
