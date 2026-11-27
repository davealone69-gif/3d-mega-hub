#!/usr/bin/env bash
# Posts decisive runtime-gate failure extracts to the pull request so reviewers can diagnose
# failures without downloading Azure-hosted job logs or artifacts. Runs only on failure.
set -uo pipefail
OUT="artifacts/avatarforge/runtime-captures"
[ -n "${PR_NUMBER:-}" ] || { echo "not a pull-request event; nothing to comment"; exit 0; }
[ -n "${GH_EVIDENCE_TOKEN:-}" ] || { echo "no token; nothing to comment"; exit 0; }
{
  echo "Runtime gate evidence for ${GITHUB_SHA:0:8} (run ${GITHUB_RUN_ID}): instrumentation test failed; decisive extracts below."
  echo '```'
  echo "--- evidence manifest ---"
  head -n 25 "$OUT/evidence-manifest.txt" 2>/dev/null || echo "missing"
  echo "--- renderer verdict / telemetry lines ---"
  grep -E "TELEMETRY |EVIDENCE|PixelCopy attempt|FRAME_PRESENTED|Renderer failed|REAL \+ VERIFIED|FAILED:|SLOW_START|STALL " "$OUT/avatarforge-logcat.txt" 2>/dev/null | tail -n 25 || echo "none"
  echo "--- crashes / ANRs ---"
  grep -E "FATAL EXCEPTION|Fatal signal|ANR in" "$OUT/logcat.txt" 2>/dev/null | tail -n 8 || echo "none"
  echo "--- JUnit failure messages ---"
  python3 - <<'EOF'
import glob, xml.etree.ElementTree as ET
found = False
for x in glob.glob("app/build/outputs/androidTest-results/connected/**/*.xml", recursive=True):
    try:
        r = ET.parse(x).getroot()
    except Exception:
        continue
    for tc in r.iter("testcase"):
        for f in list(tc.iter("failure")) + list(tc.iter("error")):
            found = True
            print(f"TEST {tc.get('classname')}.{tc.get('name')}")
            print((f.get("message") or "")[:1200])
            print((f.text or "")[:1200])
if not found:
    print("no JUnit failure node found (crash before reporting?)")
EOF
  echo '```'
} > /tmp/evidence-body.md
python3 - <<'EOF'
import json
body = open('/tmp/evidence-body.md').read()[:30000]
json.dump({"body": body}, open('/tmp/evidence-payload.json', 'w'))
EOF
curl -s -o /tmp/evidence-post.out -w "comment http %{http_code}\n" -X POST \
  -H "Authorization: Bearer $GH_EVIDENCE_TOKEN" \
  -H "Accept: application/vnd.github+json" \
  "https://api.github.com/repos/$GITHUB_REPOSITORY/issues/$PR_NUMBER/comments" \
  --data @/tmp/evidence-payload.json
head -c 300 /tmp/evidence-post.out || true
