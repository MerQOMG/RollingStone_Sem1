"""Run actual GUI/headless executables, then compare physical data (not timings).

--exercise-view is an application-owned deterministic diagnostic sequence, not
native mouse automation. Native interaction evidence is recorded separately.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
from audit_batch import audit


def physical_trials(folder):
    with (folder / 'trials.csv').open(encoding='utf-8', newline='') as stream:
        return [{k: v for k, v in row.items() if not k.endswith('_seconds')}
                for row in csv.DictReader(stream)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=Path('build/3d/bin/RollingStone3D.exe'))
    parser.add_argument('--config', type=Path, default=Path('configs3d/with_barrier.ini'))
    parser.add_argument('--output', type=Path, default=Path('results3d/gui_matrix'))
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True, exist_ok=False)
    runs = {}
    cases = [('headless', ['--headless']), ('gui0', ['--preview', '0', '--fps', '60'])]
    cases += [(f'gui2_fps{fps}', ['--preview', '2', '--fps', str(fps), '--exercise-view'])
              for fps in (30, 60, 0)]
    for name, flags in cases:
        target = root / name
        command = [str(args.exe.resolve()), '--config', str(args.config), '--batch', '3',
                   '--output', str(target), *flags]
        if name != 'headless':
            command += ['--exit-after-run', '--evidence', str(root / (name + '_events'))]
        with (root / (name + '.log')).open('wb') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=150)
        assert result.returncode == 0, (name, result.returncode)
        folders = list(target.glob('run_*'))
        assert len(folders) == 1, (name, folders)
        runs[name] = folders[0]
        audit(folders[0])
        print(f'completed {name}', flush=True)
    reference = runs['headless']
    files = ['shape_vertices.csv', 'shape_faces.csv', 'terrain_vertices.csv', 'terrain_faces.csv',
             'trajectory.csv', 'voxel_probability.csv', 'collector_yz.csv', 'collector_summary.csv',
             'projection_xy.csv', 'projection_xz.csv', 'attempt_rejections.csv']
    hashes = {name: hashlib.sha256((reference / name).read_bytes()).hexdigest() for name in files}
    expected = physical_trials(reference)
    for case, folder in runs.items():
        assert physical_trials(folder) == expected, f'physical trial fields differ: {case}'
        for name, digest in hashes.items():
            assert hashlib.sha256((folder / name).read_bytes()).hexdigest() == digest, (case, name)
        if case.startswith('gui2'):
            events = (root / (case + '_events') / 'events.txt').read_text(encoding='utf-8')
            assert 'phase=2 trial=1' in events and 'phase=2 trial=2' in events
            assert 'phase=3 trial=3' in events, 'missing background phase'
            for action in ['camera_top', 'resize=', 'minimized', 'restored', 'pause sim=', 'resume sim=']:
                assert 'exercise ' + action in events, (case, action)
    report = dict(passed=True, executable_sha256=hashlib.sha256(args.exe.read_bytes()).hexdigest(),
                  runs={key: str(value) for key, value in runs.items()},
                  compared_physical_fields=list(expected[0]), files_sha256=hashes,
                  maximum_trajectory_difference_m=0,
                  evidence='actual executable windows; application-owned view sequence; no native-input claim')
    (root / 'audit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('PASS actual GUI/headless physical parameters, termination, geometry, trajectories and counts')


if __name__ == '__main__':
    main()
