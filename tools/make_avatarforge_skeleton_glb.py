#!/usr/bin/env python3
"""Build AvatarForgeSkeleton.glb: the donor's real 71-joint skeleton without its 33.6 MiB of pixels.

WHY THIS FILE EXISTS
--------------------
`AvatarForge/artifacts/avatarforge-sample.glb` (35,188,740 bytes, generator "AvatarForge 0.9.0",
branch `work/android-engine-dev` @ ba45d59, sha256 7cbc16d0...5cda) is the production skeleton this
engine is meant to drive, and nothing in this repo covered it:

  * Fox.glb / CesiumMan.glb  - foreign joint naming, so they exercise fuzzy role matching
  * CliplessRigged.glb       - 17 synthetically named joints, no clips at all
  * female.glb               - no skin, stays static
  * mylove_rig_prepared.glb  - 20 joints, 13 MB, on another branch, cannot be a fixture

The donor's naming is not foreign: `af_rig.cpp` builds `hips`, `spine_01`..`spine_04`, `chest`,
`neck_01/02`, `head`, `clavicle_L`, `upperarm_01/02_L`, `forearm_01/02_L`, `hand_L`,
`upperarm_twist_L`, `forearm_twist_L`, `thigh_L`, `shin_L`, `foot_L`, `toe_L`, `thigh_twist_L` --
byte for byte the donor's joint names. So this fixture is the only one that exercises the engine's
OWN naming at full production depth, through `Skeleton::assignRoles()` and the authored-clip table
in `af_anim_clip.cpp` rather than through a guessing path.

WHAT IS KEPT AND WHAT IS DROPPED
--------------------------------
Kept, copied from the donor by decoding and re-encoding each accessor (so donor `byteStride`
interleaving cannot leak into the fixture):
  * all 72 nodes verbatim - names, hierarchy and rest TRS
  * the skin's 71 inverse bind matrices (4,544 bytes)
  * all 15 clips with all 4,221 keyframes - times and values (84,224 bytes)
  * the 52 ARKit morph target names

Dropped and replaced: the 35 MB mesh and its textures, which are 365x the skeleton and irrelevant
to import, rig adoption, role resolution and clip binding. In their place a 24-vertex skinned box
whose vertices are split between `hips` and `upperarm_01_L`, so a clip that rotates the arm
provably moves vertices instead of merely reporting that it played.

Output is byte-stable: no timestamps, deterministic ordering, so re-running leaves `git diff` empty.

Usage:
  python3 tools/make_avatarforge_skeleton_glb.py DONOR.glb -o OUT.glb
"""

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

GLB_MAGIC = 0x46546C67
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

COMPONENT_FMT = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
COMPONENT_SIZE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
TYPE_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

# The two joints the synthetic mesh is split between. Named, not indexed, so a donor that
# renumbers its nodes cannot silently bind every vertex to the root and make the fixture
# pass while testing nothing.
BIND_JOINT_A = "hips"
BIND_JOINT_B = "upperarm_01_L"

BOX_VERTS = 24


def fail(msg):
    print(f"FAILED: {msg}", file=sys.stderr)
    raise SystemExit(1)


def read_glb(path):
    raw = path.read_bytes()
    if len(raw) < 12:
        fail(f"{path.name} is {len(raw)} bytes, too short for a GLB header")
    magic, version, length = struct.unpack_from("<III", raw, 0)
    if magic != GLB_MAGIC:
        fail(f"{path.name} magic is 0x{magic:08X}, not glTF")
    if length != len(raw):
        print(
            f"WARNING: header length {length} != file size {len(raw)}; donor may be truncated",
            file=sys.stderr,
        )
    off, gltf, bin_chunk = 12, None, b""
    while off + 8 <= len(raw):
        clen, ctype = struct.unpack_from("<II", raw, off)
        off += 8
        body = raw[off : off + clen]
        if len(body) != clen:
            fail(f"chunk at {off} declares {clen} bytes but only {len(body)} remain")
        off += clen
        if ctype == CHUNK_JSON:
            gltf = json.loads(body.decode("utf-8"))
        elif ctype == CHUNK_BIN:
            bin_chunk = body
    if gltf is None:
        fail(f"{path.name} has no JSON chunk")
    return gltf, bin_chunk, version


def decode_accessor(gltf, bin_chunk, index):
    """Decode an accessor to a flat list of numbers, honouring byteStride."""
    acc = gltf["accessors"][index]
    ct = acc["componentType"]
    if ct not in COMPONENT_FMT:
        fail(f"accessor {index} componentType {ct} unsupported")
    ncomp = TYPE_COUNT.get(acc["type"])
    if ncomp is None:
        fail(f"accessor {index} type {acc['type']!r} unsupported")
    view = gltf["bufferViews"][acc["bufferView"]]
    stride = view.get("byteStride")
    base = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    item = COMPONENT_SIZE[ct] * ncomp
    fmt = "<" + COMPONENT_FMT[ct] * ncomp
    out = []
    for i in range(acc["count"]):
        at = base + (i * stride if stride else i * item)
        if at + item > len(bin_chunk):
            fail(f"accessor {index} element {i} overruns the BIN chunk ({len(bin_chunk)} bytes)")
        out.extend(struct.unpack_from(fmt, bin_chunk, at))
    return out, ct, ncomp, acc["count"]


class BinBuilder:
    """Accumulates tightly packed buffer data and the views/accessors that describe it."""

    def __init__(self):
        self.data = bytearray()
        self.views = []
        self.accessors = []

    def _pad4(self):
        while len(self.data) % 4:
            self.data.append(0)

    def add(self, values, component_type, acc_type, min_max=True):
        self._pad4()
        ncomp = TYPE_COUNT[acc_type]
        count = len(values) // ncomp
        if len(values) % ncomp:
            fail(f"{len(values)} values do not divide into {acc_type} ({ncomp} components)")
        payload = struct.pack("<" + COMPONENT_FMT[component_type] * len(values), *values)
        offset = len(self.data)
        self.data.extend(payload)
        self.views.append(
            {"buffer": 0, "byteOffset": offset, "byteLength": len(payload)}
        )
        acc = {
            "bufferView": len(self.views) - 1,
            "componentType": component_type,
            "count": count,
            "type": acc_type,
        }
        if min_max and component_type in (5126, 5123, 5125, 5121):
            cols = [values[i::ncomp] for i in range(ncomp)]
            acc["min"] = [min(c) for c in cols]
            acc["max"] = [max(c) for c in cols]
        self.accessors.append(acc)
        return len(self.accessors) - 1


def unit_box():
    """24 vertices (4 per face, so normals are unambiguous) of a 0.2 m box centred on origin."""
    h = 0.1
    faces = [
        ((0, 0, 1), [(-h, -h, h), (h, -h, h), (h, h, h), (-h, h, h)]),
        ((0, 0, -1), [(h, -h, -h), (-h, -h, -h), (-h, h, -h), (h, h, -h)]),
        ((1, 0, 0), [(h, -h, h), (h, -h, -h), (h, h, -h), (h, h, h)]),
        ((-1, 0, 0), [(-h, -h, -h), (-h, -h, h), (-h, h, h), (-h, h, -h)]),
        ((0, 1, 0), [(-h, h, h), (h, h, h), (h, h, -h), (-h, h, -h)]),
        ((0, -1, 0), [(-h, -h, -h), (h, -h, -h), (h, -h, h), (-h, -h, h)]),
    ]
    pos, norm, idx = [], [], []
    for n, verts in faces:
        # `pos` is a FLAT coordinate list, so the vertex index this face starts at is a third of
        # its length. Using len(pos) directly emits indices up to 63 for a 24-vertex box, which the
        # importer correctly refuses with "index beyond the primitive's vertex count".
        base = len(pos) // 3
        for v in verts:
            pos.extend(v)
            norm.extend(n)
        idx.extend([base, base + 1, base + 2, base, base + 2, base + 3])
    return pos, norm, idx


def build(donor):
    gltf, bin_chunk, version = read_glb(donor)
    nodes = gltf.get("nodes", [])
    if len(nodes) < 2:
        fail(f"donor has only {len(nodes)} nodes; expected a full skeleton")
    skins = gltf.get("skins", [])
    if len(skins) != 1:
        fail(f"donor has {len(skins)} skins; this tool copies exactly one")
    skin = skins[0]
    joint_indices = skin["joints"]
    joint_names = [nodes[j].get("name") for j in joint_indices]
    if any(n is None for n in joint_names):
        fail("a joint node has no name, so it cannot be bound by name")

    if skin.get("inverseBindMatrices") is None:
        # The engine refuses to adopt a rig without them; a fixture that could not be adopted
        # would test nothing, so this is fatal here rather than a warning.
        fail("donor skin has no inverseBindMatrices; the engine would refuse to adopt it")
    ibm, ibm_ct, ibm_nc, ibm_count = decode_accessor(gltf, bin_chunk, skin["inverseBindMatrices"])
    if ibm_nc != 16 or ibm_count != len(joint_indices):
        fail(f"inverseBindMatrices is {ibm_count}x{ibm_nc}, expected {len(joint_indices)}x16")

    for want in (BIND_JOINT_A, BIND_JOINT_B):
        if want not in joint_names:
            fail(f"donor skeleton has no joint named {want!r}; cannot bind the synthetic mesh")
    bind_a = joint_names.index(BIND_JOINT_A)
    bind_b = joint_names.index(BIND_JOINT_B)

    # Which node carries the skin? That node gets the synthetic mesh.
    skinned_nodes = [i for i, n in enumerate(nodes) if n.get("skin") == 0]
    if len(skinned_nodes) != 1:
        fail(f"{len(skinned_nodes)} nodes reference skin 0; expected exactly one")
    mesh_node = skinned_nodes[0]

    binb = BinBuilder()

    # --- geometry: a box split between two joints so an arm clip provably moves vertices ---
    pos, norm, idx = unit_box()
    pos_acc = binb.add(pos, 5126, "VEC3")
    norm_acc = binb.add(norm, 5126, "VEC3")
    idx_acc = binb.add(idx, 5123, "SCALAR")
    joints_attr, weights_attr = [], []
    for v in range(BOX_VERTS):
        # Front and top faces follow the left upper arm; everything else follows the hips.
        arm = v < 12
        joints_attr.extend([bind_b if arm else bind_a, 0, 0, 0])
        weights_attr.extend([1.0, 0.0, 0.0, 0.0])
    joints_acc = binb.add(joints_attr, 5123, "VEC4", min_max=False)
    weights_acc = binb.add(weights_attr, 5126, "VEC4")

    # --- morph targets: the donor's 52 ARKit names, each a small deterministic offset ---
    morph_names = []
    for mesh in gltf.get("meshes", []):
        ex = mesh.get("extras") or {}
        if isinstance(ex.get("targetNames"), list):
            morph_names = [str(n) for n in ex["targetNames"]]
            break
    morph_accs = []
    for t, _name in enumerate(morph_names):
        delta = []
        for v in range(BOX_VERTS):
            # A distinct, non-zero delta per target so a weight of 1 cannot be confused with
            # a weight that was never applied.
            delta.extend([0.001 * (t + 1), -0.0005 * (t + 1), 0.0])
        morph_accs.append(binb.add(delta, 5126, "VEC3"))

    ibm_acc = binb.add(ibm, ibm_ct, "MAT4")

    # --- animations: every clip, every keyframe, decoded and re-encoded tightly ---
    out_anims = []
    total_keys = 0
    for anim in gltf.get("animations", []):
        samplers_out, channels_out = [], []
        for smp in anim.get("samplers", []):
            times, t_ct, t_nc, t_count = decode_accessor(gltf, bin_chunk, smp["input"])
            vals, v_ct, v_nc, v_count = decode_accessor(gltf, bin_chunk, smp["output"])
            if t_nc != 1:
                fail("a sampler's input accessor is not SCALAR")
            t_type = "SCALAR"
            v_type = {1: "SCALAR", 2: "VEC2", 3: "VEC3", 4: "VEC4", 16: "MAT4"}.get(v_nc)
            if v_type is None:
                fail(f"a sampler output has {v_nc} components, no glTF type for that")
            if v_count != t_count:
                fail(f"sampler has {t_count} times but {v_count} values")
            total_keys += t_count
            in_acc = binb.add(times, t_ct, t_type)
            out_acc = binb.add(vals, v_ct, v_type, min_max=False)
            entry = {"input": in_acc, "output": out_acc}
            if smp.get("interpolation"):
                entry["interpolation"] = smp["interpolation"]
            samplers_out.append(entry)
        for ch in anim.get("channels", []):
            tgt = ch.get("target", {})
            if tgt.get("node") is None:
                fail("a channel targets no node")
            channels_out.append(
                {"sampler": ch["sampler"], "target": {"node": tgt["node"], "path": tgt["path"]}}
            )
        out_anims.append(
            {"name": anim.get("name") or f"animation_{len(out_anims)}",
             "channels": channels_out, "samplers": samplers_out}
        )
    if not out_anims:
        fail("donor has no animations, so there is nothing to verify clip binding against")

    # --- nodes: copied verbatim, with the skinned node repointed at the synthetic mesh ---
    out_nodes = []
    for i, n in enumerate(nodes):
        keep = {}
        if n.get("name") is not None:
            keep["name"] = n["name"]
        if n.get("children"):
            keep["children"] = list(n["children"])
        for k in ("translation", "rotation", "scale"):
            if k in n:
                keep[k] = list(n[k])
        if i == mesh_node:
            keep["mesh"] = 0
            keep["skin"] = 0
        if n.get("skin") == 0 and i != mesh_node:
            keep["skin"] = 0
        out_nodes.append(keep)

    primitive = {
        "attributes": {
            "POSITION": pos_acc,
            "NORMAL": norm_acc,
            "JOINTS_0": joints_acc,
            "WEIGHTS_0": weights_acc,
        },
        "indices": idx_acc,
    }
    if morph_accs:
        primitive["targets"] = [{"POSITION": a} for a in morph_accs]

    out_mesh = {"name": "AvatarForgeSkeletonProbe", "primitives": [primitive]}
    if morph_names:
        out_mesh["extras"] = {"targetNames": morph_names}

    out_skin = {
        "name": skin.get("name") or "AvatarSkeleton",
        "joints": list(joint_indices),
        "inverseBindMatrices": ibm_acc,
    }
    if skin.get("skeleton") is not None:
        out_skin["skeleton"] = skin["skeleton"]

    scenes = [{"nodes": [i for i, n in enumerate(out_nodes) if not _has_parent(nodes, i)]}]
    out = {
        "asset": {
            "version": "2.0",
            "generator": "tools/make_avatarforge_skeleton_glb.py (skeleton of AvatarForge 0.9.0 sample)",
        },
        "scene": 0,
        "scenes": scenes,
        "nodes": out_nodes,
        "meshes": [out_mesh],
        "skins": [out_skin],
        "animations": out_anims,
        "accessors": binb.accessors,
        "bufferViews": binb.views,
        "buffers": [{"byteLength": len(binb.data)}],
    }
    return out, bytes(binb.data), {
        "joints": len(joint_indices),
        "clips": len(out_anims),
        "keyframes": total_keys,
        "morphs": len(morph_names),
        "binBytes": len(binb.data),
        "sourceBytes": donor.stat().st_size,
        "sourceSha256": hashlib.sha256(donor.read_bytes()).hexdigest(),
    }


def _has_parent(nodes, index):
    return any(index in (n.get("children") or []) for n in nodes)


def write_glb(gltf, bin_bytes, path):
    js = json.dumps(gltf, separators=(",", ":"), sort_keys=True).encode("utf-8")
    while len(js) % 4:
        js += b" "
    while len(bin_bytes) % 4:
        bin_bytes += b"\x00"
    total = 12 + 8 + len(js) + 8 + len(bin_bytes)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", GLB_MAGIC, 2, total))
        f.write(struct.pack("<II", len(js), CHUNK_JSON))
        f.write(js)
        f.write(struct.pack("<II", len(bin_bytes), CHUNK_BIN))
        f.write(bin_bytes)
    return total


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("donor", type=Path, help="avatarforge-sample.glb to strip down")
    ap.add_argument("-o", "--output", type=Path, required=True, help="fixture GLB to write")
    args = ap.parse_args()

    if not args.donor.is_file():
        fail(f"{args.donor} is not a readable file")
    gltf, bin_bytes, stats = build(args.donor)
    written = write_glb(gltf, bin_bytes, args.output)
    print(
        f"wrote {args.output} ({written:,} bytes) from {stats['sourceBytes']:,} bytes"
        f" ({stats['sourceBytes'] / max(written, 1):.0f}x smaller)"
    )
    print(
        f"  {stats['joints']} joints | {stats['clips']} clips | {stats['keyframes']} keyframes"
        f" | {stats['morphs']} morph targets | BIN {stats['binBytes']:,} bytes"
    )
    print(f"  donor sha256 {stats['sourceSha256']}")
    if stats["joints"] != 71:
        print(
            f"WARNING: expected the AvatarForge production skeleton's 71 joints, got {stats['joints']}",
            file=sys.stderr,
        )


if __name__ == "__main__":
    main()
