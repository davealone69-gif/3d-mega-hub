#!/usr/bin/env python3
"""Identity, cross-sections, vertex-weighted PCA and mirror symmetry of a GLB (glTF 2.0).

Reproduces the numbers in docs/AVATAR_GEOMETRY_EVIDENCE.md. Needs numpy; --candidates also needs scipy.

  python3 tools/glb_symmetry_sections.py model.glb
  python3 tools/glb_symmetry_sections.py model.glb --yaw 6.0472 --mirror
  python3 tools/glb_symmetry_sections.py model.glb --candidates 0.004

What is measured:
  * file size and SHA-256;
  * triangles, and unique triangle-referenced world-space positions;
  * height H = Ymax - Ymin over those positions;
  * vertex-weighted PCA major axis in the XZ plane (every unique position counts once);
  * X and Z spans of the triangle/plane intersection at Y = Ymin + f*H, divided by H,
    for the model as stored and after a yaw of +angle and -angle about Y
    (x' = cos*x + sin*z, z' = -sin*x + cos*z);
  * --mirror: share of positions whose mirror image across X = 0 falls in an occupied 1 mm cell
    or one of its 26 neighbours, for several yaw angles (locates the symmetry plane);
  * --candidates TOL: size of the left/right sets and of the candidate graph that a one-to-one
    mirror matcher would work on at tolerance TOL metres.

Node transforms are applied to unskinned meshes. For a skinned mesh glTF ignores the node
transform, so its vertices are used as stored.
"""
import argparse
import hashlib
import json
import struct

import numpy as np

COMPONENT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def load(path):
    raw = open(path, "rb").read()
    magic, version, length = struct.unpack_from("<4sII", raw, 0)
    if magic != b"glTF" or version != 2 or length != len(raw):
        raise ValueError("not a valid GLB 2.0 file")
    doc, binary, offset = None, None, 12
    while offset < length:
        size, kind = struct.unpack_from("<II", raw, offset)
        offset += 8
        if kind == 0x4E4F534A:
            doc = json.loads(raw[offset:offset + size])
        elif kind == 0x004E4942:
            binary = memoryview(raw)[offset:offset + size]
        offset += size
    return raw, doc, binary


def accessor(doc, binary, index):
    a = doc["accessors"][index]
    view = doc["bufferViews"][a["bufferView"]]
    width, dtype = WIDTH[a["type"]], np.dtype(COMPONENT[a["componentType"]])
    start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
    stride = view.get("byteStride", 0) or dtype.itemsize * width
    if stride == dtype.itemsize * width:
        return np.frombuffer(binary, dtype=dtype, count=a["count"] * width, offset=start).reshape(a["count"], width)
    rows = np.frombuffer(binary, dtype=np.uint8, count=(a["count"] - 1) * stride + dtype.itemsize * width, offset=start)
    index_grid = np.arange(a["count"])[:, None] * stride + np.arange(dtype.itemsize * width)[None, :]
    return rows[index_grid].copy().view(dtype).reshape(a["count"], width)


def node_matrix(node):
    if "matrix" in node:
        return np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    rotation = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    matrix = np.eye(4)
    matrix[:3, :3] = rotation @ np.diag(node.get("scale", [1, 1, 1]))
    matrix[:3, 3] = node.get("translation", [0, 0, 0])
    return matrix


def triangles(doc, binary):
    nodes = doc.get("nodes", [])
    parent = {child: i for i, node in enumerate(nodes) for child in node.get("children", [])}

    def world(i):
        matrix = node_matrix(nodes[i])
        while i in parent:
            i = parent[i]
            matrix = node_matrix(nodes[i]) @ matrix
        return matrix

    out, skinned = [], 0
    for i, node in enumerate(nodes):
        if "mesh" not in node:
            continue
        matrix = np.eye(4) if "skin" in node else world(i)
        skinned += 1 if "skin" in node else 0
        for primitive in doc["meshes"][node["mesh"]]["primitives"]:
            positions = accessor(doc, binary, primitive["attributes"]["POSITION"]).astype(np.float64)
            if not np.allclose(matrix, np.eye(4)):
                positions = positions @ matrix[:3, :3].T + matrix[:3, 3]
            indices = (accessor(doc, binary, primitive["indices"]).reshape(-1).astype(np.int64)
                       if "indices" in primitive else np.arange(len(positions)))
            out.append(positions[indices].reshape(-1, 3, 3))
    return np.concatenate(out), skinned


def yaw(points, degrees):
    c, s = np.cos(np.radians(degrees)), np.sin(np.radians(degrees))
    turned = points.copy()
    turned[..., 0] = c * points[..., 0] + s * points[..., 2]
    turned[..., 2] = -s * points[..., 0] + c * points[..., 2]
    return turned


def spans(tris, y, height):
    d = tris[:, :, 1] - y
    pieces = []
    for a, b in ((0, 1), (1, 2), (2, 0)):
        da, db = d[:, a], d[:, b]
        crossing = (da * db) < 0
        t = da[crossing] / (da[crossing] - db[crossing])
        pieces.append(tris[crossing, a] + (tris[crossing, b] - tris[crossing, a]) * t[:, None])
        pieces.append(tris[da == 0, a])
    p = np.concatenate(pieces)
    return (p[:, 0].max() - p[:, 0].min()) / height, (p[:, 2].max() - p[:, 2].min()) / height


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("glb")
    parser.add_argument("--yaw", type=float, default=6.0472, help="yaw in degrees for the rotated columns")
    parser.add_argument("--mirror", action="store_true", help="locate the symmetry plane with a 1 mm cell test")
    parser.add_argument("--candidates", type=float, metavar="TOL", help="mirror-matcher candidate graph at TOL metres")
    args = parser.parse_args()

    raw, doc, binary = load(args.glb)
    print(f"file={args.glb} bytes={len(raw)} sha256={hashlib.sha256(raw).hexdigest()}")
    tris, skinned = triangles(doc, binary)
    referenced = tris.reshape(-1, 3)
    unique = np.unique(referenced.astype(np.float32), axis=0).astype(np.float64)
    ymin, ymax = referenced[:, 1].min(), referenced[:, 1].max()
    height = ymax - ymin
    print(f"skinned mesh nodes={skinned} triangles={len(tris)} unique world-space positions={len(unique)}")
    print(f"Ymin={ymin:.6f} Ymax={ymax:.6f} H={height:.6f}")

    xz = unique[:, [0, 2]] - unique[:, [0, 2]].mean(axis=0)
    values, vectors = np.linalg.eigh(np.cov(xz.T))
    major = vectors[:, np.argmax(values)]
    angle = np.degrees(np.arctan2(major[1], major[0]))
    print(f"vertex-weighted PCA major axis in XZ: {angle:+.5f} deg (modulo 180: {angle % 180:+.5f} or {angle % 180 - 180:+.5f})")

    plus, minus = yaw(tris, args.yaw), yaw(tris, -args.yaw)
    print(f"Y/H   stored X/H  Z/H      | yaw +{args.yaw} X/H  Z/H  | yaw -{args.yaw} X/H  Z/H")
    for fraction in (0.10, 0.25, 0.50, 0.75, 0.90):
        y = ymin + fraction * height
        a, b, c = spans(tris, y, height), spans(plus, y, height), spans(minus, y, height)
        print(f"{fraction:.2f}  {a[0]:.6f}  {a[1]:.6f} | {b[0]:.6f}  {b[1]:.6f} | {c[0]:.6f}  {c[1]:.6f}")

    if args.mirror:
        rng = np.random.default_rng(1)
        neighbours = [(dx, dy, dz) for dx in (-1, 0, 1) for dy in (-1, 0, 1) for dz in (-1, 0, 1)]
        print("yaw(deg)  share of positions whose mirror image lands in an occupied 1 mm cell or a neighbour")
        for degrees in (-args.yaw, -1.0, -0.5, 0.0, 0.5, 1.0, args.yaw):
            turned = yaw(unique, degrees)
            occupied = set(map(tuple, np.floor(turned / 0.001).astype(np.int64)))
            sample = turned[rng.choice(len(turned), size=min(60000, len(turned)), replace=False)].copy()
            sample[:, 0] = -sample[:, 0]
            cells = np.floor(sample / 0.001).astype(np.int64)
            hits = sum(1 for k in cells if any((k[0] + o[0], k[1] + o[1], k[2] + o[2]) in occupied for o in neighbours))
            print(f"{degrees:+8.4f}  {100.0 * hits / len(cells):6.2f}%")

    if args.candidates:
        from scipy.spatial import cKDTree
        left, right = unique[unique[:, 0] < 0], unique[unique[:, 0] > 0]
        mirrored = left.copy()
        mirrored[:, 0] = -mirrored[:, 0]
        graph = cKDTree(mirrored).sparse_distance_matrix(cKDTree(right), args.candidates, output_type="coo_matrix").tocsr()
        left_without = int((np.diff(graph.indptr) == 0).sum())
        right_without = int(len(right) - len(np.unique(graph.indices)))
        print(f"left(x<0)={len(left)} right(x>0)={len(right)} on plane={int((unique[:, 0] == 0).sum())} "
              f"count difference={abs(len(left) - len(right))}")
        print(f"candidate pairs within {args.candidates * 1000:.1f} mm: {graph.nnz}; "
              f"positions with no candidate at all: left={left_without} right={right_without}")
        most_pairs = min(len(left) - left_without, len(right) - right_without)
        print(f"a one-to-one matcher can form at most {most_pairs} pairs, so at least "
              f"{len(left) + len(right) - 2 * most_pairs} of {len(left) + len(right)} IDs stay unmatched")


if __name__ == "__main__":
    main()
