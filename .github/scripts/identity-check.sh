#!/usr/bin/env bash
# Prints the build, device and asset identity that the app wrote into a saved renderer report and
# compares the APK digest the app reports for itself with the APK file Gradle built and installed.
#
# The comparison is printed, not enforced: the job log is the evidence that the digest shown in the
# app and in its saved reports is the digest of the installed file.
#
# Usage: identity-check.sh <renderer_telemetry.json or renderer_diagnostics.json> [apk file]
set -uo pipefail

REPORT="${1:-}"
APK="${2:-}"
if [[ -z "$APK" ]]; then
  APK="$(ls app/build/outputs/apk/debug/*.apk 2>/dev/null | head -n 1 || true)"
fi

echo "::group::Build, device and asset identity reported by the app"
if [[ -z "$REPORT" || ! -s "$REPORT" ]]; then
  echo "IDENTITY_CHECK not possible: report '${REPORT}' is missing or empty"
  echo "::endgroup::"
  exit 0
fi
python3 - "$REPORT" <<'PY' || echo "IDENTITY_BLOCK could not be read from the report"
import json, sys
report = json.load(open(sys.argv[1]))
print("IDENTITY_BLOCK " + json.dumps(report.get("build"), sort_keys=True))
print("IDENTITY_ASSET asset=%s assetBytes=%s assetSha256=%s" % (
    report.get("asset"), report.get("assetBytes"), report.get("assetSha256")))
PY
APP_SHA="$(python3 -c 'import json,sys; print((json.load(open(sys.argv[1])).get("build") or {}).get("apkSha256") or "")' "$REPORT" 2>/dev/null || true)"
if [[ -n "$APK" && -s "$APK" ]]; then
  FILE_SHA="$(sha256sum "$APK" | cut -d' ' -f1)"
  MATCH=false
  if [[ -n "$APP_SHA" && "$APP_SHA" == "$FILE_SHA" ]]; then MATCH=true; fi
  echo "IDENTITY_CHECK apk_file=$APK apk_file_bytes=$(stat -c%s "$APK") apk_file_sha256=$FILE_SHA apk_sha256_reported_by_app=${APP_SHA:-missing} match=$MATCH"
else
  echo "IDENTITY_CHECK not possible: no APK file to compare with (apk_sha256_reported_by_app=${APP_SHA:-missing})"
fi
echo "::endgroup::"
exit 0
