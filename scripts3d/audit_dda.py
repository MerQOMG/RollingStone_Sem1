"""Independent exact rational oracle for the C++ DDA's recorded binary-double inputs."""
import csv
from fractions import Fraction
import hashlib
import itertools
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "results3d/dda_oracle.csv")
def fraction(text):
    return Fraction.from_float(float(text))

def intersects(lines, cell, a, b):
    low, high = Fraction(0), Fraction(1)
    low_closed = high_closed = True
    for axis in range(3):
        lower, upper = lines[axis][cell[axis]:cell[axis]+2]
        outer = cell[axis]+2 == len(lines[axis])
        delta = b[axis]-a[axis]
        if delta == 0:
            if not lower <= a[axis] <= upper or (a[axis] == upper and not outer):
                return False
            continue
        if delta > 0:
            lo, hi = (lower-a[axis])/delta, (upper-a[axis])/delta
            lc, hc = True, outer
        else:
            lo, hi = (upper-a[axis])/delta, (lower-a[axis])/delta
            lc, hc = outer, True
        if lo > low:
            low, low_closed = lo, lc
        elif lo == low:
            low_closed &= lc
        if hi < high:
            high, high_closed = hi, hc
        elif hi == high:
            high_closed &= hc
        if low > high:
            return False
    return low < high or (low == high and low_closed and high_closed)

count = 0
with path.open(encoding="utf-8", newline="") as stream:
    for count, row in enumerate(csv.DictReader(stream), 1):
        lines = [[fraction(x) for x in row["grid_"+axis].split(";") if x] for axis in "xyz"]
        a = [fraction(row["a"+axis]) for axis in "xyz"]
        b = [fraction(row["b"+axis]) for axis in "xyz"]
        shape = [len(line)-1 for line in lines]
        expected = {(x*shape[1]+y)*shape[2]+z for x,y,z in itertools.product(*(range(n) for n in shape))
                    if intersects(lines, (x,y,z), a, b)}
        actual = {int(x) for x in row["ids"].split(";") if x}
        if actual != expected:
            raise AssertionError(f"case {count}: actual {sorted(actual)}, exact {sorted(expected)}")
assert count == 4000, count
report = {"passed": True, "segments": count, "oracle": "Python Fraction.from_float; exact rational inequalities",
          "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
path.with_suffix(".audit.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
print(json.dumps(report))
