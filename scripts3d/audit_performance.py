"""Audit the frozen P0b budgets for three real-GUI benchmark repetitions."""
import argparse
import csv
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
parser.add_argument('--legacy-layout', action='store_true', help='audit historical pre-timeline viewport')
args = parser.parse_args()
folder = args.directory
with (folder / 'frames.csv').open(encoding='utf-8', newline='') as stream:
    rows = list(csv.DictReader(stream))
assert len(rows) == 3 and [r['repeat'] for r in rows] == ['1', '2', '3']
for row in rows:
    assert float(row['p95_ms']) <= 33.334, row
    assert float(row['p99_ms']) <= 50, row
    assert 0 < int(row['peak_working_set_bytes']) <= 536870912, row
    assert float(row['input_p95_ms']) <= 50 and float(row['input_max_ms']) <= 250, row
    assert int(row['input_samples']) >= 50, row
    assert (row['width'], row['height'], row['viewport_w'], row['viewport_h']) == ('1280', '720', '1200', '730' if args.legacy_layout else '605'), row
    assert (row['dpi_x'], row['dpi_y'], row['fps_limit']) == ('1.25', '1.25', '0'), row
events = (folder / 'events.txt').read_text(encoding='utf-8')
probes = [float(x) for x in re.findall(r'input_to_draw_ms=([\d.]+) source=scripted_controller', events)]
assert len(probes) >= 150 and max(probes) <= 250
assert 'exit worker_finished=1' in events
report = dict(passed=True, rows=rows, scripted_probes=len(probes), maximum_input_ms=max(probes),
              evidence='software controller-to-EndDrawing, not OS/display latency',
              limits=dict(frame_p95_ms=33.334, frame_p99_ms=50, peak_bytes=536870912,
                          input_p95_ms=50, input_max_ms=250))
(folder / 'audit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(dict(passed=True, scripted_probes=len(probes), maximum_input_ms=max(probes))))
