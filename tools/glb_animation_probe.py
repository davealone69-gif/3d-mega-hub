#!/usr/bin/env python3
"""Dependency-free GLB *animation* probe (glTF 2.0).

Usage:
  python3 tools/glb_animation_probe.py Fox.glb CesiumMan.glb
  python3 tools/glb_animation_probe.py Fox.glb --json report.json
  python3 tools/glb_animation_probe.py Fox.glb --require-clips 3 --require-skin

Companion to tools/glb_anatomy_probe.py. That tool measures proportions; this one answers the
question the animation system actually depends on:

  * Is there a skeleton?  How many joints, and what is the hierarchy?
  * Which joints are driven by which animation channel, with what interpolation?
  * How long is each clip, and does it loop?
  * Are there morph targets, what are they called, and are any of them *animated*?

It reports only what the file contains. A GLB with no skin, no animation or no morph targets is
reported as having none -- this probe never infers a missing clip from a filename, and never
claims an animation exists because the model looks like a character.

Exit status is 0 when every --require-* gate is satisfied, 1 otherwise, so CI can gate a build
on "the bundled fixture really is animated" instead of on a human reading a log.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

COMPONENT_TYPE = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2),
                  5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
TYPE_COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
                   "MAT2": 4, "MAT3": 9, "MAT4": 16}
PATH_NAMES = {"translation": "translation", "rotation": "rotation",
              "scale": "scale", "weights": "weights"}


class ProbeError(ValueError):
    """Raised for a structurally invalid GLB. Never for 'this file has no animation'."""


def read_glb(path):
    """Splits a GLB into its JSON document and BIN chunk. Mirrors af_glb_read.cpp's container rules."""
    data = Path(path).read_bytes()
    if len(data) < 20:
        raise ProbeError("file is too short to be a GLB")
    magic, version, total = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF":
        raise ProbeError("not a GLB: bad magic %r" % magic)
    if version != 2:
        raise ProbeError("unsupported GLB version %d (this tool reads 2.0)" % version)
    if total != len(data):
        raise ProbeError("declared length %d != actual %d" % (total, len(data)))
    pos, doc, binary = 12, None, b""
    while pos < total:
        if pos + 8 > total:
            raise ProbeError("truncated chunk header at %d" % pos)
        size, kind = struct.unpack_from("<I4s", data, pos)
        pos += 8
        end = pos + size
        if end > total:
            raise ProbeError("chunk %r extends past end of file" % kind)
        payload = data[pos:end]
        if kind == b"JSON":
            if doc is not None:
                raise ProbeError("multiple JSON chunks")
            doc = json.loads(payload.rstrip(b" \t\r\n\x00").decode("utf-8"))
        elif kind == b"BIN\x00":
            if binary:
                raise ProbeError("multiple BIN chunks")
            binary = payload
        pos = end
    if doc is None:
        raise ProbeError("missing JSON chunk")
    if doc.get("asset", {}).get("version", "").split(".")[0] != "2":
        raise ProbeError("asset is not glTF 2.x")
    return doc, binary


def accessor_floats(doc, binary, index):
    """Reads a SCALAR float accessor as a list of floats. Returns None for non-float accessors."""
    accs = doc.get("accessors") or []
    if index is None or index < 0 or index >= len(accs):
        return None
    acc = accs[index]
    if acc.get("componentType") != 5126:
        return None
    if acc.get("type") != "SCALAR":
        return None
    if acc.get("sparse"):
        # Sparse accessors are legal glTF; this probe reports them but does not reconstruct them,
        # because guessing a dense value would be worse than saying "sparse, not expanded".
        return None
    views = doc.get("bufferViews") or []
    bv_index = acc.get("bufferView")
    if bv_index is None or bv_index >= len(views):
        return None
    view = views[bv_index]
    if view.get("buffer", 0) != 0:
        return None
    offset = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    count = acc.get("count", 0)
    stride = view.get("byteStride") or 4
    end = offset + (count - 1) * stride + 4 if count else offset
    if end > len(binary):
        raise ProbeError("accessor %d reads past the BIN chunk" % index)
    out = []
    for i in range(count):
        out.append(struct.unpack_from("<f", binary, offset + i * stride)[0])
    return out


def node_name(doc, index):
    nodes = doc.get("nodes") or []
    if index is None or index < 0 or index >= len(nodes):
        return "<node %s>" % index
    return nodes[index].get("name") or "<unnamed node %d>" % index


def skeleton_report(doc):
    """Per-skin joint list, hierarchy depth and whether inverse bind matrices are present."""
    skins = doc.get("skins") or []
    nodes = doc.get("nodes") or []
    out = []
    for si, skin in enumerate(skins):
        joints = skin.get("joints") or []
        parent_of = {}
        for ni, node in enumerate(nodes):
            for child in (node.get("children") or []):
                parent_of[child] = ni
        depth = {}

        def depth_of(j, seen=None):
            seen = seen or set()
            if j in depth:
                return depth[j]
            if j in seen or j not in parent_of:
                depth[j] = 0
                return 0
            seen.add(j)
            depth[j] = 1 + depth_of(parent_of[j], seen)
            return depth[j]

        max_depth = max((depth_of(j) for j in joints), default=0)
        out.append({
            "skin": si,
            "name": skin.get("name") or "<unnamed skin %d>" % si,
            "jointCount": len(joints),
            "maxHierarchyDepth": max_depth,
            "hasInverseBindMatrices": "inverseBindMatrices" in skin,
            "skeletonRootNode": node_name(doc, skin.get("skeleton")) if skin.get("skeleton") is not None else None,
            "jointNames": [node_name(doc, j) for j in joints],
        })
    return out


def morph_report(doc):
    """Morph targets per mesh: count and names, taken from mesh.extras.targetNames when present."""
    out = []
    for mi, mesh in enumerate(doc.get("meshes") or []):
        targets_seen = []
        names = []
        for prim in mesh.get("primitives") or []:
            tgts = prim.get("targets") or []
            targets_seen.append(len(tgts))
        extras = mesh.get("extras") or {}
        raw_names = extras.get("targetNames")
        if isinstance(raw_names, list):
            names = [str(n) for n in raw_names]
        out.append({
            "mesh": mi,
            "name": mesh.get("name") or "<unnamed mesh %d>" % mi,
            "morphTargetCountPerPrimitive": targets_seen,
            "maxMorphTargets": max(targets_seen) if targets_seen else 0,
            "targetNames": names,
        })
    return out


def animation_report(doc, binary):
    """Every animation: duration, and per-channel target node / path / interpolation / key count."""
    animations = doc.get("animations") or []
    samplers_by_anim = []
    for ai, anim in enumerate(animations):
        samplers = anim.get("samplers") or []
        channels = anim.get("channels") or []
        entries = []
        duration = 0.0
        weight_channels = 0
        paths = {}
        interps = {}
        for ci, ch in enumerate(channels):
            si = ch.get("sampler")
            if si is None or si >= len(samplers):
                continue
            sampler = samplers[si]
            target = ch.get("target") or {}
            path = PATH_NAMES.get(target.get("path"), target.get("path"))
            times = accessor_floats(doc, binary, sampler.get("input"))
            interp = sampler.get("interpolation", "LINEAR")
            key_count = len(times) if times is not None else None
            if times:
                duration = max(duration, times[-1])
            paths[path] = paths.get(path, 0) + 1
            interps[interp] = interps.get(interp, 0) + 1
            if path == "weights":
                weight_channels += 1
            entries.append({
                "channel": ci,
                "node": target.get("node"),
                "nodeName": node_name(doc, target.get("node")),
                "path": path,
                "interpolation": interp,
                "keyCount": key_count,
                "firstKeyTime": times[0] if times else None,
                "lastKeyTime": times[-1] if times else None,
                "outputAccessorIsSparse": bool(
                    (doc.get("accessors") or [{}])[sampler.get("output", -1)].get("sparse")
                    if isinstance(sampler.get("output"), int)
                    and 0 <= sampler.get("output", -1) < len(doc.get("accessors") or [])
                    else False),
            })
        samplers_by_anim.append({
            "animation": ai,
            "name": anim.get("name") or "<unnamed animation %d>" % ai,
            "durationSeconds": round(duration, 6),
            "channelCount": len(entries),
            "samplerCount": len(samplers),
            "morphWeightChannels": weight_channels,
            "paths": paths,
            "interpolations": interps,
            "drivenNodes": sorted({e["nodeName"] for e in entries}),
            "channels": entries,
        })
    return samplers_by_anim


def probe(path):
    doc, binary = read_glb(path)
    meshes = doc.get("meshes") or []
    anims = animation_report(doc, binary)
    morphs = morph_report(doc)
    skins = skeleton_report(doc)
    return {
        "file": str(path),
        "bytes": Path(path).stat().st_size,
        "generator": (doc.get("asset") or {}).get("generator"),
        "gltfVersion": (doc.get("asset") or {}).get("version"),
        "nodeCount": len(doc.get("nodes") or []),
        "meshCount": len(meshes),
        "skinCount": len(skins),
        "animationCount": len(anims),
        "totalMorphTargets": sum(m["maxMorphTargets"] for m in morphs),
        "animatedMorphTargets": sum(a["morphWeightChannels"] for a in anims),
        "extensionsUsed": doc.get("extensionsUsed") or [],
        "extensionsRequired": doc.get("extensionsRequired") or [],
        "skins": skins,
        "meshes": morphs,
        "animations": anims,
    }


def summarise(rep, verbose_channels=False):
    lines = []
    lines.append("== %s" % rep["file"])
    lines.append("   %d bytes | glTF %s | generator: %s" % (
        rep["bytes"], rep["gltfVersion"], rep["generator"] or "<none>"))
    lines.append("   nodes=%d meshes=%d skins=%d animations=%d morphTargets=%d (animated=%d)" % (
        rep["nodeCount"], rep["meshCount"], rep["skinCount"], rep["animationCount"],
        rep["totalMorphTargets"], rep["animatedMorphTargets"]))
    if rep["extensionsRequired"]:
        lines.append("   REQUIRED extensions: %s" % ", ".join(rep["extensionsRequired"]))
    if rep["skinCount"] == 0:
        lines.append("   SKELETON: none -- this file cannot be skinned or retargeted")
    for s in rep["skins"]:
        lines.append("   SKELETON skin[%d] %r: %d joints, depth %d, inverseBind=%s, root=%s" % (
            s["skin"], s["name"], s["jointCount"], s["maxHierarchyDepth"],
            s["hasInverseBindMatrices"], s["skeletonRootNode"]))
        lines.append("      joints: %s" % ", ".join(s["jointNames"][:24]) +
                     (" ..." if len(s["jointNames"]) > 24 else ""))
    if rep["animationCount"] == 0:
        lines.append("   ANIMATION: none -- no playable clip in this file")
    for a in rep["animations"]:
        lines.append("   CLIP[%d] %r: %.3f s, %d channels, paths=%s, interp=%s, morphWeightChannels=%d" % (
            a["animation"], a["name"], a["durationSeconds"], a["channelCount"],
            json.dumps(a["paths"], sort_keys=True), json.dumps(a["interpolations"], sort_keys=True),
            a["morphWeightChannels"]))
        if verbose_channels:
            for c in a["channels"]:
                lines.append("      ch[%d] node=%r path=%s interp=%s keys=%s range=[%s..%s]" % (
                    c["channel"], c["nodeName"], c["path"], c["interpolation"],
                    c["keyCount"], c["firstKeyTime"], c["lastKeyTime"]))
    for m in rep["meshes"]:
        if m["maxMorphTargets"]:
            lines.append("   MORPHS mesh[%d] %r: %d targets, names=%s" % (
                m["mesh"], m["name"], m["maxMorphTargets"],
                json.dumps(m["targetNames"]) if m["targetNames"] else "<file declares no targetNames>"))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="Dependency-free GLB animation probe (glTF 2.0).")
    ap.add_argument("files", nargs="+", help="GLB files to inspect")
    ap.add_argument("--json", dest="json_path", help="write the full report as JSON")
    ap.add_argument("--channels", action="store_true", help="list every animation channel")
    ap.add_argument("--require-skin", action="store_true",
                    help="fail unless at least one skin with joints is present")
    ap.add_argument("--require-clips", type=int, default=0, metavar="N",
                    help="fail unless at least N animation clips are present")
    ap.add_argument("--require-morphs", action="store_true",
                    help="fail unless at least one animated morph-target channel is present")
    args = ap.parse_args()

    reports = []
    failed = []
    for f in args.files:
        try:
            rep = probe(f)
        except ProbeError as exc:
            print("!! %s: %s" % (f, exc), file=sys.stderr)
            failed.append(f)
            continue
        except (OSError, json.JSONDecodeError, struct.error) as exc:
            print("!! %s: unreadable or not a GLB: %s" % (f, exc), file=sys.stderr)
            failed.append(f)
            continue
        reports.append(rep)
        print(summarise(rep, args.channels))

    if args.json_path:
        Path(args.json_path).write_text(json.dumps(reports, indent=2))
        print("wrote %s" % args.json_path)

    gate_failures = []
    for rep in reports:
        if args.require_skin and rep["skinCount"] == 0:
            gate_failures.append("%s: --require-skin but the file has no skin" % rep["file"])
        if args.require_clips and rep["animationCount"] < args.require_clips:
            gate_failures.append("%s: --require-clips %d but found %d" % (
                rep["file"], args.require_clips, rep["animationCount"]))
        if args.require_morphs and rep["animatedMorphTargets"] == 0:
            gate_failures.append("%s: --require-morphs but no animated morph-target channel" % rep["file"])
    for g in gate_failures:
        print("GATE FAIL: %s" % g, file=sys.stderr)

    if failed or gate_failures:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
