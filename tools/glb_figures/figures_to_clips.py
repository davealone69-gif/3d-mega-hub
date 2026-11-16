#!/usr/bin/env python3
"""Turns a "showcase" GLB into an avatar file: one figure with one named clip per animation.

Some downloadable models are a showcase scene: the same rigged figure copied N times, each copy
playing a different animation inside ONE animation track, all standing in rows. A viewer that
opens such a file shows a crowd of small figures and has one clip to play.

This tool keeps ONE of the copies and turns every other copy's motion into a clip on it:

  - the copies must share a skeleton (same joints in the same order; names may differ by a
    numeric suffix), which is what copies of one rig have;
  - the kept figure is the one that is not animated (usually a T-pose), or the first one;
  - it is moved to the origin, stood facing +Z (the glTF front), and its meshes become root nodes
    with no transform, so a viewer frames it correctly;
  - each clip starts at time 0 and is sampled at the source's frame rate. A clip that repeats
    itself is cut to one period, so it loops without a jump. A clip that was cut off in the
    source keeps its jump: the missing end cannot be invented;
  - channels that never change in any clip (bone lengths, scale) become the rest pose;
  - meshes, materials and textures of the other copies are left out. Image bytes are copied as
    they are; author, licence and source in `asset.extras` are kept and a note of the change is
    added.

It needs numpy and nothing else. It reads and writes .glb only. Not supported, and refused with
a message: morph targets, sparse accessors, more than one buffer, cubic-spline samplers.

Usage:
  figures_to_clips.py IN.glb OUT.glb [--animation 0] [--base SKIN] [--names "a,b,c"]
                      [--no-trim] [--no-face-forward] [--verify] [--report]

  --report   only print what the file contains (figures, clip lengths, loop points)
  --verify   after writing, skin both files on the CPU at several times of every clip and
             print the largest distance between corresponding vertices

Nothing here is specific to any model, and no model is stored in this repository.
"""
import argparse
import json
import re
import struct
import sys

import numpy as np

COMPONENT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT2': 4, 'MAT3': 9, 'MAT4': 16}
REST = {'translation': [0.0, 0.0, 0.0], 'rotation': [0.0, 0.0, 0.0, 1.0], 'scale': [1.0, 1.0, 1.0]}


class Refused(Exception):
    pass


# ----------------------------------------------------------------------------------- reading

class Glb:
    def __init__(self, path):
        data = open(path, 'rb').read()
        if len(data) < 20 or data[:4] != b'glTF':
            raise Refused('%s is not a binary glTF file' % path)
        json_length, json_type = struct.unpack_from('<II', data, 12)
        if json_type != 0x4E4F534A:
            raise Refused('the first chunk is not JSON')
        self.doc = json.loads(data[20:20 + json_length].decode('utf-8'))
        offset = 20 + json_length
        if offset + 8 > len(data):
            raise Refused('the file has no binary chunk')
        bin_length, bin_type = struct.unpack_from('<II', data, offset)
        if bin_type != 0x004E4942:
            raise Refused('the second chunk is not BIN')
        self.bin = memoryview(data)[offset + 8:offset + 8 + bin_length]
        if len(self.doc.get('buffers', [])) != 1 or 'uri' in self.doc['buffers'][0]:
            raise Refused('only a single embedded buffer is supported')

    def view_bytes(self, index):
        view = self.doc['bufferViews'][index]
        start = view.get('byteOffset', 0)
        return bytes(self.bin[start:start + view['byteLength']])

    def accessor(self, index):
        """The accessor's values as stored (no normalisation), shape (count, width)."""
        acc = self.doc['accessors'][index]
        if 'sparse' in acc:
            raise Refused('sparse accessors are not supported')
        if 'bufferView' not in acc:
            raise Refused('an accessor without a buffer view is not supported')
        view = self.doc['bufferViews'][acc['bufferView']]
        dtype = np.dtype(COMPONENT[acc['componentType']]).newbyteorder('<')
        width = WIDTH[acc['type']]
        start = view.get('byteOffset', 0) + acc.get('byteOffset', 0)
        stride = view.get('byteStride', 0)
        size = dtype.itemsize * width
        if stride and stride != size:
            raw = np.frombuffer(self.bin, dtype=np.uint8, count=stride * (acc['count'] - 1) + size, offset=start)
            pick = (np.arange(acc['count'])[:, None] * stride + np.arange(size)[None, :]).ravel()
            return raw[pick].view(dtype).reshape(acc['count'], width)
        return np.frombuffer(self.bin, dtype=dtype, count=acc['count'] * width, offset=start).reshape(acc['count'], width)

    def floats(self, index):
        """The accessor's values as float64, with integer normalisation applied."""
        acc = self.doc['accessors'][index]
        values = self.accessor(index)
        if acc.get('normalized') and acc['componentType'] != 5126:
            info = np.iinfo(COMPONENT[acc['componentType']])
            return np.maximum(values.astype(np.float64) / info.max, -1.0)
        return values.astype(np.float64)


# ----------------------------------------------------------------------------------- maths

def quaternion_matrix(q):
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def local_matrix(node, override=None):
    override = override or {}
    if 'matrix' in node and not override:
        return np.array(node['matrix'], dtype=np.float64).reshape(4, 4).T
    if 'matrix' in node:
        raise Refused('an animated node stores a matrix instead of translation, rotation and scale')
    t = np.asarray(override.get('translation', node.get('translation', REST['translation'])), dtype=np.float64)
    r = np.asarray(override.get('rotation', node.get('rotation', REST['rotation'])), dtype=np.float64)
    s = np.asarray(override.get('scale', node.get('scale', REST['scale'])), dtype=np.float64)
    m = np.eye(4)
    m[:3, :3] = quaternion_matrix(r / np.linalg.norm(r)) * s[None, :]
    m[:3, 3] = t
    return m


def world_matrices(doc, overrides=None):
    nodes = doc['nodes']
    world = [None] * len(nodes)
    stack = [(root, np.eye(4)) for root in doc['scenes'][doc.get('scene', 0)]['nodes']]
    while stack:
        index, parent = stack.pop()
        world[index] = parent @ local_matrix(nodes[index], overrides.get(index) if overrides else None)
        for child in nodes[index].get('children', []):
            stack.append((child, world[index]))
    return world


def slerp_rows(a, b, f):
    """Spherical interpolation between rows of unit quaternions; f has shape (n, 1)."""
    dot = (a * b).sum(axis=1, keepdims=True)
    b = np.where(dot < 0, -b, b)
    dot = np.clip(np.abs(dot), 0.0, 1.0)
    theta = np.arccos(dot)
    sin = np.sin(theta)
    near = sin < 1e-6
    safe = np.where(near, 1.0, sin)
    wa = np.where(near, 1.0 - f, np.sin((1.0 - f) * theta) / safe)
    wb = np.where(near, f, np.sin(f * theta) / safe)
    out = wa * a + wb * b
    return out / np.linalg.norm(out, axis=1, keepdims=True)


def resample(times, values, grid, rotation):
    if len(times) == 1:
        return np.repeat(values[:1], len(grid), axis=0)
    index = np.clip(np.searchsorted(times, grid, side='right') - 1, 0, len(times) - 2)
    span = times[index + 1] - times[index]
    f = np.clip((grid - times[index]) / np.where(span > 0, span, 1.0), 0.0, 1.0)[:, None]
    a, b = values[index], values[index + 1]
    if rotation:
        return slerp_rows(a, b, f)
    return a + (b - a) * f


def continuous(quaternions):
    """Flips signs so that neighbouring keys lie in the same half-space."""
    out = quaternions.copy()
    for i in range(1, len(out)):
        if np.dot(out[i - 1], out[i]) < 0:
            out[i] = -out[i]
    return out


def plain_name(name):
    """A node name without the suffixes exporters add to make copies unique."""
    return re.sub(r'(\.\d{3})?(_\d+)?$', '', name or '')


# ----------------------------------------------------------------------------------- analysis

def skeleton_frame(scene, figure):
    """World matrix of the node the figure's root joint hangs from: where that copy stands."""
    joints = scene.glb.doc['skins'][figure]['joints']
    members = set(joints)
    parents = {child: index for index, node in enumerate(scene.nodes) for child in node.get('children', [])}
    top = [j for j in joints if parents.get(j) not in members][0]
    return scene.world[parents[top]] if top in parents else np.eye(4)


class Scene:
    """What the tool needs to know about the source file."""

    def __init__(self, glb, animation_index):
        self.glb = glb
        doc = glb.doc
        self.nodes = doc['nodes']
        skins = doc.get('skins', [])
        if len(skins) < 2:
            raise Refused('the file has %d skin(s); this tool is for several copies of one rigged figure' % len(skins))
        counts = {len(s['joints']) for s in skins}
        if len(counts) != 1:
            raise Refused('the skins have different joint counts (%s): they are not copies of one rig' % sorted(counts))
        self.joint_count = counts.pop()
        reference = [plain_name(self.nodes[j].get('name')) for j in skins[0]['joints']]
        for number, skin in enumerate(skins[1:], 1):
            names = [plain_name(self.nodes[j].get('name')) for j in skin['joints']]
            same = sum(1 for a, b in zip(reference, names) if a == b)
            if same < 0.9 * self.joint_count:
                raise Refused('skin %d shares only %d of %d joint names with skin 0' % (number, same, self.joint_count))
        self.joint_of = {}
        for number, skin in enumerate(skins):
            for k, joint in enumerate(skin['joints']):
                if joint in self.joint_of:
                    raise Refused('node %d is a joint of two skins: the figures are not separate' % joint)
                self.joint_of[joint] = (number, k)
        self.mesh_nodes = {n: [] for n in range(len(skins))}
        for index, node in enumerate(self.nodes):
            if 'mesh' in node and 'skin' in node:
                self.mesh_nodes[node['skin']].append(index)
        animations = doc.get('animations', [])
        if not animations:
            raise Refused('the file has no animation')
        if not 0 <= animation_index < len(animations):
            raise Refused('the file has %d animation(s); there is no number %d' % (len(animations), animation_index))
        animation = animations[animation_index]
        self.channels = {n: {} for n in range(len(skins))}
        self.ignored_channels = 0
        for channel in animation['channels']:
            target = channel['target']
            sampler = animation['samplers'][channel['sampler']]
            if target.get('node') not in self.joint_of or target['path'] not in REST:
                self.ignored_channels += 1
                continue
            if sampler.get('interpolation', 'LINEAR') == 'CUBICSPLINE':
                raise Refused('cubic-spline samplers are not supported')
            figure, k = self.joint_of[target['node']]
            times = glb.floats(sampler['input'])[:, 0]
            values = glb.floats(sampler['output'])
            self.channels[figure][(k, target['path'])] = (times, values)
        self.world = world_matrices(doc)
        # Where each copy stands: the place of the node its skeleton hangs from.
        self.positions = {number: skeleton_frame(self, number)[:3, 3] for number in range(len(skins))}

    def bind_bounds(self, figure):
        low = np.full(3, np.inf)
        high = np.full(3, -np.inf)
        for node in self.mesh_nodes[figure]:
            for primitive in self.glb.doc['meshes'][self.nodes[node]['mesh']]['primitives']:
                points = self.glb.floats(primitive['attributes']['POSITION'])
                low = np.minimum(low, points.min(axis=0))
                high = np.maximum(high, points.max(axis=0))
        return low, high

    def animated(self):
        """Animated figures: the row nearest a camera on +Z first, each row from left to right.

        Copies stand in a row when their places differ in Z by less than a quarter of a figure.
        """
        figures = [n for n in self.channels if self.channels[n] and self.mesh_nodes[n]]
        if not figures:
            return []
        low, high = self.bind_bounds(figures[0])
        tolerance = 0.25 * float((high - low).max())
        by_depth = sorted(figures, key=lambda n: -float(self.positions[n][2]))
        rows = [[by_depth[0]]]
        for figure in by_depth[1:]:
            if float(self.positions[rows[-1][0]][2]) - float(self.positions[figure][2]) > tolerance:
                rows.append([])
            rows[-1].append(figure)
        return [figure for row in rows for figure in sorted(row, key=lambda n: float(self.positions[n][0]))]

    def still(self):
        return [n for n in self.channels if not self.channels[n] and self.mesh_nodes[n]]

    def rest_value(self, figure, k, path):
        node = self.nodes[self.glb.doc['skins'][figure]['joints'][k]]
        if 'matrix' in node:
            raise Refused('joint %r stores a matrix instead of translation, rotation and scale' % node.get('name'))
        return np.asarray(node.get(path, REST[path]), dtype=np.float64)


class Clip:
    def __init__(self, scene, figure, trim):
        channels = scene.channels[figure]
        self.figure = figure
        starts = [times[0] for times, _ in channels.values()]
        ends = [times[-1] for times, _ in channels.values()]
        steps = np.concatenate([np.diff(times) for times, _ in channels.values() if len(times) > 1])
        self.rate = float(round(1.0 / np.median(steps))) if len(steps) else 30.0
        self.source_start = float(min(starts))
        frames = int(round((max(ends) - self.source_start) * self.rate))
        grid = self.source_start + np.arange(frames + 1) / self.rate
        self.tracks = {}
        for k in range(scene.joint_count):
            for path in REST:
                if (k, path) in channels:
                    times, values = channels[(k, path)]
                    track = resample(times, values, grid, path == 'rotation')
                    if path == 'rotation':
                        track = continuous(track / np.linalg.norm(track, axis=1, keepdims=True))
                else:
                    track = np.repeat(scene.rest_value(figure, k, path)[None, :], len(grid), axis=0)
                self.tracks[(k, path)] = track
        self.source_frames = frames
        self.end_jump = self.distance(0, frames)
        self.period = self.find_period() if trim else None
        if self.period:
            for key in self.tracks:
                self.tracks[key] = self.tracks[key][:self.period + 1]
        self.frames = self.period or frames
        self.loops = bool(self.period) or (self.end_jump[0] < 1e-4 and self.end_jump[1] < 1e-3)

    def distance(self, a, b):
        """(largest 1 - |q.q| over the joints, largest translation difference) between two frames."""
        rotation = 0.0
        translation = 0.0
        for (k, path), track in self.tracks.items():
            if path == 'rotation':
                rotation = max(rotation, 1.0 - abs(float(np.dot(track[a], track[b]))))
            elif path == 'translation':
                translation = max(translation, float(np.linalg.norm(track[a] - track[b])))
        return rotation, translation

    def find_period(self):
        """The first frame from which the motion repeats what it did from frame 0, or None.

        The source must show the repetition for at least half a second, and something must move
        in that stretch: a clip that only stands still at both ends is not a repetition.
        """
        rotations = np.stack([t for (k, p), t in self.tracks.items() if p == 'rotation'])
        moves = np.stack([t for (k, p), t in self.tracks.items() if p == 'translation'])
        total = rotations.shape[1] - 1
        least = max(8, int(round(0.5 * self.rate)))
        for period in range(8, total):
            overlap = total + 1 - period
            if overlap < least:
                break
            dots = np.abs((rotations[:, :overlap] * rotations[:, period:period + overlap]).sum(axis=2))
            if 1.0 - dots.min() > 1e-4:
                continue
            if np.abs(moves[:, :overlap] - moves[:, period:period + overlap]).max() > 1e-3:
                continue
            moving = 1.0 - np.abs((rotations[:, :overlap] * rotations[:, :1]).sum(axis=2)).min()
            if moving < 1e-3:
                continue
            return period
        return None

    @property
    def duration(self):
        return self.frames / self.rate


# ----------------------------------------------------------------------------------- writing

class Writer:
    def __init__(self):
        self.blob = bytearray()
        self.views = []
        self.accessors = []

    def view(self, data, target=None):
        while len(self.blob) % 4:
            self.blob.append(0)
        entry = {'buffer': 0, 'byteOffset': len(self.blob), 'byteLength': len(data)}
        if target:
            entry['target'] = target
        self.blob += data
        self.views.append(entry)
        return len(self.views) - 1

    def accessor(self, array, kind, component, target=None, bounds=False, normalized=False):
        array = np.ascontiguousarray(array, dtype=np.dtype(COMPONENT[component]).newbyteorder('<'))
        entry = {'bufferView': self.view(array.tobytes(), target), 'componentType': component,
                 'count': int(array.shape[0]), 'type': kind}
        if normalized:
            entry['normalized'] = True
        if bounds:
            entry['min'] = [float(v) for v in array.reshape(array.shape[0], -1).min(axis=0)]
            entry['max'] = [float(v) for v in array.reshape(array.shape[0], -1).max(axis=0)]
        self.accessors.append(entry)
        return len(self.accessors) - 1


def facing_rotation(scene, figure):
    """A turn about Y after which the figure's rest pose faces +Z, from its Left*/Right* joints."""
    left, right = [], []
    for joint in scene.glb.doc['skins'][figure]['joints']:
        name = plain_name(scene.nodes[joint].get('name'))
        point = scene.world[joint][:3, 3]
        if re.search(r'left', name, re.I):
            left.append(point)
        elif re.search(r'right', name, re.I):
            right.append(point)
    if not left or not right:
        return np.eye(3), None
    across = np.mean(left, axis=0) - np.mean(right, axis=0)
    across[1] = 0.0
    if np.linalg.norm(across) < 1e-4:
        return np.eye(3), None
    across /= np.linalg.norm(across)
    forward = np.cross(across, [0.0, 1.0, 0.0])          # left x up = forward
    angle = np.arctan2(forward[0], forward[2])            # turn of the forward direction away from +Z
    c, s = np.cos(-angle), np.sin(-angle)
    return np.array([[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]]), float(np.degrees(angle))


def convert(glb, scene, base, clips, names, face_forward, source_name):
    doc = glb.doc
    nodes = scene.nodes
    skin = doc['skins'][base]
    writer = Writer()

    # The rigid move that brings the kept figure to the origin, facing +Z.
    rotation, turned = facing_rotation(scene, base) if face_forward else (np.eye(3), None)
    # The centre of the rest pose is the mean of the joints: steadier than the bounding box,
    # which hair or an outstretched arm pulls to one side. The height is left as it is.
    joints_now = np.array([scene.world[j][:3, 3] for j in skin['joints']])
    centre = rotation @ joints_now.mean(axis=0)
    move = np.eye(4)
    move[:3, :3] = rotation
    move[:3, 3] = [-centre[0], 0.0, -centre[2]]

    # ---- nodes: one new root holding the joint tree, then the meshes as root nodes
    joint_set = set(skin['joints'])
    parent_of = {}
    for index, node in enumerate(nodes):
        for child in node.get('children', []):
            parent_of[child] = index
    tops = [j for j in skin['joints'] if parent_of.get(j) not in joint_set]
    if len(tops) != 1:
        raise Refused('the kept skeleton has %d root joints; one is expected' % len(tops))
    top = tops[0]
    above = parent_of.get(top)
    new_nodes = [{'name': 'Armature',
                  'matrix': [float(v) for v in (move @ (scene.world[above] if above is not None else np.eye(4))).T.ravel()],
                  'children': [1]}]
    new_index = {}
    order = []
    stack = [top]
    while stack:
        index = stack.pop()
        new_index[index] = len(new_nodes) + len(order)
        order.append(index)
        stack.extend(reversed(nodes[index].get('children', [])))
    for index in order:
        source = nodes[index]
        if 'mesh' in source or 'camera' in source:
            raise Refused('a node inside the skeleton carries a mesh or a camera; not supported')
        entry = {'name': plain_name(source.get('name'))}
        for key in ('translation', 'rotation', 'scale'):
            if key in source:
                entry[key] = [float(v) for v in source[key]]
        if 'matrix' in source:
            entry['matrix'] = source['matrix']
        children = [new_index[c] for c in source.get('children', [])]
        if children:
            entry['children'] = children
        new_nodes.append(entry)
    missing = [j for j in skin['joints'] if j not in new_index]
    if missing:
        raise Refused('%d joints of the kept skin are not under its root joint' % len(missing))

    # ---- materials, textures, images
    material_map, texture_map, image_map, sampler_map = {}, {}, {}, {}
    new_materials, new_textures, new_images, new_samplers = [], [], [], []

    def image(index):
        if index not in image_map:
            source = dict(doc['images'][index])
            if 'bufferView' in source:
                source['bufferView'] = writer.view(glb.view_bytes(source['bufferView']))
            image_map[index] = len(new_images)
            new_images.append(source)
        return image_map[index]

    def texture(index):
        if index not in texture_map:
            source = json.loads(json.dumps(doc['textures'][index]))
            if 'source' in source:
                source['source'] = image(source['source'])
            for extension in source.get('extensions', {}).values():
                if isinstance(extension, dict) and 'source' in extension:
                    extension['source'] = image(extension['source'])
            if 'sampler' in source:
                if source['sampler'] not in sampler_map:
                    sampler_map[source['sampler']] = len(new_samplers)
                    new_samplers.append(doc['samplers'][source['sampler']])
                source['sampler'] = sampler_map[source['sampler']]
            texture_map[index] = len(new_textures)
            new_textures.append(source)
        return texture_map[index]

    def remap_textures(value):
        if isinstance(value, dict):
            if 'index' in value and isinstance(value['index'], int) and set(value) <= {'index', 'texCoord', 'scale', 'strength', 'extensions', 'extras'}:
                value['index'] = texture(value['index'])
            for item in value.values():
                remap_textures(item)
        elif isinstance(value, list):
            for item in value:
                remap_textures(item)

    def material(index):
        if index not in material_map:
            source = json.loads(json.dumps(doc['materials'][index]))
            remap_textures(source)
            source['name'] = plain_name(source.get('name')) or 'material'
            material_map[index] = len(new_materials)
            new_materials.append(source)
        return material_map[index]

    # ---- meshes
    new_meshes = []
    mesh_node_indices = []
    for node_index in scene.mesh_nodes[base]:
        source_mesh = doc['meshes'][nodes[node_index]['mesh']]
        primitives = []
        for primitive in source_mesh['primitives']:
            if primitive.get('targets'):
                raise Refused('morph targets are not supported')
            attributes = {}
            for semantic, accessor_index in primitive['attributes'].items():
                acc = doc['accessors'][accessor_index]
                if semantic == 'POSITION':
                    points = glb.floats(accessor_index)
                    moved = points @ move[:3, :3].T + move[:3, 3]
                    attributes[semantic] = writer.accessor(moved, 'VEC3', 5126, 34962, bounds=True)
                elif semantic in ('NORMAL', 'TANGENT'):
                    vectors = glb.floats(accessor_index)
                    vectors = vectors.copy()
                    vectors[:, :3] = vectors[:, :3] @ move[:3, :3].T
                    attributes[semantic] = writer.accessor(vectors, acc['type'], 5126, 34962)
                elif semantic.startswith('JOINTS_') and 'WEIGHTS_' + semantic[7:] in primitive['attributes']:
                    # A joint index beside a zero weight has no effect; the glTF validator asks
                    # for it to be 0, and exporters often leave a stale index there.
                    joints = glb.accessor(accessor_index).copy()
                    joints[glb.floats(primitive['attributes']['WEIGHTS_' + semantic[7:]]) == 0] = 0
                    attributes[semantic] = writer.accessor(joints, acc['type'], acc['componentType'], 34962)
                else:
                    attributes[semantic] = writer.accessor(glb.accessor(accessor_index), acc['type'], acc['componentType'],
                                                           34962, normalized=bool(acc.get('normalized')))
            entry = {'attributes': attributes, 'mode': primitive.get('mode', 4)}
            if 'indices' in primitive:
                acc = doc['accessors'][primitive['indices']]
                entry['indices'] = writer.accessor(glb.accessor(primitive['indices'])[:, 0], 'SCALAR', acc['componentType'], 34963)
            if 'material' in primitive:
                entry['material'] = material(primitive['material'])
            primitives.append(entry)
        # Exporters name such meshes "Object_12"; the material's name says what the part is.
        label = plain_name(nodes[node_index].get('name')) or 'mesh'
        if primitives and 'material' in primitives[0]:
            label = new_materials[primitives[0]['material']]['name']
        new_meshes.append({'name': label, 'primitives': primitives})
        mesh_node_indices.append(len(new_nodes))
        new_nodes.append({'name': new_meshes[-1]['name'], 'mesh': len(new_meshes) - 1, 'skin': 0})

    # ---- skin
    inverse_bind = glb.floats(skin['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1)
    inverse_bind = inverse_bind @ np.linalg.inv(move)
    new_skin = {'joints': [new_index[j] for j in skin['joints']],
                'inverseBindMatrices': writer.accessor(inverse_bind.transpose(0, 2, 1).reshape(-1, 16), 'MAT4', 5126),
                'skeleton': new_index[top]}

    # ---- animation: what never changes in any clip becomes the rest pose
    joint_nodes = new_skin['joints']
    rest_only = {}
    for k in range(scene.joint_count):
        for path in REST:
            # Rotations always stay in the clips: the kept figure's own rest rotations are its
            # rest pose, and a clip must not depend on them.
            if path == 'rotation' or not clips:
                continue
            first = clips[0].tracks[(k, path)][0]
            if all(np.abs(clip.tracks[(k, path)] - first).max() < 1e-5 for clip in clips):
                rest_only[(k, path)] = first
    for (k, path), value in rest_only.items():
        new_nodes[joint_nodes[k]][path] = [float(v) for v in value]

    new_animations = []
    for number, clip in enumerate(clips):
        samplers, channels = [], []
        times = writer.accessor(np.arange(clip.frames + 1) / clip.rate, 'SCALAR', 5126, bounds=True)
        ends = None
        for k in range(scene.joint_count):
            for path in ('translation', 'rotation', 'scale'):
                if (k, path) in rest_only:
                    continue
                track = clip.tracks[(k, path)]
                kind = 'VEC4' if path == 'rotation' else 'VEC3'
                if np.abs(track - track[0]).max() < 1e-6:
                    if ends is None:
                        ends = writer.accessor(np.array([0.0, clip.duration]), 'SCALAR', 5126, bounds=True)
                    samplers.append({'input': ends, 'output': writer.accessor(track[[0, 0]], kind, 5126), 'interpolation': 'LINEAR'})
                else:
                    samplers.append({'input': times, 'output': writer.accessor(track, kind, 5126), 'interpolation': 'LINEAR'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': joint_nodes[k], 'path': path}})
        new_animations.append({'name': names[number], 'samplers': samplers, 'channels': channels})

    # ---- the rest pose as a last, still clip, so a viewer can return to it
    if clips:
        samplers, channels = [], []
        ends = writer.accessor(np.array([0.0, 1.0]), 'SCALAR', 5126, bounds=True)
        for k in range(scene.joint_count):
            for path in ('translation', 'rotation', 'scale'):
                if (k, path) in rest_only:
                    continue
                value = scene.rest_value(base, k, path)
                if path == 'rotation':
                    value = value / np.linalg.norm(value)
                samplers.append({'input': ends, 'output': writer.accessor(np.stack([value, value]), 'VEC4' if path == 'rotation' else 'VEC3', 5126),
                                 'interpolation': 'LINEAR'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': joint_nodes[k], 'path': path}})
        new_animations.append({'name': names[len(clips)], 'samplers': samplers, 'channels': channels})

    asset = {'version': '2.0', 'generator': 'figures_to_clips.py (3D Mega Hub tools); source written by %s' % doc.get('asset', {}).get('generator', 'unknown')}
    extras = dict(doc.get('asset', {}).get('extras', {}))
    extras['modified'] = ('Derived from %s: one of %d copies of the figure kept and moved to the origin; the single animation track '
                          'split into %d clips, each resampled at %g frames a second; a rest-pose clip added.'
                          % ('"%s"' % extras['title'] if extras.get('title') else source_name,
                             len(doc['skins']), len(clips), clips[0].rate if clips else 0))
    asset['extras'] = extras
    if 'copyright' in doc.get('asset', {}):
        asset['copyright'] = doc['asset']['copyright']
    out = {'asset': asset, 'scene': 0, 'scenes': [{'name': 'Scene', 'nodes': [0] + mesh_node_indices}],
           'nodes': new_nodes, 'meshes': new_meshes, 'skins': [new_skin], 'animations': new_animations,
           'accessors': writer.accessors, 'bufferViews': writer.views}
    if new_materials:
        out['materials'] = new_materials
    if new_textures:
        out['textures'] = new_textures
    if new_images:
        out['images'] = new_images
    if new_samplers:
        out['samplers'] = new_samplers
    used = sorted({name for name in doc.get('extensionsUsed', []) if name in json.dumps([new_materials, new_textures])})
    if used:
        out['extensionsUsed'] = used
        required = [name for name in doc.get('extensionsRequired', []) if name in used]
        if required:
            out['extensionsRequired'] = required
    while len(writer.blob) % 4:
        writer.blob.append(0)
    out['buffers'] = [{'byteLength': len(writer.blob)}]
    placement = move @ (scene.world[above] if above is not None else np.eye(4))
    return out, bytes(writer.blob), placement, turned


def write_glb(path, doc, blob):
    text = json.dumps(doc, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    text += b' ' * (-len(text) % 4)
    with open(path, 'wb') as handle:
        handle.write(struct.pack('<4sII', b'glTF', 2, 12 + 8 + len(text) + 8 + len(blob)))
        handle.write(struct.pack('<I4s', len(text), b'JSON'))
        handle.write(text)
        handle.write(struct.pack('<I4s', len(blob), b'BIN\0'))
        handle.write(blob)
    return 12 + 8 + len(text) + 8 + len(blob)


# ----------------------------------------------------------------------------------- checking

def skinned_points(glb, skin_index, mesh_nodes, overrides):
    """World positions of the vertices of the given mesh nodes, skinned on the CPU."""
    doc = glb.doc
    world = world_matrices(doc, overrides)
    skin = doc['skins'][skin_index]
    inverse_bind = glb.floats(skin['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1)
    joint_matrices = np.stack([world[j] @ inverse_bind[k] for k, j in enumerate(skin['joints'])])
    out = []
    for node in mesh_nodes:
        for primitive in doc['meshes'][doc['nodes'][node]['mesh']]['primitives']:
            points = glb.floats(primitive['attributes']['POSITION'])
            joints = glb.accessor(primitive['attributes']['JOINTS_0']).astype(np.int64)
            weights = glb.floats(primitive['attributes']['WEIGHTS_0'])
            weights = weights / np.maximum(weights.sum(axis=1, keepdims=True), 1e-12)
            blend = (joint_matrices[joints] * weights[:, :, None, None]).sum(axis=1)
            out.append(np.einsum('nij,nj->ni', blend[:, :3, :3], points) + blend[:, :3, 3])
    return np.concatenate(out)


def sample_overrides(glb, animation_index, time, only_nodes=None):
    doc = glb.doc
    animation = doc['animations'][animation_index]
    overrides = {}
    for channel in animation['channels']:
        target = channel['target']
        if only_nodes is not None and target['node'] not in only_nodes:
            continue
        sampler = animation['samplers'][channel['sampler']]
        times = glb.floats(sampler['input'])[:, 0]
        values = glb.floats(sampler['output'])
        grid = np.array([min(max(time, times[0]), times[-1])])
        overrides.setdefault(target['node'], {})[target['path']] = resample(times, values, grid, target['path'] == 'rotation')[0]
    return overrides


def verify(source, scene, animation_index, out_path, clips, placement):
    """Largest distance between a vertex of a source copy and the same vertex of the result.

    A source copy stands at its own place; `placement` is where the kept figure's skeleton
    stands in the result. The copy's vertices are carried over by the rigid move between the two.
    """
    result = Glb(out_path)
    out_meshes = [i for i, n in enumerate(result.doc['nodes']) if 'mesh' in n]
    worst = 0.0
    for number, clip in enumerate(clips):
        joints = set(source.doc['skins'][clip.figure]['joints'])
        carry = placement @ np.linalg.inv(skeleton_frame(scene, clip.figure))
        for fraction in (0.0, 0.23, 0.5, 0.77, 1.0):
            frame = int(round(fraction * clip.frames))
            time = frame / clip.rate
            original = skinned_points(source, clip.figure, scene.mesh_nodes[clip.figure],
                                      sample_overrides(source, animation_index, clip.source_start + time, joints))
            original = original @ carry[:3, :3].T + carry[:3, 3]
            converted = skinned_points(result, 0, out_meshes, sample_overrides(result, number, time))
            worst = max(worst, float(np.abs(original - converted).max()))
    return worst


def main():
    parser = argparse.ArgumentParser(description='One figure with one clip per animation, from a showcase GLB.')
    parser.add_argument('source')
    parser.add_argument('output', nargs='?')
    parser.add_argument('--animation', type=int, default=0)
    parser.add_argument('--base', type=int, default=None, help='skin number of the figure to keep')
    parser.add_argument('--names', default=None, help='comma-separated clip names, in the order of --report')
    parser.add_argument('--no-trim', action='store_true', help='keep repeated loops as they are')
    parser.add_argument('--no-face-forward', action='store_true')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--report', action='store_true')
    args = parser.parse_args()
    try:
        glb = Glb(args.source)
        scene = Scene(glb, args.animation)
        figures = scene.animated()
        still = scene.still()
        base = args.base if args.base is not None else (still[0] if still else figures[0])
        if base not in scene.mesh_nodes or not scene.mesh_nodes[base]:
            raise Refused('skin %d has no mesh' % base)
        clips = [Clip(scene, figure, not args.no_trim) for figure in figures]
        print('%s: %d copies of a figure with %d joints; %d animated, %d still; kept: skin %d%s' % (
            args.source, len(glb.doc['skins']), scene.joint_count, len(figures), len(still), base,
            '; %d channels on other nodes ignored' % scene.ignored_channels if scene.ignored_channels else ''))
        for number, clip in enumerate(clips, 1):
            place = scene.positions[clip.figure]
            if clip.period:
                how = 'repeats every %.2f s: cut to one period' % clip.duration
            elif clip.loops:
                how = 'ends where it starts'
            else:
                how = 'cut off in the source: jumps when it loops (rotation %.3f, translation %.3f m)' % clip.end_jump
            print('  clip %2d  skin %2d at x %5.1f z %5.1f  source %.2f s  ->  %.2f s  %s' % (
                number, clip.figure, place[0], place[2], clip.source_frames / clip.rate, clip.duration, how))
        if args.report:
            return 0
        if not args.output:
            raise Refused('no output file given')
        names = [n.strip() for n in args.names.split(',')] if args.names else ['Clip %02d' % n for n in range(1, len(clips) + 1)]
        if len(names) == len(clips):
            names.append('Rest pose')
        if len(names) != len(clips) + 1:
            raise Refused('%d names given for %d clips (one more may name the rest pose)' % (len(names), len(clips)))
        doc, blob, placement, turned = convert(glb, scene, base, clips, names, not args.no_face_forward, args.source.split('/')[-1])
        size = write_glb(args.output, doc, blob)
        print('wrote %s: %d bytes, %d nodes, %d meshes, %d clips%s' % (
            args.output, size, len(doc['nodes']), len(doc['meshes']), len(doc['animations']),
            '; turned %.1f degrees about Y to face +Z' % -turned if turned is not None else ''))
        if args.verify:
            worst = verify(glb, scene, args.animation, args.output, clips, placement)
            print('verify: largest distance between a source vertex and the converted one, '
                  'at 5 times of each clip: %.6f (in the model\'s units)' % worst)
        return 0
    except Refused as reason:
        print('refused: %s' % reason, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
