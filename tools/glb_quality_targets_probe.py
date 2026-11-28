#!/usr/bin/env python3
"""Dependency-free GLB *quality-targets* probe (glTF 2.0).

Companion to glb_anatomy_probe.py (proportions/bounds) and glb_animation_probe.py
(rig/clips/morph counts). It exists to produce one comparable record for measuring a
reference asset - e.g. the owner's `ink.glb` photoreal benchmark documented in
docs/QUALITY_REFERENCE_ink_glb.md - against what the AvatarForge generator produces.

It reports the fields that audit needs and the other two probes omit:
  * exact vertex/index/triangle counts per primitive and totals;
  * the per-primitive attribute inventory, including how many TEXCOORD_ UV sets exist
    and whether NORMAL/TANGENT/COLOR/JOINTS/WEIGHTS are present;
  * materials: count, which extensions each material declares, texture-slot usage;
  * images: mime type, embedded byte length, and WIDTH x HEIGHT x CHANNELS read from the
    PNG / JPEG / WebP container header (never decoded, never fabricated);
  * skins: joint count and NAMES, inverse-bind presence/count, skeleton root, joint depth;
  * morph target NAMES where the file stores them (extras.targetNames) plus slot counts.

Honouring Priority Zero: a measurement it cannot take is reported as null or as an explicit
"unsupported-by-probe" note, never invented. A WebP variant it cannot sniff yields width=null
with a reason, not a guess. Reads only what the GLB container actually contains.

Usage:
  python3 tools/glb_quality_targets_probe.py ink.glb
  python3 tools/glb_quality_targets_probe.py ink.glb generated.glb --json compare.json

Reuses the battle-tested GLB/container parsing of its sibling probes so there is a single
source of truth for byte-range validation.
"""
import argparse
import json
import os
import struct
import sys

# Make the sibling probes importable regardless of the caller's working directory, then reuse
# their validated GLB parsing rather than duplicating it (and risking divergence).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import glb_anatomy_probe as anat
    import glb_animation_probe as anim
except ImportError as exc:  # pragma: no cover - environment problem, surface it plainly
    raise SystemExit(
        "could not import the sibling probes (%s); run this from a checkout that has "
        "tools/glb_anatomy_probe.py and tools/glb_animation_probe.py" % exc
    )


def _u16be(b, off):
    return struct.unpack_from(">H", b, off)[0]


def _u32be(b, off):
    return struct.unpack_from(">I", b, off)[0]


def _u24le(b, off):
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16)


# PNG colour-type -> channel count. Palette (3) is reported as 3 (RGB) here; a separate tRNS
# chunk could add transparency, which this header-only sniff does not chase down.
_PNG_CHANNELS = {0: 1, 2: 3, 3: 3, 4: 2, 6: 4}


def sniff_image(data):
    """Return (width, height, channels, note) from a compressed image's container header.

    Width/height/channels are integers when the format is recognised, else None. ``note``
    records why something could not be determined - it is never left blank to paper over a
    failure. No pixel data is decoded.
    """
    n = len(data)
    # ---- PNG -------------------------------------------------------------
    if n >= 33 and data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR":
        width = _u32be(data, 16)
        height = _u32be(data, 20)
        bit_depth = data[24]
        color_type = data[25]
        channels = _PNG_CHANNELS.get(color_type)
        note = ("png bit_depth=%d color_type=%d" % (bit_depth, color_type)
                + ("" if channels is not None else " (channel count unknown for this colour type)"))
        return width, height, channels, note
    # ---- JPEG ------------------------------------------------------------
    if n >= 4 and data[:2] == b"\xff\xd8":
        i = 2
        while i + 9 < n:
            if data[i] != 0xFF:
                i += 1
                continue
            marker = data[i + 1]
            # Skip fill bytes.
            if marker == 0xFF:
                i += 1
                continue
            # Standalone markers carry no segment length.
            if marker in (0x01,) or 0xD0 <= marker <= 0xD7 or marker in (0xD8, 0xD9):
                i += 2
                continue
            seg_len = _u16be(data, i + 2)
            # Start-of-frame markers, excluding the differential/arithmetic ones (C4, C8, CC).
            if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
                p = i + 4
                precision = data[p]
                height = _u16be(data, p + 1)
                width = _u16be(data, p + 3)
                components = data[p + 5]
                return width, height, components, "jpeg sof=%02X precision=%d" % (marker, precision)
            if seg_len < 2:
                break
            i += 2 + seg_len
        return None, None, None, "jpeg: no start-of-frame segment found"
    # ---- WebP ------------------------------------------------------------
    if n >= 30 and data[:4] == b"RIFF" and data[8:12] == b"WEBP":
        fourcc = data[12:16]
        if fourcc == b"VP8X" and n >= 30:
            width = _u24le(data, 24) + 1
            height = _u24le(data, 27) + 1
            return width, height, None, "webp VP8X extended (channels not in header)"
        if fourcc == b"VP8 " and n >= 30:
            # Lossy bitstream: sync code 0x9D 0x01 0x2A, then 14-bit width/height little-endian.
            j = data.find(b"\x9d\x01\x2a", 20)
            if j != -1 and j + 7 <= n:
                width = _u16be_swap(data, j + 3) & 0x3FFF
                height = _u16be_swap(data, j + 5) & 0x3FFF
                return width, height, None, "webp VP8 lossy (channels not in header)"
            return None, None, None, "webp VP8: sync code not located"
        if fourcc == b"VP8L" and n >= 25:
            # Lossless: signature byte 0x2F then a 32-bit value packing 14-bit width-1/height-1.
            sig = data[21]
            bits = struct.unpack_from("<I", data, 22)[0]
            if sig == 0x2F:
                width = (bits & 0x3FFF) + 1
                height = ((bits >> 14) & 0x3FFF) + 1
                return width, height, None, "webp VP8L lossless (channels not in header)"
            return None, None, None, "webp VP8L: bad signature byte"
        return None, None, None, "webp: unhandled fourcc %r" % fourcc
    return None, None, None, "unrecognised image container (first bytes %s)" % data[:8].hex()


def _u16be_swap(b, off):
    # WebP stores these little-endian despite the bitfield layout; named to avoid confusion
    # with the big-endian helpers above.
    return struct.unpack_from("<H", b, off)[0]


def image_records(doc, binary):
    out = []
    views = doc.get("bufferViews") or []
    images = doc.get("images") or []
    for idx, img in enumerate(images):
        rec = {"index": idx, "name": img.get("name"), "mimeType": img.get("mimeType")}
        width = height = channels = None
        note = ""
        vi = img.get("bufferView")
        if isinstance(vi, int) and 0 <= vi < len(views):
            view = views[vi]
            start = view.get("byteOffset", 0)
            length = view.get("byteLength", 0)
            rec["byteLength"] = length
            end = start + length
            if start >= 0 and end <= len(binary) and length > 0:
                width, height, channels, note = sniff_image(bytes(binary[start:end]))
            else:
                note = "bufferView reads outside the BIN chunk"
        elif img.get("uri"):
            rec["byteLength"] = None
            note = "image referenced by external uri, not embedded (cannot sniff dimensions)"
        else:
            note = "image has neither bufferView nor uri"
        rec.update({"width": width, "height": height, "channels": channels, "note": note})
        out.append(rec)
    return out


def primitive_geometry(doc, prim):
    accessors = doc.get("accessors") or []
    attrs = prim.get("attributes") or {}
    pos_idx = attrs.get("POSITION")
    vertices = None
    if isinstance(pos_idx, int) and 0 <= pos_idx < len(accessors):
        vertices = accessors[pos_idx].get("count")
    mode = prim.get("mode", 4)
    idx_idx = prim.get("indices")
    indices = None
    if isinstance(idx_idx, int) and 0 <= idx_idx < len(accessors):
        indices = accessors[idx_idx].get("count")
    if mode == 4:  # TRIANGLES
        triangles = (indices // 3) if indices is not None else ((vertices // 3) if vertices else None)
    else:
        triangles = None  # non-triangle topology is not reduced to a triangle count here
    texcoords = sorted(k for k in attrs if k.startswith("TEXCOORD_"))
    joints = sorted(k for k in attrs if k.startswith("JOINTS_"))
    weights = sorted(k for k in attrs if k.startswith("WEIGHTS_"))
    return {
        "vertices": vertices,
        "indices": indices,
        "triangles": triangles,
        "mode": mode,
        "texcoordSets": texcoords,
        "jointSets": joints,
        "weightSets": weights,
        "hasNormal": "NORMAL" in attrs,
        "hasTangent": "TANGENT" in attrs,
        "hasColor": any(k.startswith("COLOR_") for k in attrs),
        "material": prim.get("material"),
    }


def geometry_summary(doc):
    meshes = doc.get("meshes") or []
    prims = []
    for mi, mesh in enumerate(meshes):
        for pi, prim in enumerate(mesh.get("primitives") or []):
            g = primitive_geometry(doc, prim)
            g["mesh"] = mi
            g["primitive"] = pi
            g["targets"] = len(prim.get("targets") or [])
            prims.append(g)
    total_v = sum(p["vertices"] or 0 for p in prims)
    total_t = sum(p["triangles"] or 0 for p in prims)
    max_tex = max((len(p["texcoordSets"]) for p in prims), default=0)
    return {
        "primitives": prims,
        "primitiveCount": len(prims),
        "totalVertices": total_v,
        "totalTriangles": total_t,
        "maxTexcoordSets": max_tex,
        "anyNormals": any(p["hasNormal"] for p in prims),
        "anyTangents": any(p["hasTangent"] for p in prims),
        "anyVertexColors": any(p["hasColor"] for p in prims),
        "skinningAttributesPresent": any(p["jointSets"] and p["weightSets"] for p in prims),
    }


def material_records(doc):
    mats = doc.get("materials") or []
    out = []
    union = set()
    for idx, mat in enumerate(mats):
        ext_keys = sorted((mat.get("extensions") or {}).keys())
        union.update(ext_keys)
        mr = mat.get("pbrMetallicRoughness") or {}
        def _img(slot):
            t = (mr.get(slot) or {}) if isinstance(mr, dict) else {}
            return t.get("index")
        out.append({
            "index": idx,
            "name": mat.get("name"),
            "metallicRoughness": bool(mat.get("pbrMetallicRoughness")),
            "baseColorTexture": _img("baseColorTexture"),
            "metallicRoughnessTexture": _img("metallicRoughnessTexture"),
            "normalTexture": (mat.get("normalTexture") or {}).get("index"),
            "occlusionTexture": (mat.get("occlusionTexture") or {}).get("index"),
            "emissiveTexture": (mat.get("emissiveTexture") or {}).get("index"),
            "doubleSided": bool(mat.get("doubleSided")),
            "alphaMode": mat.get("alphaMode"),
            "extensionKeys": ext_keys,
        })
    return out, sorted(union)


def skin_records(doc):
    nodes = doc.get("nodes") or []
    parents = {}
    for i, node in enumerate(nodes):
        for child in node.get("children") or []:
            parents[child] = i

    def depth_of(i, seen):
        if i in seen:
            raise ValueError("skin/joint node hierarchy contains a cycle")
        if i not in parents:
            return 0
        return 1 + depth_of(parents[i], seen | {i})

    accessors = doc.get("accessors") or []
    out = []
    for si, skin in enumerate(doc.get("skins") or []):
        joints = skin.get("joints") or []
        names = [nodes[j].get("name") if 0 <= j < len(nodes) else "<oob:%d>" % j for j in joints]
        ibm = skin.get("inverseBindMatrices")
        ibm_present = isinstance(ibm, int) and 0 <= ibm < len(accessors)
        ibm_count = accessors[ibm].get("count") if ibm_present else None
        root = skin.get("skeleton")
        try:
            max_depth = max((depth_of(j, set()) for j in joints if 0 <= j < len(nodes)), default=None)
        except ValueError as exc:
            max_depth = None
            names_note = str(exc)
        else:
            names_note = ""
        out.append({
            "index": si,
            "name": skin.get("name"),
            "jointCount": len(joints),
            "jointNames": names,
            "inverseBindMatricesPresent": ibm_present,
            "inverseBindMatrixCount": ibm_count,
            "skeletonRoot": (nodes[root].get("name") if isinstance(root, int) and 0 <= root < len(nodes) else root),
            "maxJointDepthFromSceneRoot": max_depth,
            "note": names_note,
        })
    return out


def morph_target_names(doc):
    meshes = doc.get("meshes") or []
    per_mesh = []
    union = []
    slots = 0
    for mi, mesh in enumerate(meshes):
        names = (mesh.get("extras") or {}).get("targetNames")
        prim_slots = max((len(p.get("targets") or []) for p in mesh.get("primitives") or []), default=0)
        slots += prim_slots
        entry = {"mesh": mi, "name": mesh.get("name"), "primitiveTargetSlots": prim_slots}
        if isinstance(names, list):
            entry["targetNames"] = names
            entry["targetNameCount"] = len(names)
            for nm in names:
                if nm not in union:
                    union.append(nm)
            if prim_slots and len(names) != prim_slots:
                entry["nameCountMismatchWithSlots"] = True
        else:
            entry["targetNames"] = None
        per_mesh.append(entry)
    return {"totalPrimitiveSlots": slots, "namedUnion": union, "namedUnionCount": len(union),
            "perMesh": per_mesh}


def scene_bounds(doc, binary):
    """World-space AABB over every primitive's POSITION, reusing the anatomy probe's maths."""
    nodes = doc.get("nodes") or []
    parents = {}
    for i, node in enumerate(nodes):
        for child in node.get("children") or []:
            parents[child] = i
    world = [None] * len(nodes)

    def world_of(i):
        if world[i] is not None:
            return world[i]
        local = anat.node_matrix(nodes[i])
        world[i] = anat.mat_mul(world_of(parents[i]), local) if i in parents else local
        return world[i]

    for i in range(len(nodes)):
        world_of(i)

    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    saw_any = False
    accessors = doc.get("accessors") or []
    # Map each mesh index to the first node that references it; that node supplies the
    # primitives' world transform, so a multi-mesh file places each mesh under the right node
    # instead of collapsing everything onto whichever mesh appears first.
    node_by_mesh = {}
    for ni, node in enumerate(nodes):
        mi = node.get("mesh")
        if isinstance(mi, int) and mi not in node_by_mesh:
            node_by_mesh[mi] = ni
    for mi, mesh in enumerate(doc.get("meshes") or []):
        mn = node_by_mesh.get(mi)
        mworld = world_of(mn) if mn is not None else None
        for prim in mesh.get("primitives") or []:
            pos_idx = (prim.get("attributes") or {}).get("POSITION")
            if not isinstance(pos_idx, int) or not 0 <= pos_idx < len(accessors):
                continue
            acc = accessors[pos_idx]
            # Prefer the accessor min/max if authored (cheap and authoritative).
            amin, amax = acc.get("min"), acc.get("max")
            if isinstance(amin, list) and isinstance(amax, list) and len(amin) == len(amax) == 3:
                pts = [(amin[0], amin[1], amin[2]), (amax[0], amax[1], amax[2])]
                for axis_combo in _corners(amin, amax):
                    pts.append(axis_combo)
            else:
                try:
                    pts = anat.accessor_values(doc, binary, pos_idx)
                except ValueError:
                    continue
            for p in pts:
                wp = anat.transform_point(mworld, tuple(p)) if mworld is not None else tuple(p)
                for k in range(3):
                    lo[k] = min(lo[k], wp[k])
                    hi[k] = max(hi[k], wp[k])
                saw_any = True
    if not saw_any:
        return None
    size = [hi[k] - lo[k] for k in range(3)]
    largest = max(range(3), key=lambda k: size[k])
    return {"min": lo, "max": hi, "size": size,
            "estimatedHeight": size[1],
            "largestAxis": ["x", "y", "z"][largest]}


def _corners(amin, amax):
    return [(amin[0], amin[1], amax[2]), (amin[0], amax[1], amin[2]),
            (amax[0], amin[1], amin[2]), (amax[0], amax[1], amax[2])]


def probe(path):
    doc, binary = anat.read_glb(path)
    geo = geometry_summary(doc)
    mats, mat_union = material_records(doc)
    imgs = image_records(doc, binary)
    skins = skin_records(doc)
    morphs = morph_target_names(doc)
    bounds = scene_bounds(doc, binary)
    anim_rep = anim.probe(path)  # reuse the animation probe's clip/duration/morph-channel work
    asset = doc.get("asset") or {}

    honesty_notes = [r["note"] for r in imgs if r["note"]]
    unsupported = sorted({r["note"] for r in imgs if "unsupported" in r["note"] or "cannot" in r["note"]
                          or "not embedded" in r["note"] or "unrecognised" in r["note"]})

    return {
        "file": str(path),
        "bytes": os.path.getsize(path),
        "generator": asset.get("generator"),
        "gltfVersion": asset.get("version"),
        "assetExtras": asset.get("extras"),
        "extensionsUsed": doc.get("extensionsUsed") or [],
        "extensionsRequired": doc.get("extensionsRequired") or [],
        "counts": {
            "nodes": len(doc.get("nodes") or []),
            "meshes": len(doc.get("meshes") or []),
            "primitives": geo["primitiveCount"],
            "materials": len(mats),
            "images": len(imgs),
            "textures": len(doc.get("textures") or []),
            "skins": len(skins),
            "animations": len(doc.get("animations") or []),
        },
        "geometry": {
            "totalVertices": geo["totalVertices"],
            "totalTriangles": geo["totalTriangles"],
            "maxTexcoordSets": geo["maxTexcoordSets"],
            "anyNormals": geo["anyNormals"],
            "anyTangents": geo["anyTangents"],
            "anyVertexColors": geo["anyVertexColors"],
            "skinningAttributesPresent": geo["skinningAttributesPresent"],
            "perPrimitive": geo["primitives"],
        },
        "materials": mats,
        "materialExtensionUnion": mat_union,
        "images": imgs,
        "skins": skins,
        "morphTargets": morphs,
        "animations": anim_rep.get("animations") or [],
        "sceneBounds": bounds,
        "honestyNotes": honesty_notes,
        "unsupportedMeasurements": unsupported,
    }


def summarise(rep):
    lines = []
    lines.append("%s" % rep["file"])
    lines.append("  glTF %s | generator: %s" % (rep["gltfVersion"], rep["generator"] or "<none>"))
    c = rep["counts"]
    lines.append("  nodes=%d meshes=%d primitives=%d materials=%d images=%d textures=%d skins=%d animations=%d"
                 % (c["nodes"], c["meshes"], c["primitives"], c["materials"], c["images"], c["textures"],
                    c["skins"], c["animations"]))
    if rep["extensionsUsed"]:
        lines.append("  extensionsUsed: %s" % ", ".join(rep["extensionsUsed"]))
    if rep["extensionsRequired"]:
        lines.append("  extensionsRequired: %s" % ", ".join(rep["extensionsRequired"]))
    g = rep["geometry"]
    lines.append("  GEOMETRY totalVertices=%d totalTriangles=%d uvSets(max)=%d normals=%s tangents=%s colors=%s skinningAttrs=%s"
                 % (g["totalVertices"], g["totalTriangles"], g["maxTexcoordSets"],
                    g["anyNormals"], g["anyTangents"], g["anyVertexColors"], g["skinningAttributesPresent"]))
    if rep["materialExtensionUnion"]:
        lines.append("  MATERIAL extensions across materials: %s" % ", ".join(rep["materialExtensionUnion"]))
    for im in rep["images"]:
        dims = ("%dx%d ch=%s" % (im["width"], im["height"], im["channels"])) if im["width"] else "dims=? (%s)" % im["note"]
        lines.append("  IMAGE[%d] %s %s bytes=%s %s" % (im["index"], im["mimeType"] or "?", dims,
                                                        im.get("byteLength"), im.get("name") or ""))
    for sk in rep["skins"]:
        lines.append("  SKIN[%d] %r joints=%d inverseBind=%s(count=%s) root=%r maxDepth=%s"
                     % (sk["index"], sk["name"], sk["jointCount"], sk["inverseBindMatricesPresent"],
                        sk["inverseBindMatrixCount"], sk["skeletonRoot"], sk["maxJointDepthFromSceneRoot"]))
        if sk["jointNames"]:
            lines.append("      jointNames: %s" % ", ".join(sk["jointNames"]))
    m = rep["morphTargets"]
    lines.append("  MORPH primitiveSlots=%d named=%d%s"
                 % (m["totalPrimitiveSlots"], m["namedUnionCount"],
                    (": %s" % ", ".join(m["namedUnion"][:20])) if m["namedUnion"] else ""))
    for a in rep["animations"]:
        paths = ",".join(sorted((a.get("paths") or {}).keys()))
        lines.append("  CLIP[%s] %r %.3fs channels=%d morphWeightChannels=%d paths=%s"
                     % (a.get("animation"), a.get("name"), a.get("durationSeconds") or 0.0,
                        a.get("channelCount") or 0, a.get("morphWeightChannels") or 0, paths or "-"))
    if rep["sceneBounds"]:
        b = rep["sceneBounds"]
        lines.append("  BOUNDS size=(%.4f, %.4f, %.4f) estimatedHeight=%.4f largestAxis=%s"
                     % (b["size"][0], b["size"][1], b["size"][2], b["estimatedHeight"], b["largestAxis"]))
    if rep["unsupportedMeasurements"]:
        lines.append("  NOTE some measurements unavailable: %s" % "; ".join(sorted(set(rep["unsupportedMeasurements"]))))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="GLB quality-targets probe (see module docstring).")
    ap.add_argument("files", nargs="+", help="GLB files to inspect")
    ap.add_argument("--json", dest="json_path", help="write the combined structured report here")
    args = ap.parse_args()

    reports = []
    failures = 0
    for f in args.files:
        try:
            rep = probe(f)
        except Exception as exc:
            print("!! %s: %s" % (f, exc), file=sys.stderr)
            failures += 1
            continue
        reports.append(rep)
        print(summarise(rep))
        print()

    if args.json_path:
        with open(args.json_path, "w", encoding="utf-8") as fh:
            json.dump(reports, fh, indent=2, sort_keys=True)
        print("wrote %s" % args.json_path)

    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
