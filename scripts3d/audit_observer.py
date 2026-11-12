"""Audit generated read-only adapter, dependency cleanliness and numerical oracle.

Run after building both build/3d and build/3d-upstream-observer.
This script executes real fixtures and fails on any differing state/statistics byte.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'results3d/obstacles_f0'
OUT.mkdir(parents=True, exist_ok=True)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args):
    return subprocess.check_output(args, cwd=ROOT, text=True, encoding='utf8', errors='replace')


dep = ROOT / '.deps3d/JoltPhysics'
assert run(['git', '-C', str(dep), 'rev-parse', 'HEAD']).strip() == '0373ec0dd762e4bc2f6acdb08371ee84fa23c6db'
assert not run(['git', '-C', str(dep), 'status', '--porcelain']).strip(), 'dirty Jolt dependency'
audit_path = ROOT / 'build/3d/generated/observer/audit.ini'
audit = dict(line.split('=', 1) for line in audit_path.read_text().splitlines() if '=' in line)
ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
project = ET.parse(ROOT / 'build/3d/jolt/Jolt.vcxproj')
sources = [x.attrib['Include'].replace('\\', '/') for x in project.findall('.//m:ClCompile[@Include]', ns)]
for part, relative in [('ContactConstraintManager', 'Jolt/Physics/Constraints'), ('PhysicsSystem', 'Jolt/Physics')]:
    assert sha(dep / relative / (part + '.cpp')) == audit[part + '.input_sha256']
    assert sha(ROOT / 'build/3d/generated/observer' / (part + '.cpp')) == audit[part + '.generated_sha256']
    matched = [p for p in sources if p.endswith('/' + part + '.cpp')]
    assert len(matched) == 1 and '/generated/observer/' in matched[0], matched
    compiler_log = list((ROOT / 'build/3d/jolt/Jolt.dir/Release').rglob('CL.command.1.tlog'))
    assert len(compiler_log) == 1
    log = compiler_log[0].read_text(encoding='utf-16').replace('\\', '/').lower()
    assert '/generated/observer/' + part.lower() + '.cpp' in log
    assert '/.deps3d/joltphysics/' + relative.lower() + '/' + part.lower() + '.cpp' not in log
traces = {}
for mode, build in [('on', '3d'), ('off', '3d'), ('upstream', '3d-upstream-observer')]:
    exe = ROOT / 'build' / build / 'bin/rs3d_contact_observer_tests.exe'
    target = OUT / ('trace_' + mode + '.csv')
    log = run([str(exe), mode, str(target)])
    (OUT / ('observer_' + mode + '.txt')).write_text(log, encoding='utf8')
    traces[mode] = sha(target)
assert len(set(traces.values())) == 1, traces
baseline = json.loads((OUT / 'baseline.json').read_text())
for name, record in baseline['files'].items():
    assert sha(ROOT / name) == record['sha256'], 'pre-existing file modified: ' + name
result = {'upstream': audit['upstream_sha'], 'observer_version': audit['observer_version'],
          'patch_sha256': audit['patch_sha256'], 'trace_sha256': traces,
          'states_and_integer_statistics_identical': True, 'generated_sources_exactly_once': True,
          'dependency_clean': True, 'preexisting_dirty_files_preserved': True}
(OUT / 'observer_audit.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
print(json.dumps(result, indent=2))
