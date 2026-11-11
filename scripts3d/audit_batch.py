"""Independent stdlib CSV/schema/denominator audit; not a physics oracle."""
import argparse
import configparser
import csv
import hashlib
import json
from pathlib import Path


def audit(folder):
    assert not folder.name.endswith('.pending'), 'uncommitted directory'
    assert not (folder / 'INCOMPLETE.txt').exists()
    manifest = configparser.ConfigParser(interpolation=None)
    manifest.read_string('[run]\n' + (folder / 'manifest.ini').read_text(encoding='utf-8'))
    meta = manifest['run']
    n, c = int(meta['n']), int(meta['c'])
    complete = meta['status'] == 'complete'
    assert complete == (meta['complete'] == '1')
    trials = list(csv.DictReader((folder / 'trials.csv').open(encoding='utf-8', newline='')))
    assert [int(r['trial_id']) for r in trials] == list(range(1, len(trials) + 1))
    assert n == len(trials) + int(meta['unstarted'])
    normal = {'resting', 'upstream', 'downstream', 'lateral'}
    assert c == sum(t['reason'] in normal for t in trials)
    assert int(meta['timeouts']) == sum(t['reason'] == 'timeout' for t in trials)
    assert int(meta['failures']) == sum(t['reason'] in {'geometry_failure', 'numeric_failure'} for t in trials)
    assert int(meta['updates']) == sum(int(t['updates']) for t in trials)
    assert int(meta['internal_steps']) == 4 * int(meta['updates'])
    assert all(int(t['internal_steps']) == 4 * int(t['updates']) for t in trials)
    if complete:
        assert c == n and int(meta['failures']) == int(meta['timeouts']) == 0
    sizes = []
    for axis in 'xyz':
        import math
        sizes.append(math.ceil((float(meta[f'grid_upper_{axis}']) - float(meta[f'grid_lower_{axis}'])) / float(meta[f'grid_cell_{axis}'])))
    expected = {'voxel_probability.csv': sizes[0] * sizes[1] * sizes[2],
                'projection_xy.csv': sizes[0] * sizes[1], 'projection_xz.csv': sizes[0] * sizes[2],
                'collector_yz.csv': sizes[1] * sizes[2]}
    rows = {}
    for name, count in expected.items():
        data = list(csv.DictReader((folder / name).open(encoding='utf-8', newline='')))
        assert len(data) == count, (name, len(data), count)
        for row in data:
            assert None not in row and all(v is not None for v in row.values()), name
            hits = int(row['hit_trials'])
            assert 0 <= hits <= c <= n
            assert int(row['N']) == n and int(row['C']) == c
            assert int(row['complete']) == int(complete)
            lo, hi = hits / n, (hits + n - c) / n
            assert abs(float(row['probability_lower']) - lo) <= 1e-15
            assert abs(float(row['probability_upper']) - hi) <= 1e-15
            assert (abs(float(row['probability']) - lo) <= 1e-15) if complete else row['probability'] == ''
        rows[name] = len(data)
    summary = next(csv.DictReader((folder / 'collector_summary.csv').open(encoding='utf-8')))
    collector = list(csv.DictReader((folder / 'collector_yz.csv').open(encoding='utf-8')))
    assert sum(int(r['hit_trials']) for r in collector) + int(summary['first_crossing_not_observed']) + int(summary['first_crossing_outside_yz']) == c
    for filename, id_column, expected_count in [('shape_vertices.csv', 'vertex_id', lambda t: int(t['k'])),
                                                ('shape_faces.csv', 'face_id', lambda t: 2 * int(t['k']) - 4)]:
        observed = {}
        for row in csv.DictReader((folder / filename).open(encoding='utf-8')):
            key = int(row['trial_id']); ordinal = observed.get(key, 0)
            assert int(row[id_column]) == ordinal
            observed[key] = ordinal + 1
        for trial in trials:
            if trial['has_shape'] == '1':
                assert observed[int(trial['trial_id'])] == expected_count(trial)
    hashes = {name: hashlib.sha256((folder / name).read_bytes()).hexdigest()
              for name in (folder / 'FILES_COMPLETE.txt').read_text().splitlines()}
    result = dict(status='pass', batch_status=meta['status'], N=n, C=c, probability_rows=rows,
                  updates=int(meta['updates']), internal_steps=int(meta['internal_steps']), files_sha256=hashes)
    (folder / 'audit.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'files_sha256'}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    audit(parser.parse_args().directory)
