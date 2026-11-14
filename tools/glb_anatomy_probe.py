#!/usr/bin/env python3
"""Dependency-free GLB anatomy/proportion probe (glTF 2.0).

Usage:
  python3 tools/glb_anatomy_probe.py mylove.glb generated.glb
  python3 tools/glb_anatomy_probe.py mylove.glb generated.glb --json report.json

This is a measurement aid, not an automatic "looks human" judge. It reports
mesh bounds/slice widths and named landmark ratios only when the GLB contains
recognisable node/joint names. Missing landmarks stay missing; no guessed
shoulder, eye, hip, or fingertip positions are fabricated.
"""
import argparse
import json
import math
import re
import struct
import sys
from pathlib import Path

COMPONENTS = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2),
              5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
TYPE_WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
              "MAT2": 4, "MAT3": 9, "MAT4": 16}


def read_glb(path):
    data = Path(path).read_bytes()
    if len(data) < 20:
        raise ValueError("file is too short to be a GLB")
    magic, version, total = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or total != len(data):
        raise ValueError("invalid GLB header, version, or declared total length")
    pos, doc, binary = 12, None, b""
    while pos < total:
        if pos + 8 > total:
            raise ValueError("truncated chunk header")
        size, kind = struct.unpack_from("<I4s", data, pos)
        pos += 8
        end = pos + size
        if end > total:
            raise ValueError("chunk extends past GLB end")
        payload = data[pos:end]
        if kind == b"JSON":
            if doc is not None:
                raise ValueError("multiple JSON chunks")
            doc = json.loads(payload.rstrip(b" \t\r\n\x00").decode("utf-8"))
        elif kind == b"BIN\x00":
            if binary:
                raise ValueError("multiple BIN chunks")
            binary = payload
        pos = end
    if doc is None:
        raise ValueError("missing JSON chunk")
    if doc.get("asset", {}).get("version") != "2.0":
        raise ValueError("asset is not glTF 2.0")
    return doc, binary


def accessor_values(doc, binary, accessor_index):
    accessors = doc.get("accessors", [])
    views = doc.get("bufferViews", [])
    if not isinstance(accessor_index, int) or not 0 <= accessor_index < len(accessors):
        raise ValueError("accessor index out of range")
    a = accessors[accessor_index]
    if "sparse" in a:
        raise ValueError("sparse accessors are not supported by this probe")
    vi = a.get("bufferView")
    if vi is None or not 0 <= vi < len(views):
        raise ValueError("accessor has no valid embedded bufferView")
    view = views[vi]
    if view.get("buffer", 0) != 0:
        raise ValueError("only the embedded GLB buffer is supported")
    ct = a.get("componentType")
    if ct not in COMPONENTS or a.get("type") not in TYPE_WIDTH:
        raise ValueError("unsupported accessor component/type")
    fmt, comp_size = COMPONENTS[ct]
    width = TYPE_WIDTH[a["type"]]
    count = a.get("count", 0)
    if count < 0:
        raise ValueError("negative accessor count")
    element = comp_size * width
    stride = view.get("byteStride", element)
    if stride < element or stride % comp_size:
        raise ValueError("invalid accessor stride")
    start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
    view_end = view.get("byteOffset", 0) + view.get("byteLength", 0)
    last = start if count == 0 else start + (count - 1) * stride + element
    if start < 0 or last > view_end or last > len(binary):
        raise ValueError("accessor reads outside its bufferView/BIN chunk")
    unpack = struct.Struct("<" + fmt * width).unpack_from
    normalized = a.get("normalized", False)
    out = []
    for i in range(count):
        values = unpack(binary, start + i * stride)
        if normalized and ct != 5126:
            if ct in (5120, 5122):
                bits = comp_size * 8
                values = tuple(max(-1.0, v / float((1 << (bits - 1)) - 1)) for v in values)
            else:
                values = tuple(v / float((1 << (comp_size * 8)) - 1) for v in values)
        out.append(values[0] if width == 1 else values)
    return out


def mat_mul(a, b):
    # glTF matrices are column-major; returned list is also column-major.
    return [sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4))
            for c in range(4) for r in range(4)]


def node_matrix(node):
    if "matrix" in node:
        m = node["matrix"]
        if len(m) != 16:
            raise ValueError("node matrix must have 16 values")
        return m
    t = node.get("translation", [0, 0, 0])
    s = node.get("scale", [1, 1, 1])
    q = node.get("rotation", [0, 0, 0, 1])
    x, y, z, w = q
    x2, y2, z2 = x*x, y*y, z*z
    r = [
        1-2*(y2+z2), 2*(x*y+z*w), 2*(x*z-y*w), 0,
        2*(x*y-z*w), 1-2*(x2+z2), 2*(y*z+x*w), 0,
        2*(x*z+y*w), 2*(y*z-x*w), 1-2*(x2+y2), 0,
        t[0], t[1], t[2], 1
    ]
    # Scale the basis columns.
    for i in range(3):
        for row in range(3):
            r[i*4+row] *= s[i]
    return r


def transform_point(m, p):
    x, y, z = p
    w = m[3]*x + m[7]*y + m[11]*z + m[15]
    if abs(w) < 1e-12:
        raise ValueError("node transform produced zero homogeneous w")
    return ((m[0]*x + m[4]*y + m[8]*z + m[12])/w,
            (m[1]*x + m[5]*y + m[9]*z + m[13])/w,
            (m[2]*x + m[6]*y + m[10]*z + m[14])/w)


def side_of(name):
    n = name.lower()
    if re.search(r"(^|[_.\-\s])(left|l)([_.\-\s]|$)", n) or n.endswith("_l"):
        return "left"
    if re.search(r"(^|[_.\-\s])(right|r)([_.\-\s]|$)", n) or n.endswith("_r"):
        return "right"
    return None


LANDMARK_PATTERNS = {
    "head_top": r"head.?top|crown|skull.?top",
    "chin": r"chin|jaw.?tip",
    "neck": r"neck",
    "left_eye": r"eye.*left|left.*eye|eye_l|eyel",
    "right_eye": r"eye.*right|right.*eye|eye_r|eyer",
    "left_head_side": r"left.*temple|temple.*left|head_l",
    "right_head_side": r"right.*temple|temple.*right|head_r",
    "left_shoulder": r"left.*shoulder|shoulder.*left|clavicle_l|upperarm_l",
    "right_shoulder": r"right.*shoulder|shoulder.*right|clavicle_r|upperarm_r",
    "left_hip": r"left.*hip|hip.*left|upleg_l|thigh_l",
    "right_hip": r"right.*hip|hip.*right|upleg_r|thigh_r",
    "left_fingertip": r"left.*(finger.?tip|fingertip)|finger.?tip.*left|index.?distal_l|hand.?tip_l",
    "right_fingertip": r"right.*(finger.?tip|fingertip)|finger.?tip.*right|index.?distal_r|hand.?tip_r",
    "left_ankle": r"left.*ankle|ankle.*left|foot_l",
    "right_ankle": r"right.*ankle|ankle.*right|foot_r",
}


def analyse(path):
    doc, binary = read_glb(path)
    nodes = doc.get("nodes", [])
    parents = {}
    for i, node in enumerate(nodes):
        for child in node.get("children", []):
            if child in parents:
                raise ValueError("node has more than one parent")
            parents[child] = i
    world = [None] * len(nodes)
    visiting = set()

    def world_of(i):
        if not 0 <= i < len(nodes):
            raise ValueError("node index out of range")
        if world[i] is not None:
            return world[i]
        if i in visiting:
            raise ValueError("node hierarchy contains a cycle")
        visiting.add(i)
        local = node_matrix(nodes[i])
        result = mat_mul(world_of(parents[i]), local) if i in parents else local
        visiting.remove(i)
        world[i] = result
        return result

    for i in range(len(nodes)):
        world_of(i)

    points = []
    mesh_instances = 0
    primitive_count = 0
    triangle_count = 0
    for ni, node in enumerate(nodes):
        mi = node.get("mesh")
        if mi is None:
            continue
        if not 0 <= mi < len(doc.get("meshes", [])):
            raise ValueError("node mesh index out of range")
        mesh_instances += 1
        for prim in doc["meshes"][mi].get("primitives", []):
            primitive_count += 1
            if "POSITION" not in prim.get("attributes", {}):
                continue
            positions = accessor_values(doc, binary, prim["attributes"]["POSITION"])
            if prim.get("mode", 4) != 4:
                continue
            idx = accessor_values(doc, binary, prim["indices"]) if "indices" in prim else list(range(len(positions)))
            if len(idx) % 3:
                raise ValueError("triangle primitive index count is not divisible by three")
            if any(not isinstance(v, int) or v < 0 or v >= len(positions) for v in idx):
                raise ValueError("primitive index outside POSITION accessor")
            triangle_count += len(idx) // 3
            wm = world[ni]
            points.extend(transform_point(wm, positions[v]) for v in set(idx))

    if not points:
        raise ValueError("no triangle POSITION data found in the scene nodes")
    if any(not all(math.isfinite(c) for c in p) for p in points):
        raise ValueError("mesh contains non-finite transformed positions")
    mins = [min(p[k] for p in points) for k in range(3)]
    maxs = [max(p[k] for p in points) for k in range(3)]
    dims = [maxs[k] - mins[k] for k in range(3)]
    if min(dims) <= 1e-8:
        raise ValueError("mesh has a degenerate world-space bounding box")
    height = dims[1]
    center = [(mins[k] + maxs[k]) * 0.5 for k in range(3)]

    # Slice widths are descriptive geometry samples, not semantic landmarks.
    slices = {}
    for label, fraction in (("lower_torso", .48), ("waist_region", .58),
                            ("upper_torso", .70), ("shoulder_region", .82),
                            ("head_region", .93)):
        y = mins[1] + height * fraction
        band = [p for p in points if abs(p[1] - y) <= height * .0125]
        if band:
            slices[label] = {"height_fraction": fraction,
                             "width_x_m": max(p[0] for p in band)-min(p[0] for p in band),
                             "depth_z_m": max(p[2] for p in band)-min(p[2] for p in band)}
        else:
            slices[label] = None

    landmarks = {}
    for ni, node in enumerate(nodes):
        name = node.get("name", "")
        if not name:
            continue
        lname = name.lower()
        for key, pattern in LANDMARK_PATTERNS.items():
            if re.search(pattern, lname):
                m = world[ni]
                landmarks.setdefault(key, []).append([m[12], m[13], m[14]])

    # Use the mean only when duplicate matching names agree spatially.
    resolved = {}
    for key, values in landmarks.items():
        if len(values) == 1:
            resolved[key] = values[0]
        else:
            spread = max(math.dist(a, b) for a in values for b in values)
            if spread <= height * .02:
                resolved[key] = [sum(v[k] for v in values)/len(values) for k in range(3)]

    ratios = {"head_height_over_body_height": None,
              "shoulder_width_over_hip_width": None,
              "interpupillary_over_head_width": None,
              "arm_span_over_body_height": None}
    if "head_top" in resolved and "chin" in resolved:
        ratios["head_height_over_body_height"] = abs(resolved["head_top"][1]-resolved["chin"][1])/height
    if all(k in resolved for k in ("left_shoulder", "right_shoulder", "left_hip", "right_hip")):
        sw = math.dist(resolved["left_shoulder"], resolved["right_shoulder"])
        hw = math.dist(resolved["left_hip"], resolved["right_hip"])
        if hw > 1e-8:
            ratios["shoulder_width_over_hip_width"] = sw/hw
    if all(k in resolved for k in ("left_eye", "right_eye", "left_head_side", "right_head_side")):
        eye = math.dist(resolved["left_eye"], resolved["right_eye"])
        head_width = math.dist(resolved["left_head_side"], resolved["right_head_side"])
        if head_width > 1e-8:
            ratios["interpupillary_over_head_width"] = eye/head_width
    if "left_fingertip" in resolved and "right_fingertip" in resolved:
        ratios["arm_span_over_body_height"] = math.dist(resolved["left_fingertip"], resolved["right_fingertip"])/height

    return {
        "file": str(path), "generator": doc.get("asset", {}).get("generator"),
        "mesh_instances": mesh_instances, "primitives": primitive_count,
        "triangles": triangle_count, "coordinate_space": "scene world space, unskinned rest pose",
        "bounds_m": {"min": mins, "max": maxs, "dimensions": dims, "height": height},
        "ratios": ratios, "landmarks_m": resolved,
        "missing_landmarks": sorted(set(LANDMARK_PATTERNS)-set(resolved)),
        "cross_section_samples": slices,
        "notes": [
            "Only POSITION vertices referenced by triangle primitives are sampled.",
            "Skinned vertices are measured in their unskinned rest pose; animation and joint deformation are not applied.",
            "Landmarks are reported only from recognisable named nodes. Unnamed mesh anatomy is not guessed.",
            "Cross-section widths are descriptive samples, not anatomical shoulder/hip measurements."
        ]
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("reference", help="reference GLB, e.g. mylove.glb")
    ap.add_argument("candidate", help="generated avatar GLB")
    ap.add_argument("--json", dest="json_path", help="write complete comparison report")
    args = ap.parse_args()
    ref, candidate = analyse(args.reference), analyse(args.candidate)
    keys = sorted(set(ref["ratios"]) | set(candidate["ratios"]))
    comparison = {}
    for key in keys:
        a, b = ref["ratios"].get(key), candidate["ratios"].get(key)
        comparison[key] = {"reference": a, "candidate": b,
                           "absolute_delta": abs(a-b) if a is not None and b is not None else None,
                           "status": "COMPARABLE" if a is not None and b is not None else "MISSING_LANDMARK"}
    report = {"reference": ref, "candidate": candidate, "comparison": comparison,
              "overall_status": "MEASURED_WITH_GAPS" if any(v["status"] != "COMPARABLE" for v in comparison.values()) else "MEASURED"}
    rendered = json.dumps(report, indent=2)
    if args.json_path:
        Path(args.json_path).write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    if any(v["status"] != "COMPARABLE" for v in comparison.values()):
        print("NOTE: Some ratios cannot be compared because named landmarks are absent.", file=sys.stderr)


if __name__ == "__main__":
    main()
