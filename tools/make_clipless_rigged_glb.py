#!/usr/bin/env python3
"""Generate CliplessRigged.glb: a skinned humanoid with inverse bind matrices and ZERO animations.

WHY THIS FILE EXISTS
--------------------
The engine's imported-animation path has two distinct refusals that are easy to conflate:

  * a file with no skin at all            -> nothing to deform, stays static (female.glb covers this)
  * a file with a skin but no animations  -> deformable, but nothing to play

The second case is the one mylove_rig_prepared.glb (20 joints, inverse bind matrices, 17/17 humanoid
roles, 0 animations) falls into, and it is what the authored-library role-binding fallback exists for.
That file is 13 MB and lives on another branch, so it cannot be a test fixture. No asset in this repo
covered the case, which meant the fallback could only be verified out-of-tree by hand.

This script writes a ~4 KB file that does: 17 canonically named joints so assignRoles() resolves all 17
roles, identity rest rotations, real inverse bind matrices, and a skinned cube whose vertices are split
between Hips and the left arm so that a clip which rotates the arm provably moves vertices. It contains
no animations at all, so importing it exercises the fallback and nothing else.

Regenerate with:  python3 tools/make_clipless_rigged_glb.py
The output is byte-stable, so `git diff` stays empty after a regeneration.
"""

import json
import os
import struct
import sys

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "app", "src", "main", "assets", "models", "animation", "CliplessRigged.glb")

# name, parent index (-1 for root), local translation
JOINTS = [
    ("Hips",           -1, (0.00, 0.90, 0.00)),
    ("Spine",           0, (0.00, 0.13, 0.00)),
    ("Chest",           1, (0.00, 0.14, 0.00)),
    ("Neck",            2, (0.00, 0.11, 0.00)),
    ("Head",            3, (0.00, 0.08, 0.00)),
    ("LeftUpperArm",    2, (-0.13, -0.02, 0.00)),
    ("LeftLowerArm",    5, (0.00, -0.25, 0.00)),
    ("LeftHand",        6, (0.00, -0.22, 0.00)),
    ("RightUpperArm",   2, (0.13, -0.02, 0.00)),
    ("RightLowerArm",   8, (0.00, -0.25, 0.00)),
    ("RightHand",       9, (0.00, -0.22, 0.00)),
    ("LeftUpperLeg",    0, (-0.09, -0.02, 0.00)),
    ("LeftLowerLeg",   11, (0.00, -0.20, 0.00)),
    ("LeftFoot",       12, (0.00, -0.20, 0.04)),
    ("RightUpperLeg",   0, (0.09, -0.02, 0.00)),
    ("RightLowerLeg",  14, (0.00, -0.20, 0.00)),
    ("RightFoot",      15, (0.00, -0.20, 0.04)),
]

# Vertices of a small cube, and the joint each one is rigidly bound to. Half follow Hips, half follow
# LeftLowerArm, so a clip that only rotates the arm moves a measurable subset and leaves the rest put.
ARM_JOINT = 6   # LeftLowerArm
CUBE = [
    (-0.10, 0.80, -0.10), (0.10, 0.80, -0.10), (0.10, 1.00, -0.10), (-0.10, 1.00, -0.10),
    (-0.10, 0.80,  0.10), (0.10, 0.80,  0.10), (0.10, 1.00,  0.10), (-0.10, 1.00,  0.10),
]
ARM_VERTS = {2, 3, 6, 7}
INDICES = [
    0, 1, 2, 0, 2, 3,   4, 6, 5, 4, 7, 6,   0, 4, 5, 0, 5, 1,
    3, 2, 6, 3, 6, 7,   0, 3, 7, 0, 7, 4,   1, 5, 6, 1, 6, 2,
]
FACES = [(0, 0, -1), (0, 0, 1), (-1, 0, 0), (1, 0, 0), (0, -1, 0), (0, 1, 0)]


def world_translations():
    out = []
    for _, parent, t in JOINTS:
        if parent < 0:
            out.append(t)
        else:
            p = out[parent]
            out.append((p[0] + t[0], p[1] + t[1], p[2] + t[2]))
    return out


def build():
    worlds = world_translations()
    buf = bytearray()
    views = []
    accessors = []

    def add_view(data, target=None):
        while len(buf) % 4:
            buf.append(0)
        offset = len(buf)
        buf.extend(data)
        v = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if target is not None:
            v["target"] = target
        views.append(v)
        return len(views) - 1

    def add_accessor(view, component_type, count, atype, mn=None, mx=None):
        a = {"bufferView": view, "componentType": component_type, "count": count, "type": atype}
        if mn is not None:
            a["min"], a["max"] = mn, mx
        accessors.append(a)
        return len(accessors) - 1

    # ---- geometry -------------------------------------------------------------
    pos = b"".join(struct.pack("<3f", *v) for v in CUBE)
    pos_view = add_view(pos, target=34962)
    pos_acc = add_accessor(pos_view, 5126, len(CUBE), "VEC3",
                           mn=[min(v[i] for v in CUBE) for i in range(3)],
                           mx=[max(v[i] for v in CUBE) for i in range(3)])

    per_vert_normal = [(0.0, 0.0, 0.0)] * len(CUBE)
    for tri in range(len(INDICES) // 3):
        n = FACES[tri // 2]
        for k in range(3):
            per_vert_normal[INDICES[tri * 3 + k]] = n
    normals = b"".join(struct.pack("<3f", *n) for n in per_vert_normal)
    nor_acc = add_accessor(add_view(normals, target=34962), 5126, len(CUBE), "VEC3")

    uvs = b"".join(struct.pack("<2f", 0.0, 0.0) for _ in CUBE)
    uv_acc = add_accessor(add_view(uvs, target=34962), 5126, len(CUBE), "VEC2")

    joints = b"".join(
        struct.pack("<4H", (ARM_JOINT if i in ARM_VERTS else 0), 0, 0, 0)
        for i in range(len(CUBE)))
    joints_acc = add_accessor(add_view(joints, target=34962), 5123, len(CUBE), "VEC4")

    weights = b"".join(struct.pack("<4f", 1.0, 0.0, 0.0, 0.0) for _ in CUBE)
    weights_acc = add_accessor(add_view(weights, target=34962), 5126, len(CUBE), "VEC4")

    idx = b"".join(struct.pack("<H", i) for i in INDICES)
    idx_acc = add_accessor(add_view(idx, target=34963), 5123, len(INDICES), "SCALAR")

    # ---- inverse bind matrices ------------------------------------------------
    # Rest rotations are identity and rest scale is 1, so each joint's bind-space world transform is a
    # pure translation and its inverse is the negation. Written as real data, not assumed by the reader.
    ibm = bytearray()
    for w in worlds:
        m = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, -w[0], -w[1], -w[2], 1.0]
        ibm.extend(struct.pack("<16f", *m))
    ibm_acc = add_accessor(add_view(bytes(ibm)), 5126, len(JOINTS), "MAT4")

    # ---- scene graph ----------------------------------------------------------
    joint_nodes = []
    for i, (name, parent, t) in enumerate(JOINTS):
        node = {"name": name, "translation": list(t)}
        children = [c for c, (_, p, _) in enumerate(JOINTS) if p == i]
        if children:
            node["children"] = children
        joint_nodes.append(node)
    mesh_node_index = len(JOINTS)
    nodes = joint_nodes + [{"name": "CliplessRiggedMesh", "mesh": 0, "skin": 0}]

    gltf = {
        "asset": {"version": "2.0",
                  "generator": "tools/make_clipless_rigged_glb.py (synthetic test fixture)"},
        "scene": 0,
        "scenes": [{"name": "CliplessRigged", "nodes": [0, mesh_node_index]}],
        "nodes": nodes,
        "meshes": [{"name": "CliplessRiggedCube", "primitives": [{
            "attributes": {"POSITION": pos_acc, "NORMAL": nor_acc, "TEXCOORD_0": uv_acc,
                           "JOINTS_0": joints_acc, "WEIGHTS_0": weights_acc},
            "indices": idx_acc, "mode": 4}]}],
        "skins": [{"name": "CliplessRiggedSkin", "inverseBindMatrices": ibm_acc,
                   "joints": list(range(len(JOINTS))), "skeleton": 0}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(buf)}],
        # Deliberately absent: "animations". That is the entire point of this fixture.
    }

    js = json.dumps(gltf, separators=(",", ":"), sort_keys=True).encode("ascii")
    while len(js) % 4:
        js += b" "
    while len(buf) % 4:
        buf.append(0)

    total = 12 + 8 + len(js) + 8 + len(buf)
    out = bytearray()
    out += struct.pack("<4sII", b"glTF", 2, total)
    out += struct.pack("<II", len(js), 0x4E4F534A)
    out += js
    out += struct.pack("<II", len(buf), 0x004E4942)
    out += buf
    return bytes(out)


def main():
    data = build()
    with open(OUT, "wb") as fh:
        fh.write(data)
    print("wrote %s (%d bytes)" % (OUT, len(data)))
    print("  %d joints, %d vertices, %d triangles, 0 animations"
          % (len(JOINTS), len(CUBE), len(INDICES) // 3))
    print("  vertices bound to LeftLowerArm: %s" % sorted(ARM_VERTS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
