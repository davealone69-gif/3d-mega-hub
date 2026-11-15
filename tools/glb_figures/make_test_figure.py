#!/usr/bin/env python3
"""Writes a small rigged test figure made of boxes, as a GLB. Pure Python, no dependencies.

Two forms:

  make_test_figure.py single OUT.glb
      One figure, seven joints, three named clips ("Wave", "March", "Bow"). A copy is kept at
      app/src/androidTest/assets/rigged_test_figure.glb: the instrumented test
      GlbClipPreviewTest opens it in the app to show that the glTF viewer lists the clips of
      an imported file by name and that the picture follows the skeleton. After a change
      here, write that copy again; test_figures_to_clips.py fails while the two differ.

  make_test_figure.py showcase OUT.glb
      Four copies of the figure in a row, each turned a little, inside ONE animation track:
      one marches (the same second three times over), one bows (ends where it starts), one
      waves but is cut off before the end, one stands still. Node names carry the numeric
      suffixes exporters add. This is the input figures_to_clips.py is written for, and
      test_figures_to_clips.py checks the tool against it.

The figure faces +Z: its feet and a nose point that way. Units are metres.
"""
import json
import math
import struct
import sys

RATE = 30

# name, parent, translation from the parent
JOINTS = [
    ('Hips', None, (0.0, 0.95, 0.0)),
    ('Spine', 'Hips', (0.0, 0.05, 0.0)),
    ('Head', 'Spine', (0.0, 0.50, 0.0)),
    ('LeftArm', 'Spine', (0.26, 0.42, 0.0)),
    ('RightArm', 'Spine', (-0.26, 0.42, 0.0)),
    ('LeftLeg', 'Hips', (0.10, 0.0, 0.0)),
    ('RightLeg', 'Hips', (-0.10, 0.0, 0.0)),
]

# joint, box minimum, box maximum (in the figure's own space), material
BOXES = [
    ('Hips', (-0.18, 0.88, -0.10), (0.18, 1.00, 0.10), 0),
    ('Spine', (-0.20, 1.00, -0.11), (0.20, 1.46, 0.11), 0),
    ('Head', (-0.10, 1.50, -0.10), (0.10, 1.72, 0.10), 1),
    ('Head', (-0.02, 1.58, 0.10), (0.02, 1.63, 0.15), 3),          # nose: the front
    ('LeftArm', (0.21, 0.86, -0.05), (0.31, 1.44, 0.05), 1),
    ('RightArm', (-0.31, 0.86, -0.05), (-0.21, 1.44, 0.05), 1),
    ('LeftLeg', (0.03, 0.06, -0.07), (0.17, 0.95, 0.07), 2),
    ('RightLeg', (-0.17, 0.06, -0.07), (-0.03, 0.95, 0.07), 2),
    ('LeftLeg', (0.03, 0.00, -0.07), (0.17, 0.06, 0.20), 3),       # feet: the front
    ('RightLeg', (-0.17, 0.00, -0.07), (-0.03, 0.06, 0.20), 3),
]

MATERIALS = [
    ('Shirt', (0.10, 0.32, 0.75)),
    ('Skin', (0.80, 0.55, 0.40)),
    ('Trousers', (0.12, 0.12, 0.16)),
    ('Marker', (0.85, 0.10, 0.10)),
]

FACES = [  # normal, four corners as (x, y, z) picks of minimum (0) or maximum (1), counter-clockwise
    ((1, 0, 0), [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)]),
    ((-1, 0, 0), [(0, 0, 1), (0, 1, 1), (0, 1, 0), (0, 0, 0)]),
    ((0, 1, 0), [(0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0)]),
    ((0, -1, 0), [(0, 0, 1), (0, 0, 0), (1, 0, 0), (1, 0, 1)]),
    ((0, 0, 1), [(1, 0, 1), (1, 1, 1), (0, 1, 1), (0, 0, 1)]),
    ((0, 0, -1), [(0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)]),
]


def turn(axis, angle):
    s = math.sin(angle / 2.0)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(angle / 2.0))


def march(t):
    swing = 0.6 * math.sin(2.0 * math.pi * t)
    return {'LeftLeg': turn((1, 0, 0), swing), 'RightLeg': turn((1, 0, 0), -swing),
            'LeftArm': turn((1, 0, 0), -swing), 'RightArm': turn((1, 0, 0), swing)}


def bow(t):
    bend = 0.9 * 0.5 * (1.0 - math.cos(2.0 * math.pi * t / 2.0))
    return {'Spine': turn((1, 0, 0), bend), 'Head': turn((1, 0, 0), 0.3 * bend)}


def wave(t):
    lift = min(1.0, t / 0.4) if t < 1.6 else max(0.0, (2.0 - t) / 0.4)
    shake = 0.35 * math.sin(2.0 * math.pi * 2.0 * t) * lift
    return {'RightArm': turn((0, 0, 1), -2.5 * lift + shake)}


MOTIONS = {'March': (march, 1.0), 'Bow': (bow, 2.0), 'Wave': (wave, 2.0)}


class Blob:
    def __init__(self):
        self.data = bytearray()
        self.views = []
        self.accessors = []

    def add(self, fmt, rows, kind, component, target=None, bounds=False):
        while len(self.data) % 4:
            self.data.append(0)
        start = len(self.data)
        width = len(rows[0]) if isinstance(rows[0], (tuple, list)) else 1
        for row in rows:
            if width == 1:
                self.data += struct.pack('<' + fmt, row)
            else:
                self.data += struct.pack('<%d%s' % (width, fmt), *row)
        view = {'buffer': 0, 'byteOffset': start, 'byteLength': len(self.data) - start}
        if target:
            view['target'] = target
        self.views.append(view)
        accessor = {'bufferView': len(self.views) - 1, 'componentType': component, 'count': len(rows), 'type': kind}
        if bounds:
            columns = list(zip(*rows)) if width > 1 else [rows]
            accessor['min'] = [float(min(c)) for c in columns]
            accessor['max'] = [float(max(c)) for c in columns]
        self.accessors.append(accessor)
        return len(self.accessors) - 1


def rotate_y(point, angle):
    c, s = math.cos(angle), math.sin(angle)
    return (point[0] * c + point[2] * s, point[1], -point[0] * s + point[2] * c)


def bind_positions():
    world = {}
    for name, parent, offset in JOINTS:
        base = world[parent] if parent else (0.0, 0.0, 0.0)
        world[name] = (base[0] + offset[0], base[1] + offset[1], base[2] + offset[2])
    return world


def build(copies, showcase):
    """copies: list of (x, z, yaw, [(motion name, seconds to play)] or None).

    A showcase puts every copy's motion into one animation track, names nodes with exporter
    suffixes, starts at frame 1 and writes still translation and scale channels too, as the
    files it imitates do. Otherwise each motion becomes a clip of its own with rotations only.
    """
    blob = Blob()
    nodes, meshes, skins, animations = [], [], [], []
    channels, samplers = [], []
    scene_nodes = []
    suffixes = showcase
    joint_index = {name: k for k, (name, _, _) in enumerate(JOINTS)}
    bind = bind_positions()
    for number, (x, z, yaw, plays) in enumerate(copies):
        def label(name):
            return '%s_%d' % (name, len(nodes) + 17) if suffixes else name
        c, s = math.cos(yaw), math.sin(yaw)
        armature = len(nodes)
        nodes.append({'name': label('Armature' if number == 0 else 'Armature.%03d' % number), 'children': []})
        if (x, z, yaw) != (0.0, 0.0, 0.0):
            nodes[armature]['matrix'] = [c, 0.0, -s, 0.0, 0.0, 1.0, 0.0, 0.0, s, 0.0, c, 0.0, x, 0.0, z, 1.0]
        scene_nodes.append(armature)
        first_joint = len(nodes)
        for name, parent, offset in JOINTS:
            nodes.append({'name': label(name), 'translation': list(offset), 'rotation': [0.0, 0.0, 0.0, 1.0], 'scale': [1.0, 1.0, 1.0]})
            index = len(nodes) - 1
            owner = armature if parent is None else first_joint + joint_index[parent]
            nodes[owner].setdefault('children', []).append(index)
        # The mesh: one primitive per material, vertices in world space at the copy's place.
        primitives = []
        for material in range(len(MATERIALS)):
            positions, normals, joints, weights, indices = [], [], [], [], []
            for joint, low, high, box_material in BOXES:
                if box_material != material:
                    continue
                for normal, corners in FACES:
                    base = len(positions)
                    for pick in corners:
                        point = tuple(high[a] if pick[a] else low[a] for a in range(3))
                        turned = rotate_y(point, yaw)
                        positions.append((turned[0] + x, turned[1], turned[2] + z))
                        normals.append(rotate_y(normal, yaw))
                        joints.append((joint_index[joint], 0, 0, 0))
                        weights.append((1.0, 0.0, 0.0, 0.0))
                    indices += [base, base + 1, base + 2, base, base + 2, base + 3]
            primitives.append({
                'attributes': {
                    'POSITION': blob.add('f', positions, 'VEC3', 5126, 34962, bounds=True),
                    'NORMAL': blob.add('f', normals, 'VEC3', 5126, 34962),
                    'JOINTS_0': blob.add('H', joints, 'VEC4', 5123, 34962),
                    'WEIGHTS_0': blob.add('f', weights, 'VEC4', 5126, 34962)},
                'indices': blob.add('H', indices, 'SCALAR', 5123, 34963), 'material': material, 'mode': 4})
        meshes.append({'name': 'Figure', 'primitives': primitives})
        matrices = []
        for name, _, _ in JOINTS:
            p = rotate_y(bind[name], yaw)
            px, py, pz = p[0] + x, p[1], p[2] + z
            # inverse of (translate by p) x (turn by yaw): turn by -yaw, then translate back
            matrices.append((c, 0.0, s, 0.0, 0.0, 1.0, 0.0, 0.0, -s, 0.0, c, 0.0,
                             -(c * px - s * pz), -py, -(s * px + c * pz), 1.0))
        skins.append({'joints': [first_joint + k for k in range(len(JOINTS))],
                      'inverseBindMatrices': blob.add('f', matrices, 'MAT4', 5126), 'skeleton': first_joint})
        nodes.append({'name': label('Object'), 'mesh': len(meshes) - 1, 'skin': len(skins) - 1})
        if showcase:
            nodes[armature]['children'].append(len(nodes) - 1)   # as exporters do; the transform has no effect
        else:
            scene_nodes.append(len(nodes) - 1)
        for motion, seconds in plays or []:
            function, period = MOTIONS[motion]
            frames = int(round(seconds * RATE))
            first = 1 if showcase else 0
            time_accessor = blob.add('f', [(i + first) / RATE for i in range(frames + 1)], 'SCALAR', 5126, bounds=True)
            tracks = {}
            for i in range(frames + 1):
                pose = function((i / RATE) % period if seconds > period else i / RATE)
                for name, _, _ in JOINTS:
                    tracks.setdefault(name, []).append(pose.get(name, (0.0, 0.0, 0.0, 1.0)))
            for name, _, offset in JOINTS:
                node = first_joint + joint_index[name]
                paths = [('rotation', tracks[name], 'VEC4')]
                if showcase:
                    paths += [('translation', [offset] * (frames + 1), 'VEC3'), ('scale', [(1.0, 1.0, 1.0)] * (frames + 1), 'VEC3')]
                for path, rows, kind in paths:
                    samplers.append({'input': time_accessor, 'output': blob.add('f', rows, kind, 5126), 'interpolation': 'LINEAR'})
                    channels.append({'sampler': len(samplers) - 1, 'target': {'node': node, 'path': path}})
            if not showcase:
                animations.append({'name': motion, 'samplers': samplers, 'channels': channels})
                channels, samplers = [], []
    if showcase and channels:
        animations.append({'name': 'Animation', 'samplers': samplers, 'channels': channels})
    while len(blob.data) % 4:
        blob.data.append(0)
    doc = {'asset': {'version': '2.0', 'generator': '3D Mega Hub test figure (make_test_figure.py)'},
           'scene': 0, 'scenes': [{'nodes': scene_nodes}], 'nodes': nodes, 'meshes': meshes, 'skins': skins,
           'materials': [{'name': name, 'pbrMetallicRoughness': {'baseColorFactor': [r, g, b, 1.0], 'metallicFactor': 0.0, 'roughnessFactor': 0.7}}
                         for name, (r, g, b) in MATERIALS],
           'accessors': blob.accessors, 'bufferViews': blob.views, 'buffers': [{'byteLength': len(blob.data)}]}
    if animations:
        doc['animations'] = animations
    return doc, bytes(blob.data)


def write(path, doc, data):
    text = json.dumps(doc, separators=(',', ':')).encode('utf-8')
    text += b' ' * (-len(text) % 4)
    with open(path, 'wb') as handle:
        handle.write(struct.pack('<4sII', b'glTF', 2, 12 + 8 + len(text) + 8 + len(data)))
        handle.write(struct.pack('<I4s', len(text), b'JSON'))
        handle.write(text)
        handle.write(struct.pack('<I4s', len(data), b'BIN\0'))
        handle.write(data)
    return 12 + 8 + len(text) + 8 + len(data)


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ('single', 'showcase'):
        print(__doc__)
        return 2
    if sys.argv[1] == 'single':
        doc, data = build([(0.0, 0.0, 0.0, [('Wave', 2.0), ('March', 1.0), ('Bow', 2.0)])], showcase=False)
    else:
        doc, data = build([
            (-1.5, 0.0, 0.35, [('March', 3.0)]),        # the same second three times over
            (-0.5, 0.0, -0.20, [('Bow', 2.0)]),         # ends where it starts
            (0.5, 0.0, 0.10, [('Wave', 1.3)]),          # cut off before its end
            (1.5, 0.0, 0.50, None),                     # stands still: the one to keep
        ], showcase=True)
    size = write(sys.argv[2], doc, data)
    print('wrote %s: %d bytes, %d nodes, %d skins, %d animation(s)' % (
        sys.argv[2], size, len(doc['nodes']), len(doc['skins']), len(doc.get('animations', []))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
