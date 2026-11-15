#!/usr/bin/env python3
"""Publishes small copies of emulator screenshots as check runs on the tested commit.

Why: the screenshots a workflow uploads are artifacts, and an artifact is a zip file behind a
redirect. A reviewer, or an agent that cannot follow that redirect, cannot look at them. The output
text of a check run is served by the GitHub API itself, so a picture stored there as base64 can be
read back with nothing but the API:

    gh api repos/OWNER/REPO/commits/SHA/check-runs \
        --jq '.check_runs[] | select(.name | startswith("Emulator screenshot")) | "\\(.id) \\(.name)"'
    gh api repos/OWNER/REPO/check-runs/ID --jq '.output.summary + .output.text' \
        | tr -d '\\n' | base64 -d > screenshot.jpg

The check run is "neutral": it is evidence, not a verdict, and no gate reads it.

Usage: publish_screenshot_check.py <check name> <image> [<check name> <image> ...]
Environment: GITHUB_TOKEN (needs checks: write), GITHUB_REPOSITORY, HEAD_SHA.
Needs Pillow. A missing image is skipped; nothing here fails the job that calls it.
"""
import base64
import hashlib
import io
import json
import os
import sys
import urllib.request

FIELD_LIMIT = 60000  # characters; the API allows 65535 for each of summary and text


def encode(path):
    from PIL import Image

    image = Image.open(path).convert("RGB")
    for width, quality in ((720, 70), (720, 55), (540, 65), (540, 50), (432, 55), (360, 50), (270, 45)):
        height = max(1, round(image.height * width / image.width))
        small = image.resize((width, height), Image.LANCZOS) if width < image.width else image
        buffer = io.BytesIO()
        small.save(buffer, "JPEG", quality=quality, optimize=True)
        data = buffer.getvalue()
        text = base64.b64encode(data).decode("ascii")
        if len(text) <= 2 * FIELD_LIMIT:
            return data, text, small.size, quality
    raise RuntimeError("the screenshot does not fit a check run even at 270 pixels wide")


def publish(name, path):
    data, text, size, quality = encode(path)
    title = "%dx%d JPEG q%d, %d bytes, sha256 %s; base64 = summary + text" % (
        size[0], size[1], quality, len(data), hashlib.sha256(data).hexdigest())
    output = {"title": title, "summary": text[:FIELD_LIMIT]}
    if len(text) > FIELD_LIMIT:
        output["text"] = text[FIELD_LIMIT:]
    body = json.dumps({
        "name": name,
        "head_sha": os.environ["HEAD_SHA"],
        "status": "completed",
        "conclusion": "neutral",
        "output": output,
    }).encode("utf-8")
    request = urllib.request.Request(
        "https://api.github.com/repos/%s/check-runs" % os.environ["GITHUB_REPOSITORY"],
        data=body,
        method="POST",
        headers={
            "Authorization": "Bearer " + os.environ["GITHUB_TOKEN"],
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
        },
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        created = json.loads(response.read().decode("utf-8"))
    print("published check run %s '%s': %s" % (created.get("id"), name, title))


def main():
    arguments = sys.argv[1:]
    if len(arguments) < 2 or len(arguments) % 2:
        print(__doc__)
        return 2
    for name, path in zip(arguments[0::2], arguments[1::2]):
        if not os.path.isfile(path) or os.path.getsize(path) == 0:
            print("skipped '%s': %s is missing or empty" % (name, path))
            continue
        try:
            publish(name, path)
        except Exception as failure:  # evidence only: never fail the job over it
            print("could not publish '%s': %s" % (name, failure))
    return 0


if __name__ == "__main__":
    sys.exit(main())
