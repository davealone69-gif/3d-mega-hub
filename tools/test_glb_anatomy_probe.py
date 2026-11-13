#!/usr/bin/env python3
"""Small synthetic regression tests for glb_anatomy_probe.py."""
import importlib.util
import json
import struct
import tempfile
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("glb_anatomy_probe.py")
spec = importlib.util.spec_from_file_location("glb_anatomy_probe", MODULE_PATH)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def make_fixture(path, scale=1.0):
    positions = [(0, 0, 0), (scale, 0, 0), (0, 2*scale, scale)]
    vertex_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    index_bytes = struct.pack("<3H", 0, 1, 2)
    binary = vertex_bytes + index_bytes
    nodes = [{"name": "BodyMesh", "mesh": 0}]
    landmarks = {
        "Crown": (0, 1.9, 0), "Chin": (0, 1.7, 0),
        "Left_Eye": (-.1, 1.82, 0), "Right_Eye": (.1, 1.82, 0),
        "Left_Temple": (-.2, 1.8, 0), "Right_Temple": (.2, 1.8, 0),
        "Left_Shoulder": (-.3, 1.4, 0), "Right_Shoulder": (.3, 1.4, 0),
        "Left_Hip": (-.2, .9, 0), "Right_Hip": (.2, .9, 0),
        "Left_Fingertip": (-.9, 1.2, 0), "Right_Fingertip": (.9, 1.2, 0),
    }
    for name, xyz in landmarks.items():
        nodes.append({"name": name, "translation": [v*scale for v in xyz]})
    doc = {
        "asset": {"version": "2.0", "generator": "anatomy-probe-test"},
        "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [0, 0, 0], "max": [scale, 2*scale, scale]},
            {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(vertex_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": len(vertex_bytes), "byteLength": len(index_bytes), "target": 34963},
        ],
        "buffers": [{"byteLength": len(binary)}],
    }
    json_chunk = json.dumps(doc, separators=(",", ":")).encode()
    json_chunk += b" " * ((-len(json_chunk)) % 4)
    binary += b"\x00" * ((-len(binary)) % 4)
    total = 12 + 8 + len(json_chunk) + 8 + len(binary)
    glb = (struct.pack("<4sII", b"glTF", 2, total)
           + struct.pack("<I4s", len(json_chunk), b"JSON") + json_chunk
           + struct.pack("<I4s", len(binary), b"BIN\x00") + binary)
    path.write_bytes(glb)


def main():
    with tempfile.TemporaryDirectory() as td:
        a, b = Path(td)/"reference.glb", Path(td)/"candidate.glb"
        make_fixture(a, 1.0)
        make_fixture(b, 1.1)
        ref, candidate = probe.analyse(a), probe.analyse(b)
        assert ref["triangles"] == 1
        assert ref["ratios"]["head_height_over_body_height"] is not None
        assert abs(ref["ratios"]["shoulder_width_over_hip_width"] - 1.5) < 1e-6
        assert abs(ref["ratios"]["interpupillary_over_head_width"] - 0.5) < 1e-6
        assert abs(ref["ratios"]["arm_span_over_body_height"] - 0.9) < 1e-6
        assert candidate["bounds_m"]["height"] > ref["bounds_m"]["height"]
        assert not ref["missing_landmarks"]
        print("PASS: GLB parsing, world transforms, named landmarks, and proportion ratios")


if __name__ == "__main__":
    main()
