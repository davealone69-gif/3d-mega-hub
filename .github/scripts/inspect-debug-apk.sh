#!/usr/bin/env bash
# Inspects the debug APK that is published for the phone test and records the evidence.
# Every value is read back from the APK file itself, never taken from Gradle output. The job fails
# when the file is not the app it claims to be: wrong application ID, version or SDK level, a
# missing ABI or library, a project library that links Vulkan, or a signature that does not verify.
# A phone test of the wrong file proves nothing.
#
#   inspect-debug-apk.sh <apk> <output-dir>
set -euo pipefail

APK="${1:?APK path is required}"
OUT="${2:?output directory is required}"
mkdir -p "$OUT"
test -s "$APK"

BUILD_TOOL() {
  find "$ANDROID_HOME/build-tools" -type f -name "$1" -perm -u+x -print | sort -V | tail -n 1
}

GRADLE_FILE="app/build.gradle.kts"
expected() { grep -E "^\s*$1\s*=" "$GRADLE_FILE" | head -n 1 | sed -E 's/.*=\s*"?([^"]*)"?\s*$/\1/'; }
EXPECTED_ID="$(expected applicationId)"
EXPECTED_CODE="$(expected versionCode)"
EXPECTED_NAME="$(expected versionName)"
EXPECTED_MIN="$(expected minSdk)"
EXPECTED_TARGET="$(expected targetSdk)"
echo "Expected from $GRADLE_FILE: applicationId=$EXPECTED_ID versionCode=$EXPECTED_CODE versionName=$EXPECTED_NAME minSdk=$EXPECTED_MIN targetSdk=$EXPECTED_TARGET"
for value in "$EXPECTED_ID" "$EXPECTED_CODE" "$EXPECTED_NAME" "$EXPECTED_MIN" "$EXPECTED_TARGET"; do
  test -n "$value"
done

FAILURES=0
fail() { echo "INSPECT-FAIL: $*"; FAILURES=$((FAILURES + 1)); }

AAPT2="$(BUILD_TOOL aapt2)"
APKSIGNER="$(BUILD_TOOL apksigner)"
test -x "$AAPT2"
test -x "$APKSIGNER"

APK_BYTES="$(stat -c%s "$APK")"
APK_SHA256="$(sha256sum "$APK" | cut -d' ' -f1)"

echo "::group::Debug APK manifest (aapt2 dump badging)"
"$AAPT2" dump badging "$APK" > "$OUT/apk-badging-full.txt"
{ grep -E "^(package:|sdkVersion:|minSdkVersion:|targetSdkVersion:|native-code:|application-label:|application-debuggable|uses-permission:|uses-feature)" \
  "$OUT/apk-badging-full.txt" || true; } | tee "$OUT/apk-badging.txt"
echo "::endgroup::"
PACKAGE_LINE="$(grep -E '^package:' "$OUT/apk-badging-full.txt" || true)"
badging_attr() { sed -nE "s/.* $1='([^']*)'.*/\\1/p" <<<"$PACKAGE_LINE" | head -n 1; }
APK_ID="$(badging_attr name)"
APK_CODE="$(badging_attr versionCode)"
APK_NAME="$(badging_attr versionName)"
# aapt2 prints the minimum SDK as "sdkVersion:" in older build-tools and "minSdkVersion:" in newer
# ones. If neither line is present, read the attribute from the binary manifest directly.
APK_MIN="$(sed -nE "s/^(sdkVersion|minSdkVersion):'([^']*)'.*/\\2/p" "$OUT/apk-badging-full.txt" | head -n 1)"
if [[ -z "$APK_MIN" ]]; then
  APK_MIN="$({ "$AAPT2" dump xmltree "$APK" --file AndroidManifest.xml || true; } \
    | sed -nE 's/.*minSdkVersion\(0x0101020c\)=([0-9]+).*/\1/p' | head -n 1)"
fi
APK_TARGET="$(sed -nE "s/^targetSdkVersion:'([^']*)'.*/\\1/p" "$OUT/apk-badging-full.txt" | head -n 1)"
if grep -q "application-debuggable" "$OUT/apk-badging-full.txt"; then APK_DEBUGGABLE=true; else APK_DEBUGGABLE=false; fi
[[ "$APK_ID" == "$EXPECTED_ID" ]] || fail "applicationId is '${APK_ID:-not found}', expected $EXPECTED_ID"
[[ "$APK_CODE" == "$EXPECTED_CODE" ]] || fail "versionCode is '${APK_CODE:-not found}', expected $EXPECTED_CODE"
[[ "$APK_NAME" == "$EXPECTED_NAME" ]] || fail "versionName is '${APK_NAME:-not found}', expected $EXPECTED_NAME"
[[ "$APK_MIN" == "$EXPECTED_MIN" ]] || fail "minSdk is '${APK_MIN:-not found}', expected $EXPECTED_MIN"
[[ "$APK_TARGET" == "$EXPECTED_TARGET" ]] || fail "targetSdk is '${APK_TARGET:-not found}', expected $EXPECTED_TARGET"
# This script is for the debug build that carries the on-screen diagnostics and the PixelCopy check.
[[ "$APK_DEBUGGABLE" == "true" ]] || fail "the APK is not debuggable, so it is not the diagnostic debug build"

echo "::group::Debug APK native libraries per ABI"
{ unzip -Z1 "$APK" | { grep -E '^lib/[^/]+/[^/]+\.so$' || true; } | sort; } | tee "$OUT/apk-native-libs.txt"
echo "::endgroup::"
APK_ABIS="$(sed -E 's#^lib/([^/]+)/.*#\1#' "$OUT/apk-native-libs.txt" | sort -u | tr '\n' ' ' | sed -E 's/ +$//')"
# The Samsung SM-S921B runs arm64-v8a; the emulator gates run x86_64. Both must be in the one file.
grep -q '^lib/arm64-v8a/' "$OUT/apk-native-libs.txt" || fail "the APK has no arm64-v8a native libraries"
grep -q '^lib/x86_64/' "$OUT/apk-native-libs.txt" || fail "the APK has no x86_64 native libraries"
REFERENCE_SET="$({ grep '^lib/arm64-v8a/' "$OUT/apk-native-libs.txt" || true; } | sed -E 's#^lib/[^/]+/##' | sort | tr '\n' ' ')"
for abi in $APK_ABIS; do
  ABI_SET="$({ grep "^lib/$abi/" "$OUT/apk-native-libs.txt" || true; } | sed -E 's#^lib/[^/]+/##' | sort | tr '\n' ' ')"
  echo "ABI $abi: $ABI_SET"
  [[ "$ABI_SET" == "$REFERENCE_SET" ]] || fail "ABI $abi does not carry the same native libraries as arm64-v8a"
done
for required in libmega_hub_avatarforge.so libafcore.so libafrender.so libfilament-jni.so libgltfio-jni.so; do
  grep -q "^lib/arm64-v8a/$required$" "$OUT/apk-native-libs.txt" || fail "arm64-v8a is missing $required"
done

# The app renders with OpenGL ES only: the project's libraries must not link the Vulkan loader and
# the renderer library must link OpenGL ES. Same rule as the signed-release gate.
echo "::group::Debug APK native library dependencies (NEEDED entries read back with readelf)"
READELF="$(command -v readelf || true)"
if [[ -z "$READELF" ]]; then
  READELF="$(ls "${ANDROID_HOME:-/nonexistent}"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf 2>/dev/null | sort | tail -n 1 || true)"
fi
if [[ -z "$READELF" ]]; then
  fail "no readelf is available, so the native library dependencies could not be read back"
else
  LIBS_DIR="$OUT/apk-libs"
  rm -rf "$LIBS_DIR"
  mkdir -p "$LIBS_DIR"
  unzip -q -o "$APK" 'lib/*' -d "$LIBS_DIR" || fail "could not extract the native libraries from the APK"
  : > "$OUT/apk-native-deps.txt"
  while IFS= read -r library; do
    NEEDED="$( { "$READELF" -d "$LIBS_DIR/$library" 2>/dev/null || true; } | { grep 'NEEDED' || true; } | sed -E 's/.*\[(.*)\].*/\1/' | sort | tr '\n' ' ')"
    echo "$library: $NEEDED" | tee -a "$OUT/apk-native-deps.txt"
  done < "$OUT/apk-native-libs.txt"
  rm -rf "$LIBS_DIR"
  for own in libafrender.so libafcore.so libmega_hub_avatarforge.so; do
    if { grep -E "^lib/[^/]+/$own: " "$OUT/apk-native-deps.txt" || true; } | grep -q 'libvulkan'; then
      fail "$own links libvulkan.so; the app must render with OpenGL ES only"
    fi
  done
  for abi in $APK_ABIS; do
    { grep -E "^lib/$abi/libafrender.so: " "$OUT/apk-native-deps.txt" || true; } | grep -q 'libGLESv3.so' \
      || fail "$abi libafrender.so does not link libGLESv3.so"
  done
fi
echo "::endgroup::"

echo "::group::Debug APK signature (apksigner verify --verbose --print-certs)"
if "$APKSIGNER" verify --verbose --print-certs "$APK" > "$OUT/apk-signature.txt" 2>&1; then
  SIGNATURE_VERIFIES=true
else
  SIGNATURE_VERIFIES=false
  fail "apksigner could not verify the APK signature"
fi
cat "$OUT/apk-signature.txt"
echo "::endgroup::"
# apksigner labels the signer "Signer #1" in older build-tools and "V2 Signer:" in newer ones, so
# the label is not matched. The first certificate line belongs to the scheme Android verifies.
CERT_SHA256="$(sed -nE 's/^.*certificate SHA-256 digest: ([0-9a-fA-F]{64}).*$/\1/p' "$OUT/apk-signature.txt" | head -n 1)"
CERT_DN="$(sed -nE 's/^.*certificate DN: (.*)$/\1/p' "$OUT/apk-signature.txt" | head -n 1)"
SIGNER_COUNT="$(sed -nE 's/^Number of signers: ([0-9]+).*$/\1/p' "$OUT/apk-signature.txt" | head -n 1)"
[[ "${SIGNER_COUNT:-1}" == "1" ]] || fail "the APK has ${SIGNER_COUNT} signers; exactly one is expected"
SCHEMES="$({ grep -E '^Verified using v[0-9.]+ scheme .*: true' "$OUT/apk-signature.txt" || true; } | sed -E 's/^Verified using (v[0-9.]+) scheme.*/\1/' | tr '\n' ' ' | sed -E 's/ +$//')"
[[ -n "$CERT_SHA256" ]] || fail "the signing certificate SHA-256 could not be read"

{
  echo "file=$(basename "$APK")"
  echo "bytes=$APK_BYTES"
  echo "sha256=$APK_SHA256"
  echo "source_commit=${GITHUB_SHA:-unknown}"
  echo "workflow_run=${GITHUB_RUN_ID:-unknown}"
  echo "applicationId=${APK_ID:-?}"
  echo "versionCode=${APK_CODE:-?}"
  echo "versionName=${APK_NAME:-?}"
  echo "minSdk=${APK_MIN:-?}"
  echo "targetSdk=${APK_TARGET:-?}"
  echo "debuggable=$APK_DEBUGGABLE"
  echo "abis=$APK_ABIS"
  echo "signature_verifies=$SIGNATURE_VERIFIES"
  echo "signature_schemes=${SCHEMES:-none}"
  echo "certificate_sha256=${CERT_SHA256:-?}"
  echo "certificate_dn=${CERT_DN:-?}"
  echo "certificate_note=debug key created on this CI runner for this job; it identifies this one file, not a publisher"
  echo "inspection_failures=$FAILURES"
} | tee "$OUT/APK_INSPECTION.txt"

echo "APK_INSPECTION sha256=$APK_SHA256 bytes=$APK_BYTES applicationId=${APK_ID:-?} versionCode=${APK_CODE:-?} versionName=${APK_NAME:-?} minSdk=${APK_MIN:-?} targetSdk=${APK_TARGET:-?} debuggable=$APK_DEBUGGABLE abis=[$APK_ABIS] schemes=[${SCHEMES:-none}] certificateSha256=${CERT_SHA256:-?} failures=$FAILURES"

if [[ "$FAILURES" -ne 0 ]]; then
  echo "Debug APK inspection FAILED with $FAILURES problem(s)."
  exit 1
fi
echo "Debug APK inspection passed: the file is the debug build of $APK_ID $APK_NAME for [$APK_ABIS] with a signature that verifies."
