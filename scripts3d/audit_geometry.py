"""Audit saved acceptance evidence; this does not substitute for generating the samples."""
import csv
import json
import math
import pathlib
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "results3d/geometry_5000_v2")
rows = list(csv.DictReader((root / "samples.csv").open(encoding="utf-8")))
assert len(rows) == 5000, f"expected 5000 successful samples, got {len(rows)}"
assert (root / "failures.txt").read_text(encoding="utf-8") == "", "failures must not be omitted"
assert [int(r["sample"]) for r in rows] == list(range(1, 5001)), "missing/duplicate trial"
for k in range(10, 21):
    first = rows[(k-10)*100:(k-9)*100]
    assert all(int(r["k"]) == k and float(r["r"]) == 7 and float(r["V"]) == 20 for r in first)
coverage = set()
for row in rows:
    k, radius, volume = int(row["k"]), float(row["r"]), float(row["V"])
    actual = float(row["actual_V"])
    assert abs(actual-volume) <= max(1e-12*radius**3, 1e-6*volume)
    assert float(row["d_in"]) >= 32*float(row["tau"])*radius
    assert float(row["engine_V_rel"]) <= 1e-4 and float(row["engine_mass_rel"]) <= 1e-4
    assert float(row["engine_COM_m"]) <= 1e-5*radius and float(row["engine_I_rel"]) <= 1e-3
    assert float(row["engine_vertex_m"]) <= 1e-5*radius and float(row["engine_d_in"]) > 0
    assert 1 <= int(row["attempts"]) <= 64 and 1 <= int(row["root_iterations"]) <= 80
    if "max_radius_relative_error" in row:
        assert float(row["max_radius_relative_error"]) <= 1e-8
        assert 0 < float(row["min_radius_over_r"]) <= 1
        assert float(row["canonical_COM_over_r"]) <= 1e-8
    if int(row["sample"]) > 1100:
        ratio = volume/radius**3
        cap = 0.9*(k-2)*math.sin(2*math.pi/(k-2))/3
        band = min(range(3), key=lambda b: abs(ratio-[0.02, (0.02+cap)/2, cap][b]))
        assert abs(ratio-[0.02, (0.02+cap)/2, cap][band]) < 1e-12
        coverage.add((k, radius, band))
assert coverage == {(k, r, band) for k in range(10, 101) for r in (0.2, 2, 7, 10) for band in range(3)}
result = {"passed": True, "samples": len(rows), "user_samples": 1100, "boundary_combinations": len(coverage),
          "final_failures": 0, "retry_trials": sum(int(r["attempts"]) > 1 for r in rows),
          "max_canonical_volume_relative_error": max(abs(float(r["actual_V"])/float(r["V"])-1) for r in rows)}
(root / "coverage_audit.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
print(json.dumps(result))
