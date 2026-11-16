#!/usr/bin/env bash
# Verifies the debug APK the way its user receives it: from the downloaded artifact, on a machine
# other than the one that built it. The APK is inspected again (application ID, version, ABIs,
# library dependencies, signature, certificate) and then tied to the rest of the evidence:
#
#   downloaded file  ==  SHA256SUMS in the artifact
#                    ==  the file the build job inspected (APK_INSPECTION.txt)
#                    ==  the APK the app reported for itself while the production render test ran
#                        on the emulator (renderer_diagnostics.json, build.apkSha256)
#
# and the build job's record must name this commit and this workflow run. Any broken link fails.
#
#   verify-downloaded-apk.sh <downloaded-artifact-dir> <output-dir>
set -euo pipefail

DIR="${1:?downloaded artifact directory is required}"
OUT="${2:?output directory is required}"
mkdir -p "$OUT"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FAILURES=0
fail() { echo "DOWNLOAD-VERIFY-FAIL: $*"; FAILURES=$((FAILURES + 1)); }

echo "::group::Files in the downloaded artifact"
(cd "$DIR" && find . -type f | sort | head -n 200)
echo "::endgroup::"

APK="$(find "$DIR" -type f -name '3D-Mega-Hub-generated-avatar-diagnostic-debug.apk' | head -n 1)"
SUMS="$(find "$DIR" -type f -name 'SHA256SUMS' | head -n 1)"
RECORD="$(find "$DIR" -type f -name 'APK_INSPECTION.txt' | head -n 1)"
DIAGNOSTICS="$(find "$DIR" -type f -path '*runtime-captures/renderer_diagnostics.json' | head -n 1)"
if [[ -z "$DIAGNOSTICS" ]]; then
  DIAGNOSTICS="$(find "$DIR" -type f -name 'renderer_diagnostics.json' | head -n 1)"
fi

if [[ -z "$APK" || ! -s "$APK" ]]; then
  echo "DOWNLOAD-VERIFY-FAIL: the artifact does not contain the debug APK"
  exit 1
fi

# 1. The downloaded file is the app it claims to be. This repeats the build job's inspection on
#    the bytes that came out of the artifact store.
INSPECTION_OK=true
if ! bash "$HERE/inspect-debug-apk.sh" "$APK" "$OUT/inspection"; then
  INSPECTION_OK=false
  fail "the downloaded APK did not pass inspection"
fi

DOWNLOADED_SHA="$(sha256sum "$APK" | cut -d' ' -f1)"
DOWNLOADED_BYTES="$(stat -c%s "$APK")"
DOWNLOADED_CERT="$(sed -n 's/^certificate_sha256=//p' "$OUT/inspection/APK_INSPECTION.txt" 2>/dev/null | head -n 1 || true)"

# 2. The checksum file that travels with the APK.
SUMS_SHA=""
if [[ -n "$SUMS" ]]; then SUMS_SHA="$(awk 'NR==1 {print $1}' "$SUMS")"; fi
[[ -n "$SUMS_SHA" ]] || fail "SHA256SUMS is missing from the artifact"
[[ -z "$SUMS_SHA" || "$SUMS_SHA" == "$DOWNLOADED_SHA" ]] || fail "SHA256SUMS says $SUMS_SHA, the downloaded APK is $DOWNLOADED_SHA"

# 3. The record the build job wrote before the emulator tests ran.
BUILT_SHA=""; BUILT_CERT=""; BUILT_COMMIT=""; BUILT_RUN=""
if [[ -n "$RECORD" ]]; then
  BUILT_SHA="$(sed -n 's/^sha256=//p' "$RECORD" | head -n 1)"
  BUILT_CERT="$(sed -n 's/^certificate_sha256=//p' "$RECORD" | head -n 1)"
  BUILT_COMMIT="$(sed -n 's/^source_commit=//p' "$RECORD" | head -n 1)"
  BUILT_RUN="$(sed -n 's/^workflow_run=//p' "$RECORD" | head -n 1)"
fi
[[ -n "$BUILT_SHA" ]] || fail "the build job's inspection record is missing from the artifact"
[[ -z "$BUILT_SHA" || "$BUILT_SHA" == "$DOWNLOADED_SHA" ]] || fail "the build job inspected $BUILT_SHA, the downloaded APK is $DOWNLOADED_SHA"
[[ -z "$BUILT_CERT" || "$BUILT_CERT" == "$DOWNLOADED_CERT" ]] || fail "the build job recorded certificate $BUILT_CERT, the downloaded APK is signed with ${DOWNLOADED_CERT:-nothing readable}"
if [[ -n "${GITHUB_SHA:-}" ]]; then
  [[ "$BUILT_COMMIT" == "$GITHUB_SHA" ]] || fail "the build record names commit '${BUILT_COMMIT:-none}', this run is for $GITHUB_SHA"
fi
if [[ -n "${GITHUB_RUN_ID:-}" ]]; then
  [[ "$BUILT_RUN" == "$GITHUB_RUN_ID" ]] || fail "the build record names workflow run '${BUILT_RUN:-none}', this is run $GITHUB_RUN_ID"
fi

# 4. The report the app saved while the production render test ran on the emulator.
TESTED_SHA=""; TESTED_STATUS=""; TESTED_PIXELS=""; TESTED_MINIMUM=""; TESTED_ASSET_SHA=""; TESTED_DEVICE=""
if [[ -n "$DIAGNOSTICS" ]]; then
  REPORT_FIELDS="$(python3 - "$DIAGNOSTICS" <<'PY' || true
import json, sys
try:
    with open(sys.argv[1], encoding="utf-8") as handle:
        report = json.load(handle)
except Exception as error:  # an unreadable report is reported below as a missing link
    print("parse_error=%s" % str(error).replace("\n", " "))
    sys.exit(0)
build = report.get("build") or {}
print("apk=%s" % build.get("apkSha256", ""))
print("status=%s" % report.get("status", ""))
print("pixels=%s" % report.get("visiblePixels", ""))
print("minimum=%s" % report.get("minimumVisiblePixels", ""))
print("asset=%s" % report.get("assetSha256", ""))
print("device=%s %s, Android %s (API %s), %s" % (
    build.get("deviceManufacturer", "?"), build.get("deviceModel", "?"),
    build.get("androidRelease", "?"), build.get("androidSdk", "?"), build.get("abi", "?")))
PY
)"
  field() { sed -n "s/^$1=//p" <<<"$REPORT_FIELDS" | head -n 1; }
  TESTED_SHA="$(field apk)"
  TESTED_STATUS="$(field status)"
  TESTED_PIXELS="$(field pixels)"
  TESTED_MINIMUM="$(field minimum)"
  TESTED_ASSET_SHA="$(field asset)"
  TESTED_DEVICE="$(field device)"
  PARSE_ERROR="$(field parse_error)"
  [[ -z "$PARSE_ERROR" ]] || fail "renderer_diagnostics.json could not be read: $PARSE_ERROR"
fi
[[ -n "$TESTED_SHA" ]] || fail "the artifact has no renderer report that names the APK the emulator test ran"
[[ -z "$TESTED_SHA" || "$TESTED_SHA" == "$DOWNLOADED_SHA" ]] || fail "the emulator test ran APK $TESTED_SHA, the downloaded APK is $DOWNLOADED_SHA"
[[ "$TESTED_STATUS" == "PIXELS_VERIFIED" ]] || fail "the renderer report's status is '${TESTED_STATUS:-missing}', not PIXELS_VERIFIED"

if [[ "$FAILURES" -eq 0 ]]; then CHAIN=true; else CHAIN=false; fi
{
  echo "downloaded_apk=$(basename "$APK")"
  echo "downloaded_bytes=$DOWNLOADED_BYTES"
  echo "downloaded_sha256=$DOWNLOADED_SHA"
  echo "downloaded_apk_passes_inspection=$INSPECTION_OK"
  echo "sha256sums_sha256=${SUMS_SHA:-missing}"
  echo "build_job_inspected_sha256=${BUILT_SHA:-missing}"
  echo "emulator_tested_apk_sha256=${TESTED_SHA:-missing}"
  echo "certificate_sha256=${DOWNLOADED_CERT:-missing}"
  echo "build_job_certificate_sha256=${BUILT_CERT:-missing}"
  echo "source_commit=${BUILT_COMMIT:-missing}"
  echo "workflow_run=${BUILT_RUN:-missing}"
  echo "render_test_status=${TESTED_STATUS:-missing}"
  echo "render_test_visible_pixels=${TESTED_PIXELS:-missing}"
  echo "render_test_minimum_visible_pixels=${TESTED_MINIMUM:-missing}"
  echo "render_test_asset_sha256=${TESTED_ASSET_SHA:-missing}"
  echo "render_test_device=${TESTED_DEVICE:-missing}"
  echo "chain_complete=$CHAIN"
  echo "failures=$FAILURES"
} | tee "$OUT/DOWNLOAD_VERIFICATION.txt"

echo "DOWNLOAD_VERIFY downloaded_sha256=$DOWNLOADED_SHA sha256sums=${SUMS_SHA:-missing} build_job=${BUILT_SHA:-missing} emulator_test=${TESTED_SHA:-missing} certificate=${DOWNLOADED_CERT:-missing} commit=${BUILT_COMMIT:-missing} run=${BUILT_RUN:-missing} render_test=${TESTED_STATUS:-missing} visiblePixels=${TESTED_PIXELS:-missing} chain_complete=$CHAIN"

if [[ "$FAILURES" -ne 0 ]]; then
  echo "Downloaded APK verification FAILED with $FAILURES problem(s)."
  exit 1
fi
echo "Downloaded APK verification passed: the file in the artifact is the file that was built, inspected and tested by this run of commit $BUILT_COMMIT."
