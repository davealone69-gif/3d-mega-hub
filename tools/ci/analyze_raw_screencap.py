#!/usr/bin/env python3
"""Summarise a raw `adb exec-out screencap` frame without any third-party libraries.

Android writes a header (width, height, format[, colorspace]) of 32-bit little-endian words
followed by tightly packed pixels. The exact word count varies by platform version, so the
header length is inferred from the total payload size instead of assumed.
"""
import struct
import sys

FORMAT_RGBA_8888 = 1
FORMAT_RGBX_8888 = 2
FORMAT_RGB_888 = 3
FORMAT_RGB_565 = 4


def infer_header(width, height):
    for words, bpp in ((4, 4), (3, 4), (2, 4), (4, 3), (3, 3), (2, 2), (3, 2), (4, 2)):
        if width <= 0 or height <= 0:
            continue
        total = words * 4 + width * height * bpp
        yield words, bpp, total


def main(path):
    data = open(path, "rb").read()
    if len(data) < 12:
        print("raw frame: empty or truncated (%d bytes)" % len(data))
        return

    width, height, fmt = struct.unpack_from("<III", data, 0)
    print("raw frame: %d bytes, header says %dx%d format=%d" % (len(data), width, height, fmt))

    if not (0 < width <= 8192 and 0 < height <= 8192):
        print("raw frame: implausible dimensions, treating as unusable")
        return

    header = None
    for words, bpp, total in infer_header(width, height):
        if total == len(data):
            header = (words * 4, bpp)
            break

    if header is None:
        print("raw frame: size does not match any known header/pixel layout")
        return

    offset, bpp = header
    print("raw frame: header=%d bytes, %d bytes/pixel (inferred)" % (offset, bpp))

    pixels = data[offset:]
    count = width * height
    if count == 0:
        return

    total_r = total_g = total_b = 0
    non_black = 0
    bright = 0
    quantised = set()

    step = 1
    if count > 400000:  # subsample large frames, still deterministic
        step = 4

    sampled = 0
    for i in range(0, count, step):
        base = i * bpp
        if bpp >= 3:
            r, g, b = pixels[base], pixels[base + 1], pixels[base + 2]
        else:  # RGB_565
            lo = pixels[base]
            hi = pixels[base + 1]
            value = lo | (hi << 8)
            r = ((value >> 11) & 0x1F) << 3
            g = ((value >> 5) & 0x3F) << 2
            b = (value & 0x1F) << 3
        sampled += 1
        total_r += r
        total_g += g
        total_b += b
        if r > 8 or g > 8 or b > 8:
            non_black += 1
        if r + g + b > 96:
            bright += 1
        quantised.add((r >> 4, g >> 4, b >> 4))
        if len(quantised) > 4096:
            quantised = set(list(quantised)[:2048])

    print("sampled pixels: %d of %d" % (sampled, count))
    print("mean rgb: (%d, %d, %d)" % (total_r // sampled, total_g // sampled, total_b // sampled))
    print("non-black fraction: %.4f" % (non_black / sampled))
    print("bright-pixel fraction: %.4f" % (bright / sampled))
    print("distinct quantised colours: %d" % len(quantised))

    if non_black / sampled > 0.01 and len(quantised) > 8:
        print("verdict: the captured frame contains rendered content")
    elif non_black / sampled > 0.01:
        print("verdict: the frame is not black but is almost flat in colour")
    else:
        print("verdict: the captured frame is essentially black")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: analyze_raw_screencap.py <frame.raw>")
        sys.exit(2)
    main(sys.argv[1])
