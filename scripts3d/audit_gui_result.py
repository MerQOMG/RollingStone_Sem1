"""Re-run a saved GUI input headlessly and compare actual numerical records.

Usage: python scripts3d/audit_gui_result.py <GUI run directory> <evidence folder>
Only elapsed timing fields may differ. Display and output paths are not physics.
"""
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def frame_digests(directory):
    # Published block boundaries may differ because GUI preview is paced.
    # Compare the ordered, explicitly serialized frame bytes within each trial.
    hashes, counts, statuses = {}, {}, {}
    with (directory / "motion.bin").open("rb") as motion, (directory / "motion_index.csv").open(encoding="utf-8", newline="") as index:
        assert motion.read(24) == b"RS3DMOT1" + struct.pack("<IIII", 1, 308, 256, 0)
        for row in csv.DictReader(index):
            trial = row["trial_id"]
            if row["kind"] == "trial":
                statuses[trial] = (row["status"], row["error"])
                continue
            count = int(row["frame_count"])
            assert 0 < count <= 256 and int(row["length"]) == 40 + 308 * count
            motion.seek(int(row["offset"]))
            header = struct.unpack("<IIQIIQQ", motion.read(40))
            assert header == (0x314B4C42, 1, int(trial), count, 308, count*308, int(row["checksum"]))
            payload = motion.read(count*308)
            assert len(payload) == count*308
            hashes.setdefault(trial, hashlib.sha256()).update(payload)
            counts[trial] = counts.get(trial, 0) + count
    return {key: dict(frames=counts[key], sha256=value.hexdigest(), status=statuses[key]) for key, value in hashes.items()}


def main():
    gui, evidence = (Path(value).resolve() for value in sys.argv[1:3])
    evidence.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    exe = root / "build/3d/bin/RollingStone3D.exe"
    command = [str(exe), "--config", str(gui / "scene.ini"), "--headless", "--preview", "0",
               "--output", str(evidence / "headless")]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            encoding="utf-8", errors="replace", check=True)
    (evidence / "headless.txt").write_text(result.stdout, encoding="utf-8")
    headless = Path(next(line[7:] for line in result.stdout.splitlines() if line.startswith("output=")))
    identical = {}
    for path in gui.glob("*.csv"):
        if path.name in ("trials.csv", "motion_index.csv"):
            continue
        other = headless / path.name
        assert path.read_bytes() == other.read_bytes(), path.name
        identical[path.name] = digest(path)
    frames = frame_digests(gui)
    assert frames == frame_digests(headless), "motion frame values, order or terminal status differ"
    rows = []
    for directory in (gui, headless):
        with (directory / "trials.csv").open(encoding="utf-8", newline="") as stream:
            rows.append([{key: value for key, value in row.items() if not key.endswith("_seconds")}
                         for row in csv.DictReader(stream)])
    assert rows[0] == rows[1], "trial inputs, geometry, states, statuses, counts or budgets differ"
    audit = dict(gui=str(gui), headless=str(headless), command=command,
                 executable_sha256=digest(exe), identical_files=identical,
                 identical_motion_frames=frames,
                 trial_rows=len(rows[0]), all_non_timing_trial_columns_identical=True)
    (evidence / "gui_headless_identity.json").write_text(json.dumps(audit, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"PASS GUI preview=2 vs headless preview=0: {len(rows[0])} trials, {len(identical)} files byte-identical")


if __name__ == "__main__":
    main()
