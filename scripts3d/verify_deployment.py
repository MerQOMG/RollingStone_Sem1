"""Verify a copied Release package from ordinary cmd, without VS environment.

The local font copy is outside the distributable package and is not redistributed.
Uses actual GUI framebuffer capture plus real numerical output, not mock data.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.stdout.reconfigure(encoding="utf-8")
from audit_batch import audit

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
parser.add_argument('--suffix', default='')
args = parser.parse_args()
package = args.package.resolve()
root = package.parent
font = root / '本机测试字体 with spaces.ttf'
shutil.copyfile(Path(os.environ['WINDIR']) / 'Fonts' / 'simhei.ttf', font)
env = os.environ.copy()
removed = []
for name in list(env):
    upper = name.upper()
    if upper.startswith(('VS', 'VSCMD', 'VCTOOLS', 'WINDOWSSDK', 'UNIVERSALCRT', 'UCRTVERSION')) or upper in ('INCLUDE', 'LIB', 'LIBPATH', 'VCINSTALLDIR'):
        removed.append(name)
        del env[name]
env['PATH'] = str(Path(env['WINDIR']) / 'System32')
evidence = root / ('部署证据 with spaces' + args.suffix)
evidence.mkdir(exist_ok=False)


def run(name, flags, expected):
    # Paths are generated locally; reject cmd metacharacters before constructing
    # the single /c argument. list2cmdline handles spaces and embedded quotes.
    values = ['run_3d.cmd', *map(str, flags)]
    assert not any(any(c in value for c in '&|<>^%!\r\n') for value in values)
    command = subprocess.list2cmdline(values)
    completed = subprocess.run(f'"{env["COMSPEC"]}" /d /s /c "{command}"', cwd=package, env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=150)
    (evidence / (name + '.log')).write_bytes(completed.stdout)
    assert completed.returncode == expected, (name, completed.returncode, completed.stdout[-1000:])
    return completed.stdout.decode('utf-8', errors='replace')


results = root / ('输出 数据' + args.suffix)
run('help', ['--help'], 0)
run('gui', ['--config', 'configs3d/user_case_10_20_7_20.ini', '--single', '--exit-after-run',
            '--capture', '--font', font, '--evidence', evidence / '窗口', '--output', results / 'GUI'], 0)
run('headless', ['--config', 'configs3d/batch_throughput.ini', '--batch', '2', '--headless',
                 '--output', results / '数值'], 0)
missing = run('missing-font', ['--font', root / '不存在.ttf', '--duration', '1'], 1)
assert 'CJK font not found' in missing
missing_config = run('missing-config', ['--config', root / '不存在.ini', '--headless'], 1)
assert 'cannot open scene' in missing_config
# A file cannot act as the export root; real NTFS Deny is checked separately.
blocked = root / '不可写文件'
blocked.write_text('deployment fixture', encoding='utf-8')
failure = run('bad-output', ['--config', 'configs3d/batch_throughput.ini', '--batch', '2',
                            '--headless', '--output', blocked], 1)
assert 'status=io_error' in failure
for folder in results.glob('*/*'):
    if folder.is_dir():
        audit(folder)
summary = dict(passed=True, package=str(package), ordinary_shell=env['COMSPEC'], path=env['PATH'],
               removed_developer_variables=removed,
               executable_sha256=hashlib.sha256((package / 'RollingStone3D.exe').read_bytes()).hexdigest(),
               gui_framebuffer=str(evidence / '窗口' / 'framebuffer.png'),
               font='local system-font test copy outside release package; not redistributed',
               exits=dict(help=0, gui=0, headless=0, missing_font=1, missing_config=1, bad_output=1))
(evidence / 'audit.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(summary, ensure_ascii=False, indent=2))
