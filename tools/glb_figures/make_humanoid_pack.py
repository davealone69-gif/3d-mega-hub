#!/usr/bin/env python3
"""Writes a humanoid "motion pack" made of boxes, as a GLB. Pure Python, no dependencies.

  make_humanoid_pack.py OUT.glb

It is a test asset for the retargeting of animations onto the app's own avatars
(af_retarget.cpp), built to be as awkward as the files people really have:

  - joint names with the Mixamo prefix ("mixamorig:LeftForeArm"), plus end joints that have
    to be ignored ("mixamorig:HeadTop_End", "mixamorig:LeftToe_End");
  - a T pose, where the app's avatars rest with their arms down;
  - centimetres, under an "Armature" node scaled by 0.01;
  - bone-aligned joints: every joint's own Y axis runs along its bone, so the rest rotations
    are anything but the identity;
  - the whole figure stands off the origin and is turned 25 degrees, so nothing can be read
    off the axes: where it faces has to be found from its feet.

Five clips, each with an effect a test can measure on the avatar (the numbers are in
avatarforge-engine/src/test/cpp/animation_system_test.cpp):

  Arms Up        both arms from the T pose to straight overhead and back      2 s, loops
  Knee Lift      the LEFT knee up to hip height, shin hanging, and back       2 s, loops
  Turn Left      the hips turn a quarter turn to the figure's left, and stay  2 s
  Hop            the hips rise 20 cm and come down                            1 s, loops
  Stride Ahead   the legs swing while the hips travel 1 m forward             2 s

A copy is kept at app/src/androidTest/assets/humanoid_motion_pack.glb for the instrumented
test and the host test. After a change here, write that copy again;
test_figures_to_clips.py fails while the two differ.
"""
import math
import sys

from make_test_figure import FACES, Blob, write

RATE = 30
PREFIX = 'mixamorig:'

# name, parent, position in the figure's own space: centimetres, T pose, facing +Z, left is +X
JOINTS = [
    ('Hips', None, (0, 100, 0)),
    ('Spine', 'Hips', (0, 110, 0)),
    ('Spine1', 'Spine', (0, 122, 0)),
    ('Spine2', 'Spine1', (0, 135, 0)),
    ('Neck', 'Spine2', (0, 150, 0)),
    ('Head', 'Neck', (0, 159, 0)),
    ('HeadTop_End', 'Head', (0, 177, 0)),
    ('LeftShoulder', 'Spine2', (6, 145, 0)),
    ('LeftArm', 'LeftShoulder', (18, 145, 0)),
    ('LeftForeArm', 'LeftArm', (45, 145, 0)),
    ('LeftHand', 'LeftForeArm', (70, 145, 0)),
    ('LeftHandMiddle1', 'LeftHand', (79, 145, 0)),
    ('RightShoulder', 'Spine2', (-6, 145, 0)),
    ('RightArm', 'RightShoulder', (-18, 145, 0)),
    ('RightForeArm', 'RightArm', (-45, 145, 0)),
    ('RightHand', 'RightForeArm', (-70, 145, 0)),
    ('RightHandMiddle1', 'RightHand', (-79, 145, 0)),
    ('LeftUpLeg', 'Hips', (9, 94, 0)),
    ('LeftLeg', 'LeftUpLeg', (9, 52, 0)),
    ('LeftFoot', 'LeftLeg', (9, 10, 0)),
    ('LeftToeBase', 'LeftFoot', (9, 2, 13)),
    ('LeftToe_End', 'LeftToeBase', (9, 2, 20)),
    ('RightUpLeg', 'Hips', (-9, 94, 0)),
    ('RightLeg', 'RightUpLeg', (-9, 52, 0)),
    ('RightFoot', 'RightLeg', (-9, 10, 0)),
    ('RightToeBase', 'RightFoot', (-9, 2, 13)),
    ('RightToe_End', 'RightToeBase', (-9, 2, 20)),
]

# joint, box minimum, box maximum (figure space, centimetres), material
BOXES = [
    ('Hips', (-15, 92, -9), (15, 108, 9), 0),
    ('Spine1', (-16, 108, -10), (16, 148, 10), 0),
    ('Head', (-9, 152, -9), (9, 176, 9), 1),
    ('Head', (-2, 160, 9), (2, 165, 13), 3),                  # nose: the front
    ('LeftArm', (18, 141, -4), (45, 149, 4), 1),
    ('LeftForeArm', (45, 142, -3.5), (70, 148, 3.5), 1),
    ('LeftHand', (70, 143, -4), (84, 147, 4), 3),
    ('RightArm', (-45, 141, -4), (-18, 149, 4), 1),
    ('RightForeArm', (-70, 142, -3.5), (-45, 148, 3.5), 1),
    ('RightHand', (-84, 143, -4), (-70, 147, 4), 2),
    ('LeftUpLeg', (3, 52, -6), (15, 94, 6), 2),
    ('LeftLeg', (4, 10, -5), (14, 52, 5), 2),
    ('LeftFoot', (4, 0, -6), (14, 10, 20), 3),                 # feet: the front
    ('RightUpLeg', (-15, 52, -6), (-3, 94, 6), 2),
    ('RightLeg', (-14, 10, -5), (-4, 52, 5), 2),
    ('RightFoot', (-14, 0, -6), (-4, 10, 20), 3),
]

MATERIALS = [
    ('Shirt', (0.10, 0.45, 0.35)),
    ('Skin', (0.80, 0.55, 0.40)),
    ('Trousers', (0.14, 0.14, 0.30)),
    ('Marker', (0.85, 0.10, 0.10)),
]

# Where the whole figure stands in the scene: off the origin and turned about the vertical.
PLACE = (0.30, 0.0, -0.20)
PLACE_YAW = math.radians(25.0)
SCALE = 0.01


# ---------------------------------------------------------------- small maths -------------
def q_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def q_conj(q):
    return (-q[0], -q[1], -q[2], q[3])


def q_rotate(q, v):
    x, y, z, _ = q_mul(q_mul(q, (v[0], v[1], v[2], 0.0)), q_conj(q))
    return (x, y, z)


def q_axis(axis, degrees):
    half = math.radians(degrees) / 2.0
    s = math.sin(half)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half))


def q_from_axes(x, y, z):
    """Quaternion of the rotation whose columns are the unit vectors x, y, z."""
    m00, m10, m20 = x
    m01, m11, m21 = y
    m02, m12, m22 = z
    trace = m00 + m11 + m22
    if trace > 0.0:
        k = math.sqrt(trace + 1.0) * 2.0
        q = ((m21 - m12) / k, (m02 - m20) / k, (m10 - m01) / k, 0.25 * k)
    elif m00 > m11 and m00 > m22:
        k = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
        q = (0.25 * k, (m01 + m10) / k, (m02 + m20) / k, (m21 - m12) / k)
    elif m11 > m22:
        k = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
        q = ((m01 + m10) / k, 0.25 * k, (m12 + m21) / k, (m02 - m20) / k)
    else:
        k = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
        q = ((m02 + m20) / k, (m12 + m21) / k, 0.25 * k, (m10 - m01) / k)
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(v):
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / n, v[1] / n, v[2] / n)


def to_world(point_cm):
    """A point of the figure (centimetres) where it stands in the scene (metres)."""
    x, y, z = (c * SCALE for c in point_cm)
    c, s = math.cos(PLACE_YAW), math.sin(PLACE_YAW)
    return (x * c + z * s + PLACE[0], y + PLACE[1], -x * s + z * c + PLACE[2])


def invert_rigid_scaled(rotation, translation, scale):
    """Inverse of translate x rotate x uniform scale, as a column-major 4x4."""
    inv = q_conj(rotation)
    k = 1.0 / scale
    cols = [q_rotate(inv, axis) for axis in ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
    t = q_rotate(inv, translation)
    return (cols[0][0] * k, cols[0][1] * k, cols[0][2] * k, 0.0,
            cols[1][0] * k, cols[1][1] * k, cols[1][2] * k, 0.0,
            cols[2][0] * k, cols[2][1] * k, cols[2][2] * k, 0.0,
            -t[0] * k, -t[1] * k, -t[2] * k, 1.0)


# ---------------------------------------------------------------- the skeleton ------------
INDEX = {name: k for k, (name, _, _) in enumerate(JOINTS)}
POSITION = {name: tuple(float(c) for c in p) for name, _, p in JOINTS}
PARENT = {name: parent for name, parent, _ in JOINTS}
CHILDREN = {}
for _name, _parent, _ in JOINTS:
    if _parent:
        CHILDREN.setdefault(_parent, []).append(_name)


def rest_orientation(name):
    """Bone-aligned: the joint's Y axis runs to its first child (or on from its parent)."""
    if name in CHILDREN:
        along = unit(sub(POSITION[CHILDREN[name][0]], POSITION[name]))
    else:
        along = unit(sub(POSITION[name], POSITION[PARENT[name]]))
    reference = (0.0, 0.0, 1.0) if abs(along[2]) < 0.9 else (1.0, 0.0, 0.0)
    x = unit(cross(along, reference))
    z = cross(x, along)
    return q_from_axes(x, along, z)


REST = {name: rest_orientation(name) for name, _, _ in JOINTS}


def local_rotation(name, delta=(0.0, 0.0, 0.0, 1.0)):
    """The joint's rotation relative to its parent when it has turned by `delta` (a rotation
    about the figure's own axes) on top of whatever its parent does."""
    turned = q_mul(delta, REST[name])
    parent = PARENT[name]
    return turned if parent is None else q_mul(q_conj(REST[parent]), turned)


def local_translation(name, offset=(0.0, 0.0, 0.0)):
    parent = PARENT[name]
    if parent is None:
        return tuple(POSITION[name][a] + offset[a] for a in range(3))
    return q_rotate(q_conj(REST[parent]), sub(POSITION[name], POSITION[parent]))


# ---------------------------------------------------------------- the clips ---------------
X, Y, Z = (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)


def there_and_back(t, seconds):
    return 0.5 * (1.0 - math.cos(2.0 * math.pi * t / seconds))


def arms_up(t):
    u = there_and_back(t, 2.0)
    return {'LeftArm': q_axis(Z, 90.0 * u), 'RightArm': q_axis(Z, -90.0 * u)}, (0.0, 0.0, 0.0)


def knee_lift(t):
    u = there_and_back(t, 2.0)
    return {'LeftUpLeg': q_axis(X, -90.0 * u), 'LeftLeg': q_axis(X, 90.0 * u)}, (0.0, 0.0, 0.0)


def turn_left(t):
    return {'Hips': q_axis(Y, 90.0 * t / 2.0)}, (0.0, 0.0, 0.0)


def hop(t):
    return {}, (0.0, 20.0 * there_and_back(t, 1.0), 0.0)


def stride_ahead(t):
    swing = 25.0 * math.sin(2.0 * math.pi * t)
    return {'LeftUpLeg': q_axis(X, -swing), 'RightUpLeg': q_axis(X, swing)}, (0.0, 0.0, 100.0 * t / 2.0)


CLIPS = [('Arms Up', arms_up, 2.0), ('Knee Lift', knee_lift, 2.0), ('Turn Left', turn_left, 2.0),
         ('Hop', hop, 1.0), ('Stride Ahead', stride_ahead, 2.0)]


def build():
    blob = Blob()
    place = q_axis(Y, math.degrees(PLACE_YAW))
    nodes = [{'name': 'Stage', 'translation': list(PLACE), 'rotation': list(place), 'children': [1]},
             {'name': 'Armature', 'scale': [SCALE, SCALE, SCALE], 'children': [2 + INDEX['Hips']]}]
    first_joint = len(nodes)
    for name, parent, _ in JOINTS:
        nodes.append({'name': PREFIX + name, 'translation': list(local_translation(name)),
                      'rotation': list(local_rotation(name)), 'scale': [1.0, 1.0, 1.0]})
        if parent:
            nodes[first_joint + INDEX[parent]].setdefault('children', []).append(len(nodes) - 1)

    primitives = []
    for material in range(len(MATERIALS)):
        positions, normals, joints, weights, indices = [], [], [], [], []
        for joint, low, high, box_material in BOXES:
            if box_material != material:
                continue
            for normal, corners in FACES:
                base = len(positions)
                for pick in corners:
                    positions.append(to_world(tuple(high[a] if pick[a] else low[a] for a in range(3))))
                    normals.append(q_rotate(place, normal))
                    joints.append((INDEX[joint], 0, 0, 0))
                    weights.append((1.0, 0.0, 0.0, 0.0))
                indices += [base, base + 1, base + 2, base, base + 2, base + 3]
        primitives.append({
            'attributes': {
                'POSITION': blob.add('f', positions, 'VEC3', 5126, 34962, bounds=True),
                'NORMAL': blob.add('f', normals, 'VEC3', 5126, 34962),
                'JOINTS_0': blob.add('H', joints, 'VEC4', 5123, 34962),
                'WEIGHTS_0': blob.add('f', weights, 'VEC4', 5126, 34962)},
            'indices': blob.add('H', indices, 'SCALAR', 5123, 34963), 'material': material, 'mode': 4})
    # Each joint's rest transform in the scene: placed, scaled, then bone-aligned.
    matrices = [invert_rigid_scaled(q_mul(place, REST[name]), to_world(POSITION[name]), SCALE) for name, _, _ in JOINTS]
    skin = {'joints': [first_joint + k for k in range(len(JOINTS))],
            'inverseBindMatrices': blob.add('f', matrices, 'MAT4', 5126), 'skeleton': first_joint}
    nodes.append({'name': 'Figure', 'mesh': 0, 'skin': 0})

    animations = []
    for clip_name, motion, seconds in CLIPS:
        frames = int(round(seconds * RATE))
        times = blob.add('f', [i / RATE for i in range(frames + 1)], 'SCALAR', 5126, bounds=True)
        rotations = {name: [] for name, _, _ in JOINTS}
        hips = []
        for i in range(frames + 1):
            deltas, offset = motion(i / RATE)
            for name, _, _ in JOINTS:
                rotations[name].append(local_rotation(name, deltas.get(name, (0.0, 0.0, 0.0, 1.0))))
            hips.append(local_translation('Hips', offset))
        samplers, channels = [], []
        for name, _, _ in JOINTS:
            if name.endswith('_End'):
                continue
            samplers.append({'input': times, 'output': blob.add('f', rotations[name], 'VEC4', 5126), 'interpolation': 'LINEAR'})
            channels.append({'sampler': len(samplers) - 1, 'target': {'node': first_joint + INDEX[name], 'path': 'rotation'}})
        samplers.append({'input': times, 'output': blob.add('f', hips, 'VEC3', 5126), 'interpolation': 'LINEAR'})
        channels.append({'sampler': len(samplers) - 1, 'target': {'node': first_joint + INDEX['Hips'], 'path': 'translation'}})
        animations.append({'name': clip_name, 'samplers': samplers, 'channels': channels})

    while len(blob.data) % 4:
        blob.data.append(0)
    doc = {'asset': {'version': '2.0', 'generator': '3D Mega Hub humanoid motion pack (make_humanoid_pack.py)'},
           'scene': 0, 'scenes': [{'nodes': [0, len(nodes) - 1]}], 'nodes': nodes,
           'meshes': [{'name': 'Figure', 'primitives': primitives}], 'skins': [skin],
           'materials': [{'name': name, 'pbrMetallicRoughness': {'baseColorFactor': [r, g, b, 1.0], 'metallicFactor': 0.0, 'roughnessFactor': 0.7}}
                         for name, (r, g, b) in MATERIALS],
           'animations': animations,
           'accessors': blob.accessors, 'bufferViews': blob.views, 'buffers': [{'byteLength': len(blob.data)}]}
    return doc, bytes(blob.data)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    doc, data = build()
    size = write(sys.argv[1], doc, data)
    print('wrote %s: %d bytes, %d nodes, %d joints, %d animation(s)' % (
        sys.argv[1], size, len(doc['nodes']), len(doc['skins'][0]['joints']), len(doc['animations'])))
    return 0


if __name__ == '__main__':
    sys.exit(main())
