#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${GITHUB_TOKEN:-}" ]]; then
  echo "No GITHUB_TOKEN; skipping PR smoke failure comment."
  exit 0
fi

PR_NUMBER=$(gh pr list --head "${GITHUB_REF_NAME:-}" --state open --json number --jq '.[0].number // empty' || true)
if [[ -z "$PR_NUMBER" ]]; then
  echo "No open PR for ${GITHUB_REF_NAME:-}; skipping smoke failure comment."
  exit 0
fi

python3 - <<'PY' > /tmp/smoke-failure-body.md
import glob, os, re, xml.etree.ElementTree as ET

lines = ["### AvatarForge GLES GLB Render Smoke Failure Summary", ""]
xml_files = sorted(glob.glob("app/build/outputs/androidTest-results/connected/**/TEST-*.xml", recursive=True))
if xml_files:
    lines.append("**JUnit Test Results:**")
    for xf in xml_files:
        try:
            root = ET.parse(xf).getroot()
            for tc in root.findall(".//testcase"):
                name = f"{tc.attrib.get('classname', '')}.{tc.attrib.get('name', '')}"
                t = tc.attrib.get("time", "?")
                fail = tc.find("failure")
                err = tc.find("error")
                node = fail if fail is not None else err
                if node is not None:
                    msg = (node.text or node.attrib.get("message") or str(node.attrib)).strip()
                    lines.append(f"- `[FAIL]` `{name}` ({t}s)")
                    lines.append("```text")
                    lines.append(msg[:2500])
                    lines.append("```")
                else:
                    lines.append(f"- `[PASS]` `{name}` ({t}s)")
            for tag in ("system-err", "system-out"):
                el = root.find(tag)
                if el is not None and (el.text or "").strip():
                    lines.append(f"**JUnit `{tag}`:**")
                    lines.append("```text")
                    lines.append((el.text or "").strip()[-2000:])
                    lines.append("```")
        except Exception as e:
            lines.append(f"- Could not parse `{xf}`: {e}")
else:
    lines.append("_No JUnit XML files found in `app/build/outputs/androidTest-results/connected/`._")

diag = "avatarforge-render-smoke-evidence/renderer_diagnostics.json"
if os.path.isfile(diag):
    lines.append("")
    lines.append("**`renderer_diagnostics.json`:**")
    lines.append("```json")
    with open(diag, "r", errors="replace") as f:
        lines.append(f.read()[:1500])
    lines.append("```")

logcat_af = "avatarforge-render-smoke-evidence/avatarforge-logcat.txt"
if os.path.isfile(logcat_af):
    with open(logcat_af, "r", errors="replace") as f:
        tail = "".join(f.readlines()[-35:])
    if tail:
        lines.append("")
        lines.append("**`avatarforge-logcat.txt` (tail):**")
        lines.append("```text")
        lines.append(tail[-2500:])
        lines.append("```")

logcat_all = "avatarforge-render-smoke-evidence/logcat.txt"
if os.path.isfile(logcat_all):
    with open(logcat_all, "r", errors="replace") as f:
        all_lines = f.readlines()
    pat = re.compile(r"FATAL|Fatal signal|SIGSEGV|SIGABRT|AndroidRuntime|TestRunner|DEBUG   :|backtrace:|#0[0-9] pc|OutOfMemory|ANR in|died|WIN DEATH|Process com\.davemega", re.I)
    hits = [l for l in all_lines if pat.search(l)]
    if hits:
        lines.append("")
        lines.append("**`logcat.txt` crash/TestRunner excerpts (tail):**")
        lines.append("```text")
        lines.append("".join(hits[-60:])[-4000:])
        lines.append("```")

print("\n".join(lines))
PY

gh pr comment "$PR_NUMBER" --body-file /tmp/smoke-failure-body.md || true
