"""Plot actual exported convex meshes, without smoothing or altering vertices.

Requires local matplotlib/numpy only for this optional inspection artifact.
The application and all numerical tests do not depend on Python plotting.
"""
import argparse
import csv
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
parser.add_argument('--start', type=int, default=0)
parser.add_argument('--name', default='gallery.png')
parser.add_argument('--count', type=int, choices=(6, 12), default=12)
args = parser.parse_args()
root = args.directory
vertices, faces = {}, {}
for row in csv.DictReader((root / 'vertices.csv').open()):
    vertices.setdefault(int(row['sample']), []).append([float(row[a]) for a in ('x', 'y', 'z')])
for row in csv.DictReader((root / 'faces.csv').open()):
    faces.setdefault(int(row['sample']), []).append([int(row[a]) for a in ('a', 'b', 'c')])
metadata = list(csv.DictReader((root / 'metrics.csv').open()))
columns, rows = (3, 2) if args.count == 6 else (4, 3)
fig = plt.figure(figsize=(13, 6 if args.count == 6 else 9), facecolor='#f2f1ed', layout='constrained')
fig.suptitle('Actual generated convex rocks — no smoothing, equal axis scale', fontsize=17)
light = np.array([-.4, -.3, 1.0]); light /= np.linalg.norm(light)
for cell in range(args.count):
    index = args.start + cell
    points = np.asarray(vertices[index]); points -= points.mean(axis=0)
    # Rigid alignment only: compare outlines without random overall orientation.
    _, rotation = np.linalg.eigh(points.T @ points)
    rotation = rotation[:, ::-1]
    if np.linalg.det(rotation) < 0: rotation[:, -1] *= -1
    points = points @ rotation
    triangles = points[np.asarray(faces[index])]
    normals = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    brightness = .38 + .58 * np.abs(normals @ light)
    colors = brightness[:, None] * np.array([.76, .68, .54])[None, :]
    ax = fig.add_subplot(rows, columns, cell+1, projection='3d', facecolor='#f2f1ed')
    ax.add_collection3d(Poly3DCollection(triangles, facecolors=colors, edgecolors='#5d594f', linewidths=.4))
    half = np.ptp(points, axis=0).max() * .55
    for setter in (ax.set_xlim, ax.set_ylim, ax.set_zlim): setter(-half, half)
    ax.set_box_aspect((1, 1, 1)); ax.view_init(elev=24, azim=-55); ax.set_axis_off()
    row = metadata[index]
    ax.set_title(f"Sample {index+1} | k={row['k']}  r={float(row['r']):g}  V={float(row['V']):.3g}", fontsize=10)
fig.savefig(root / args.name, dpi=160)
print(root / args.name)
