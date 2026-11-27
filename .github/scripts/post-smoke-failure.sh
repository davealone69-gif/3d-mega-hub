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
import glob, os, xml.etree.ElementTree as ET

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
                if fail is not None:
                    msg = (fail.text or fail.attrib.get("message") or "").strip()
                    lines.append(f"- `[FAIL]` `{name}` ({t}s)")
                    lines.append("```text")
                    lines.append(msg[:2500])
                    lines.append("```")
                else:
                    lines.append(f"- `[PASS]` `{name}` ({t}s)")
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
        lines.append(f.read()[:2000])
    lines.append("```")

logcat = "avatarforge-render-smoke-evidence/avatarforge-logcat.txt"
if os.path.isfile(logcat):
    with open(logcat, "r", errors="replace") as f:
        tail = f.readlines()[-40:]
    if tail:
        lines.append("")
        lines.append("**`avatarforge-logcat.txt` (last 40 lines):**")
        lines.append("```text")
        lines.append("".join(tail)[:2500])
        lines.append("```")

gradle_out = "avatarforge-render-smoke-evidence/gradle-output.txt"
if os.path.isfile(gradle_out):
    with open(gradle_out, "r", errors="replace") as f:
        tail = f.readlines()[-45:]
    if tail:
        lines.append("")
        lines.append("**`gradle-output.txt` (last 45 lines):**")
        lines.append("```text")
        lines.append("".join(tail)[:3000])
        lines.append("```")

print("\n".join(lines))
PY

gh pr comment "$PR_NUMBER" --body-file /tmp/smoke-failure-body.md || true
