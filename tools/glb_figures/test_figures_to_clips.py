#!/usr/bin/env python3
"""Checks figures_to_clips.py against the generated showcase of make_test_figure.py.

Run: python3 tools/glb_figures/test_figures_to_clips.py   (needs numpy)
Exit code 0 and "ALL PASSED" when every check holds.
"""
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FAILURES = []


def check(condition, what):
    print(('PASS  ' if condition else 'FAIL  ') + what)
    if not condition:
        FAILURES.append(what)


def run(*arguments):
    return subprocess.run([sys.executable, '-B'] + list(arguments), capture_output=True, text=True)


def read(path):
    data = open(path, 'rb').read()
    length, = struct.unpack_from('<I', data, 12)
    return json.loads(data[20:20 + length].decode('utf-8'))


def duration(doc, animation):
    return max(doc['accessors'][s['input']]['max'][0] for s in animation['samplers'])


def main():
    with tempfile.TemporaryDirectory() as folder:
        showcase = os.path.join(folder, 'showcase.glb')
        single = os.path.join(folder, 'single.glb')
        out = os.path.join(folder, 'out.glb')
        check(run(os.path.join(HERE, 'make_test_figure.py'), 'showcase', showcase).returncode == 0, 'the showcase test file is written')
        check(run(os.path.join(HERE, 'make_test_figure.py'), 'single', single).returncode == 0, 'the single test figure is written')
        # The instrumented test GlbClipPreviewTest opens a committed copy of the single figure.
        committed = os.path.join(HERE, '..', '..', 'app', 'src', 'androidTest', 'assets', 'rigged_test_figure.glb')
        check(os.path.isfile(committed) and open(committed, 'rb').read() == open(single, 'rb').read(),
              'app/src/androidTest/assets/rigged_test_figure.glb is what "make_test_figure.py single" writes')
        # The retargeting tests (host and Android) open a committed copy of the humanoid pack.
        pack = os.path.join(folder, 'pack.glb')
        check(run(os.path.join(HERE, 'make_humanoid_pack.py'), pack).returncode == 0, 'the humanoid motion pack is written')
        committed_pack = os.path.join(HERE, '..', '..', 'app', 'src', 'androidTest', 'assets', 'humanoid_motion_pack.glb')
        check(os.path.isfile(committed_pack) and open(committed_pack, 'rb').read() == open(pack, 'rb').read(),
              'app/src/androidTest/assets/humanoid_motion_pack.glb is what make_humanoid_pack.py writes')
        humanoid = read(pack)
        check([a['name'] for a in humanoid['animations']] == ['Arms Up', 'Knee Lift', 'Turn Left', 'Hop', 'Stride Ahead'],
              'the pack has the clips Arms Up, Knee Lift, Turn Left, Hop and Stride Ahead')
        check(len(humanoid['skins'][0]['joints']) == 27 and all(humanoid['nodes'][j]['name'].startswith('mixamorig:') for j in humanoid['skins'][0]['joints']),
              'its 27 joints carry the Mixamo prefix')
        check(all(sum(1 for c in a['channels'] if c['target']['path'] == 'translation') == 1 for a in humanoid['animations']),
              'and every clip moves the hips with one translation channel')
        figure = read(single)
        check([a['name'] for a in figure['animations']] == ['Wave', 'March', 'Bow'], 'the single figure has the clips Wave, March and Bow')
        check([round(duration(figure, a), 3) for a in figure['animations']] == [2.0, 1.0, 2.0], 'which last 2, 1 and 2 seconds')
        check(all(len(a['channels']) == 7 for a in figure['animations']), 'and each sets all seven joints, so no clip leaves a pose behind for the next')
        source = read(showcase)
        check(len(source['skins']) == 4 and len(source['animations']) == 1, 'the showcase has four copies and one animation track')

        result = run(os.path.join(HERE, 'figures_to_clips.py'), showcase, out, '--verify', '--names', 'March,Bow,Wave')
        print(result.stdout.rstrip())
        check(result.returncode == 0, 'the tool accepts the showcase')
        found = re.search(r'at 5 times of each clip: ([0-9.]+)', result.stdout)
        check(bool(found) and float(found.group(1)) < 1e-5, 'every clip moves the kept figure exactly as its copy moved')
        check('repeats every 1.00 s' in result.stdout, 'the copy that repeats one second three times is cut to one period')
        check('ends where it starts' in result.stdout, 'the copy that returns to its start is kept whole')
        check('cut off in the source' in result.stdout, 'the copy that was cut off is reported as jumping')

        doc = read(out)
        names = [a['name'] for a in doc['animations']]
        check(names == ['March', 'Bow', 'Wave', 'Rest pose'], 'clips are named in order from left to right, with the rest pose last: %s' % names)
        lengths = [round(duration(doc, a), 3) for a in doc['animations']]
        check(lengths == [1.0, 2.0, 1.3, 1.0], 'clip lengths are 1.0, 2.0, 1.3 and 1.0 seconds: %s' % lengths)
        check(len(doc['skins']) == 1 and len(doc['meshes']) == 1, 'one skin and one mesh remain')
        roots = doc['scenes'][0]['nodes']
        mesh_nodes = [i for i, n in enumerate(doc['nodes']) if 'mesh' in n]
        check(all(i in roots for i in mesh_nodes) and all('matrix' not in doc['nodes'][i] for i in mesh_nodes),
              'the mesh is a root node without a transform')
        low = [min(doc['accessors'][p['attributes']['POSITION']]['min'][a] for p in doc['meshes'][0]['primitives']) for a in range(3)]
        high = [max(doc['accessors'][p['attributes']['POSITION']]['max'][a] for p in doc['meshes'][0]['primitives']) for a in range(3)]
        check(abs(low[0] + high[0]) < 0.02 and abs(low[1]) < 1e-6, 'the figure stands at the origin: x %.3f..%.3f, floor %.3f' % (low[0], high[0], low[1]))
        check(high[2] > 0.19 and low[2] > -0.12, 'its feet and nose point to +Z: z %.3f..%.3f' % (low[2], high[2]))
        matrix = doc['nodes'][0].get('matrix', [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])
        check(abs(matrix[0] - 1) < 1e-6 and abs(matrix[10] - 1) < 1e-6 and abs(matrix[12]) < 1e-6 and abs(matrix[14]) < 1e-6,
              'the kept copy, which stood turned and to one side, now stands straight at the origin')
        channels = {len(a['channels']) for a in doc['animations']}
        check(channels == {7}, 'every clip sets the rotation of all seven joints and nothing else: %s' % channels)
        check('modified' in doc['asset'].get('extras', {}), 'the change is noted in asset.extras')

        refused = run(os.path.join(HERE, 'figures_to_clips.py'), single, out)
        check(refused.returncode == 2 and 'refused' in refused.stderr, 'a file with one figure is refused with a message')
        report = run(os.path.join(HERE, 'figures_to_clips.py'), showcase, '--report')
        check(report.returncode == 0 and 'wrote' not in report.stdout, '--report writes nothing')
        kept = run(os.path.join(HERE, 'figures_to_clips.py'), showcase, out, '--no-trim')
        check(kept.returncode == 0 and round(duration(read(out), read(out)['animations'][0]), 3) == 3.0, '--no-trim keeps the repeated clip at 3 seconds')
    if FAILURES:
        print('%d FAILURE(S)' % len(FAILURES))
        return 1
    print('ALL PASSED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
