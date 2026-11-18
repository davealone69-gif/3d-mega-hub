#!/usr/bin/env python3
"""Extract a GLB's skeleton, clips and morph-target names into a compact JSON manifest.

WHY THIS FILE EXISTS
--------------------
The donor asset `AvatarForge/artifacts/avatarforge-sample.glb` (35,188,740 bytes, generator
"AvatarForge 0.9.0", branch `work/android-engine-dev` @ ba45d59) carries the production
skeleton this engine is meant to drive: one skin of 71 named joints at depth 16 WITH inverse
bind matrices, 15 animation clips, and the full 52-name ARKit blendshape set.

At 33.6 MiB it cannot be a test fixture, and that is a settled rule in this repo rather than a
fresh judgement: `tools/make_clipless_rigged_glb.py` records that `mylove_rig_prepared.glb`
"is 13 MB and lives on another branch, so it cannot be a test fixture", the largest GLB
tracked here is 438,044 bytes, and git-lfs is not installed. Committing 33.6 MiB would be 77x
that, permanently, in history.

What is worth keeping is the structure, not the pixels. This tool reads the donor and writes a
manifest of exactly the parts the engine's import path depends on: joint names, the joint
parent tree, inverse bind matrices, clip names with durations and channel targets, and morph
target names. A compact fixture is then generated FROM this manifest, so the fixture is derived
from the real asset instead of transcribed by hand -- a transcription of 71 joint names is
exactly the kind of thing that looks right and silently binds to nothing.

Output is byte-stable: no timestamp, no absolute paths, floats rounded to six decimals, so
re-running against the same donor leaves `git diff` empty.

Usage:
  python3 tools/extract_glb_skeleton.py donor.glb                 # summary only, writes nothing
  python3 tools/extract_glb_skeleton.py donor.glb -o out.json     # write the manifest
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

COMPONENT_TYPE = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
COMPONENT_SIZE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
TYPE_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

ROUND_TO = 6


def fail(msg):
    print(f"FAILED: {msg}", file=sys.stderr)
    raise SystemExit(1)


def read_glb(path):
    """Return (gltf_dict, bin_bytes). Refuses a file it cannot fully account for."""
    raw = path.read_bytes()
    if len(raw) < 12:
        fail(f"{path.name} is {len(raw)} bytes, too short for a GLB header")
    magic, version, length = struct.unpack_from("<III", raw, 0)
    if magic != GLB_MAGIC:
        fail(f"{path.name} magic is 0x{magic:08X}, not glTF 0x{GLB_MAGIC:08X}")
    if length != len(raw):
        # Not fatal on its own, but it means the file is truncated or padded, and a
        # truncated file silently yields a shorter skeleton. Say so rather than proceed.
        print(
            f"WARNING: header length {length} != file size {len(raw)}; the file may be truncated",
            file=sys.stderr,
        )
    offset = 12
    gltf, bin_chunk = None, b""
    while offset + 8 <= len(raw):
        clen, ctype = struct.unpack_from("<II", raw, offset)
        offset += 8
        body = raw[offset : offset + clen]
        if len(body) != clen:
            fail(f"chunk at {offset} declares {clen} bytes but only {len(body)} remain")
        offset += clen
        if ctype == CHUNK_JSON:
            gltf = json.loads(body.decode("utf-8"))
        elif ctype == CHUNK_BIN:
            bin_chunk = body
    if gltf is None:
        fail(f"{path.name} has no JSON chunk")
    return gltf, bin_chunk, version


def accessor_values(gltf, bin_chunk, index):
    """Decode one accessor into a flat list of numbers, honouring byteOffset and stride."""
    acc = gltf["accessors"][index]
    ctype = acc["componentType"]
    if ctype not in COMPONENT_TYPE:
        fail(f"accessor {index} componentType {ctype} is not a numeric type this tool knows")
    ncomp = TYPE_COUNT.get(acc["type"])
    if ncomp is None:
        fail(f"accessor {index} type {acc['type']!r} is not supported")
    count = acc["count"]
    view = gltf["bufferViews"][acc["bufferView"]]
    stride = view.get("byteStride")
    base = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    item = COMPONENT_SIZE[ctype] * ncomp
    fmt = "<" + COMPONENT_TYPE[ctype] * ncomp
    out = []
    for i in range(count):
        at = base + (i * stride if stride else i * item)
        if at + item > len(bin_chunk):
            fail(
                f"accessor {index} element {i} needs bytes {at}..{at + item} "
                f"but the BIN chunk is {len(bin_chunk)} bytes"
            )
        out.extend(struct.unpack_from(fmt, bin_chunk, at))
    return out, ncomp


def joint_depth(parent_of, joints):
    """Depth of the joint tree, counting the root as depth 1. Returns (depth, root_name)."""
    roots = [j for j in joints if parent_of.get(j) is None]
    if len(roots) != 1:
        # More than one root inside a skin is legal glTF but means the hierarchy this
        # engine expects is not what the file holds. Report it instead of picking one.
        print(
            f"WARNING: skin has {len(roots)} joint roots: {sorted(roots)}", file=sys.stderr
        )
    depth = {}

    def walk(node, d):
        depth[node] = max(depth.get(node, 0), d)
        for child, par in parent_of.items():
            if par == node:
                walk(child, d + 1)

    for r in roots:
        walk(r, 1)
    missing = [j for j in joints if j not in depth]
    if missing:
        fail(f"joints unreachable from any root: {sorted(missing)}")
    return max(depth.values()), (roots[0] if len(roots) == 1 else None), depth


def morph_names(gltf):
    """Morph target names, from mesh.extras.targetNames or a primitive's extras."""
    names = []
    for mesh in gltf.get("meshes", []):
        ex = mesh.get("extras") or {}
        if isinstance(ex.get("targetNames"), list):
            return [str(n) for n in ex["targetNames"]]
        for prim in mesh.get("primitives", []):
            pex = prim.get("extras") or {}
            if isinstance(pex.get("targetNames"), list):
                return [str(n) for n in pex["targetNames"]]
            targets = prim.get("targets") or []
            if len(targets) > len(names):
                names = [f"target_{i}" for i in range(len(targets))]
    return names


def extract(path):
    gltf, bin_chunk, version = read_glb(path)
    nodes = gltf.get("nodes", [])
    node_name = [n.get("name") or f"node_{i}" for i, n in enumerate(nodes)]

    parent_of = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []) or []:
            parent_of[node_name[c]] = node_name[i]

    skins = gltf.get("skins", [])
    if not skins:
        fail(f"{path.name} has no skin at all, so there is no skeleton to extract")
    skin = skins[0]
    joints = [node_name[j] for j in skin["joints"]]
    if len(set(joints)) != len(joints):
        fail(f"skin lists duplicate joint names among {len(joints)} joints")

    ibm_index = skin.get("inverseBindMatrices")
    inverse_binds = None
    if ibm_index is None:
        # This is the case the engine refuses to adopt, so it must be loud here too.
        print(
            "WARNING: skin has NO inverseBindMatrices accessor; the engine refuses to adopt "
            "a rig without them (WARN + clear())",
            file=sys.stderr,
        )
    else:
        flat, ncomp = accessor_values(gltf, bin_chunk, ibm_index)
        if ncomp != 16:
            fail(f"inverseBindMatrices accessor is not MAT4 (got {ncomp} components)")
        if len(flat) // 16 != len(joints):
            fail(
                f"inverseBindMatrices holds {len(flat) // 16} matrices for {len(joints)} joints"
            )
        inverse_binds = [
            [round(v, ROUND_TO) for v in flat[i * 16 : (i + 1) * 16]]
            for i in range(len(joints))
        ]

    depth, root, per_joint_depth = joint_depth(parent_of, joints)

    clips = []
    for anim in gltf.get("animations", []):
        samplers = anim.get("samplers", [])
        channels = []
        duration = 0.0
        for ch in anim.get("channels", []):
            tgt = ch.get("target", {})
            smp = samplers[ch["sampler"]]
            node = tgt.get("node")
            inp, _ = accessor_values(gltf, bin_chunk, smp["input"])
            if inp:
                duration = max(duration, max(inp))
            channels.append(
                {
                    "joint": node_name[node] if node is not None else None,
                    "path": tgt.get("path"),
                    "interpolation": smp.get("interpolation", "LINEAR"),
                    "outputs": len(accessor_values(gltf, bin_chunk, smp["output"])[0]),
                }
            )
        joints_touched = sorted({c["joint"] for c in channels if c["joint"]})
        clips.append(
            {
                "name": anim.get("name") or f"animation_{len(clips)}",
                "durationSeconds": round(duration, ROUND_TO),
                "channelCount": len(channels),
                "paths": sorted({c["path"] for c in channels if c["path"]}),
                "interpolations": sorted({c["interpolation"] for c in channels}),
                "jointsTouched": joints_touched,
                "morphWeightChannels": sum(1 for c in channels if c["path"] == "weights"),
            }
        )

    raw = path.read_bytes()
    return {
        "provenance": {
            "sourceFile": path.name,
            "sourceBytes": len(raw),
            "sourceSha256": hashlib.sha256(raw).hexdigest(),
            "glbVersion": version,
            "generator": (gltf.get("asset") or {}).get("generator"),
            "gltfVersion": (gltf.get("asset") or {}).get("version"),
            "extractor": "tools/extract_glb_skeleton.py",
        },
        "skeleton": {
            "skinName": skin.get("name"),
            "jointCount": len(joints),
            "jointNames": joints,
            "depth": depth,
            "root": root,
            "parentOfJoint": {j: parent_of.get(j) for j in joints},
            "jointDepth": {j: per_joint_depth[j] for j in joints},
            "hasInverseBindMatrices": inverse_binds is not None,
            "inverseBindMatrices": inverse_binds,
        },
        "clips": clips,
        "morphTargetNames": morph_names(gltf),
        "sceneCounts": {
            "nodes": len(nodes),
            "meshes": len(gltf.get("meshes", [])),
            "skins": len(skins),
            "animations": len(gltf.get("animations", [])),
            "materials": len(gltf.get("materials", [])),
        },
    }


def summarise(m):
    sk = m["skeleton"]
    p = m["provenance"]
    print(f"== {p['sourceFile']}")
    print(
        f"   {p['sourceBytes']} bytes | glTF {p['glbVersion']} container"
        f" | generator: {p['generator']}"
    )
    print(
        f"   skin {sk['skinName']!r}: {sk['jointCount']} joints, depth {sk['depth']},"
        f" root={sk['root']}, inverseBind={sk['hasInverseBindMatrices']}"
    )
    print(f"   clips: {len(m['clips'])} -> {', '.join(c['name'] for c in m['clips'])}")
    print(f"   morph targets: {len(m['morphTargetNames'])}")
    print(f"   sha256 {p['sourceSha256']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("glb", type=Path, help="donor GLB to read")
    ap.add_argument("-o", "--output", type=Path, help="write the manifest here")
    args = ap.parse_args()

    if not args.glb.is_file():
        fail(f"{args.glb} is not a readable file")
    m = extract(args.glb)
    summarise(m)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(m, indent=2, sort_keys=True) + "\n")
        print(f"wrote {args.output} ({args.output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
