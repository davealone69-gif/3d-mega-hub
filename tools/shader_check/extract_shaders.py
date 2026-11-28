#!/usr/bin/env python3
"""Writes every GLSL source embedded in af_shaders_gl.h to a file of its own.

    python3 extract_shaders.py af_shaders_gl.h OUTDIR

OUTDIR/<name>.<vert|frag|comp>, as the GLES driver receives them. Standard library only.
"""
import os
import re
import sys


def main(argv):
    if len(argv) != 3:
        raise SystemExit(__doc__)
    header = open(argv[1], encoding='utf-8').read()
    os.makedirs(argv[2], exist_ok=True)
    count = 0
    for found in re.finditer(r'static const char kSrc_(\w+)_(vert|frag|comp)\[\] =\n(.*?)\n;\n', header, re.S):
        lines = []
        for line in found.group(3).split('\n'):
            line = line.strip()
            if not (line.startswith('"') and line.endswith('"')):
                raise SystemExit('extract_shaders: unexpected line in %s: %s' % (found.group(1), line))
            lines.append(line[1:-1].encode('utf-8').decode('unicode_escape'))
        path = os.path.join(argv[2], '%s.%s' % (found.group(1), found.group(2)))
        with open(path, 'w', encoding='utf-8') as out:
            out.write(''.join(lines))
        print(path)
        count += 1
    if count == 0:
        raise SystemExit('extract_shaders: no shader found in ' + argv[1])
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
