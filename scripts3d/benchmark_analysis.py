"""Sequential P1/P2 measurements on the existing Release binaries; no dependencies.

Run with no concurrent builds, numerical tests or GUI benchmarks. The current
obstacles-off/history-off build is the P2 baseline, not a historical timing.
"""
import argparse
import ctypes
from ctypes import wintypes
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time


class Memory(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
            "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
            "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage", "PrivateUsage")]


kernel = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Memory), wintypes.DWORD]


def measure(command, folder):
    folder.mkdir(parents=True, exist_ok=True)
    log = folder / "stdout.txt"
    start = time.perf_counter()
    peak_working = peak_private = 0
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen([str(x) for x in command], stdout=stream, stderr=subprocess.STDOUT)
        handle = kernel.OpenProcess(0x410, False, process.pid)
        if not handle:
            raise OSError("cannot monitor benchmark process")
        try:
            while True:
                memory = Memory()
                memory.cb = ctypes.sizeof(memory)
                if psapi.GetProcessMemoryInfo(handle, ctypes.byref(memory), memory.cb):
                    peak_working = max(peak_working, memory.PeakWorkingSetSize)
                    peak_private = max(peak_private, memory.PrivateUsage)
                if process.poll() is not None:
                    break
                time.sleep(.05)
        finally:
            kernel.CloseHandle(handle)
    result = dict(command=[str(x) for x in command], exit=process.returncode,
                  wall_seconds=time.perf_counter()-start,
                  peak_working_set_bytes=peak_working,
                  sampled_peak_private_bytes=peak_private,
                  executable_sha256=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest())
    (folder / "measurement.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    if process.returncode:
        raise RuntimeError(f"benchmark failed: {log}")
    return result, log.read_text(encoding="utf-8", errors="replace")


def set_field(text, section, key, value):
    match = re.search(rf"(?m)^\[{re.escape(section)}\]\s*$", text)
    if not match:
        return text + f"\n[{section}]\n{key}={value}\n"
    end = text.find("\n[", match.end())
    if end < 0:
        end = len(text)
    block = text[match.start():end]
    if re.search(rf"(?m)^{re.escape(key)}=", block):
        block = re.sub(rf"(?m)^{re.escape(key)}=.*$", f"{key}={value}", block)
    else:
        block += f"\n{key}={value}\n"
    return text[:match.start()] + block + text[end:]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("p1", "p2"))
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    if args.mode == "p1":
        print("P1: 1000 x 60s at explicit 100Hz and auto 50Hz, all-trial display", flush=True)
        result, output = measure([root / "build/3d/bin/rs3d_playback_tests.exe", "--performance", folder / "archives"], folder)
        print(output, end="", flush=True)
        return
    source = (root / "configs3d/obstacles_heatmap_timeline.ini").read_text(encoding="utf-8")
    results = []
    for obstacles, recording in ((False, False), (False, True), (True, False), (True, True)):
        name = f"obstacles_{int(obstacles)}_history_{int(recording)}"
        case = folder / name
        case.mkdir(exist_ok=True)
        config = set_field(source, "obstacles", "enabled", str(obstacles).lower())
        config = set_field(config, "history", "enabled", str(recording).lower())
        config = set_field(config, "history", "interval", "auto")
        scene = case / "input.ini"
        scene.write_text(config, encoding="utf-8")
        print(f"P2 starting {name}: N=100", flush=True)
        result, output = measure([root / "build/3d/bin/RollingStone3D.exe", "--config", scene,
                                  "--headless", "--preview", "0", "--output", case / "runs"], case)
        path = Path(re.search(r"(?m)^output=(.+)$", output).group(1).strip())
        manifest = {}
        for line in (path / "manifest.ini").read_text(encoding="utf-8").splitlines():
            if line.startswith("["):
                break
            key, value = line.split("=", 1)
            manifest[key] = value
        result.update(name=name, directory=str(path), manifest=manifest,
                      disk_bytes=sum(f.stat().st_size for f in path.iterdir() if f.is_file()),
                      motion_bytes=(path / "motion.bin").stat().st_size,
                      timings={key: float(value) for key, value in re.findall(r"(\w+_s)=([\d.eE+-]+)", output)})
        results.append(result)
        (folder / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
        print(json.dumps({key: result[key] for key in ("name", "timings", "disk_bytes", "peak_working_set_bytes")}), flush=True)
    # Recording is a display/output setting. Per-trial numerical columns and all
    # integer counts must stay identical with it enabled or disabled.
    for a, b in ((results[0], results[1]), (results[2], results[3])):
        for filename, keys in (("trials.csv", ("reason", "final_com_x", "final_com_y", "final_com_z")),
                               ("route_heatmap_xy.csv", ("H", "N", "C")),
                               ("barrier_first_contact_heatmap.csv", ("H_b", "B", "M", "unknown"))):
            rows = []
            for case in (a, b):
                with (Path(case["directory"]) / filename).open(encoding="utf-8", newline="") as stream:
                    rows.append([tuple(row[key] for key in keys) for row in csv.DictReader(stream)])
            assert rows[0] == rows[1], (a["name"], filename)
    print("P2 all four actual N=100 cases complete; recording on/off numerical/count checks PASS", flush=True)


if __name__ == "__main__":
    main()
