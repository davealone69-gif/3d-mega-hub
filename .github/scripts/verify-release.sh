#!/usr/bin/env bash
# Verifies a signed release APK and AAB against the Gradle configuration and records the evidence.
# A successful Gradle build is not treated as verification: every claim below is read back from
# the built artifacts themselves, and any mismatch fails the job.
#
#   verify-release.sh verify  <apk> <aab> <output-dir>
#   verify-release.sh compare <apk> <aab> <output-dir>   (second clean build vs the verified one)
set -euo pipefail

MODE="${1:?mode (verify|compare) is required}"
APK="${2:?APK path is required}"
AAB="${3:?AAB path is required}"
OUT="${4:?output directory is required}"
mkdir -p "$OUT"
test -s "$APK"
test -s "$AAB"

BUILD_TOOL() {
  find "$ANDROID_HOME/build-tools" -type f -name "$1" -perm -u+x -print | sort -V | tail -n 1
}

if [[ "$MODE" == "compare" ]]; then
  # Bit-for-bit comparison of a second clean build made with the same key in the same job.
  REFERENCE_APK="$OUT/3D-Mega-Hub-preview-release.apk"
  REFERENCE_AAB="$OUT/3D-Mega-Hub-preview-release.aab"
  test -s "$REFERENCE_APK"
  test -s "$REFERENCE_AAB"
  {
    echo "REPRODUCIBILITY CHECK (two clean builds, same commit, same runner, same one-run key)"
    for kind in apk aab; do
      if [[ "$kind" == "apk" ]]; then first="$REFERENCE_APK"; second="$APK"; else first="$REFERENCE_AAB"; second="$AAB"; fi
      first_sha="$(sha256sum "$first" | cut -d' ' -f1)"
      second_sha="$(sha256sum "$second" | cut -d' ' -f1)"
      echo "$kind build 1 sha256=$first_sha bytes=$(stat -c%s "$first")"
      echo "$kind build 2 sha256=$second_sha bytes=$(stat -c%s "$second")"
      if [[ "$first_sha" == "$second_sha" ]]; then
        echo "$kind REPRODUCIBLE=yes (byte-identical)"
      else
        echo "$kind REPRODUCIBLE=no (bytes differ). Entries whose CRC or size differ:"
        diff <(unzip -v "$first" | awk 'NR>3 {print $NF, $(NF-1), $1}' | sort) \
             <(unzip -v "$second" | awk 'NR>3 {print $NF, $(NF-1), $1}' | sort) | head -n 40 || true
      fi
    done
  } | tee "$OUT/REPRODUCIBILITY.txt"
  exit 0
fi

[[ "$MODE" == "verify" ]] || { echo "unknown mode: $MODE"; exit 2; }

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
fail() { echo "VERIFY-FAIL: $*"; FAILURES=$((FAILURES + 1)); }

# ---------------------------------------------------------------- APK
AAPT2="$(BUILD_TOOL aapt2)"
APKSIGNER="$(BUILD_TOOL apksigner)"
test -x "$AAPT2"
test -x "$APKSIGNER"

echo "::group::APK manifest (aapt2 dump badging)"
"$AAPT2" dump badging "$APK" > "$OUT/apk-badging-full.txt"
grep -E "^(package:|sdkVersion:|targetSdkVersion:|native-code:|application-label:|uses-permission:|uses-feature)" \
  "$OUT/apk-badging-full.txt" | tee "$OUT/apk-badging.txt"
echo "::endgroup::"
PACKAGE_LINE="$(grep -E '^package:' "$OUT/apk-badging-full.txt")"
[[ "$PACKAGE_LINE" == *"name='$EXPECTED_ID'"* ]] || fail "APK applicationId is not $EXPECTED_ID: $PACKAGE_LINE"
[[ "$PACKAGE_LINE" == *"versionCode='$EXPECTED_CODE'"* ]] || fail "APK versionCode is not $EXPECTED_CODE: $PACKAGE_LINE"
[[ "$PACKAGE_LINE" == *"versionName='$EXPECTED_NAME'"* ]] || fail "APK versionName is not $EXPECTED_NAME: $PACKAGE_LINE"
grep -qx "sdkVersion:'$EXPECTED_MIN'" "$OUT/apk-badging-full.txt" || fail "APK minSdk is not $EXPECTED_MIN"
grep -qx "targetSdkVersion:'$EXPECTED_TARGET'" "$OUT/apk-badging-full.txt" || fail "APK targetSdk is not $EXPECTED_TARGET"
if grep -q "application-debuggable" "$OUT/apk-badging-full.txt"; then fail "release APK is debuggable"; fi

echo "::group::APK native libraries per ABI"
unzip -Z1 "$APK" | grep -E '^lib/[^/]+/[^/]+\.so$' | sort | tee "$OUT/apk-native-libs.txt"
echo "::endgroup::"
grep -q '^lib/arm64-v8a/' "$OUT/apk-native-libs.txt" || fail "APK has no arm64-v8a native libraries"
REFERENCE_SET="$(grep '^lib/arm64-v8a/' "$OUT/apk-native-libs.txt" | sed -E 's#^lib/[^/]+/##' | sort | tr '\n' ' ')"
for abi in $(sed -E 's#^lib/([^/]+)/.*#\1#' "$OUT/apk-native-libs.txt" | sort -u); do
  ABI_SET="$(grep "^lib/$abi/" "$OUT/apk-native-libs.txt" | sed -E 's#^lib/[^/]+/##' | sort | tr '\n' ' ')"
  echo "ABI $abi: $ABI_SET"
  # Every packaged ABI must carry the same libraries; an ABI with only some of them installs and
  # then fails in System.loadLibrary at runtime.
  [[ "$ABI_SET" == "$REFERENCE_SET" ]] || fail "ABI $abi does not carry the same native libraries as arm64-v8a"
done
for required in libmega_hub_avatarforge.so libafcore.so libafrender.so libfilament-jni.so libgltfio-jni.so; do
  grep -q "^lib/arm64-v8a/$required$" "$OUT/apk-native-libs.txt" || fail "arm64-v8a is missing $required"
done

echo "::group::APK signature (apksigner verify --verbose --print-certs)"
"$APKSIGNER" verify --verbose --print-certs "$APK" | tee "$OUT/apk-signature.txt"
echo "::endgroup::"
grep -q "Verified using v2 scheme (APK Signature Scheme v2): true" "$OUT/apk-signature.txt" || fail "APK is not v2-signed"

# ---------------------------------------------------------------- AAB
echo "::group::AAB signature (jarsigner -verify)"
jarsigner -verify -verbose -certs "$AAB" > "$OUT/aab-signature.txt" 2>&1 || fail "jarsigner could not verify the AAB"
{ grep -E "jar verified|jar is unsigned|CN=|Signed by|warning|Warning" "$OUT/aab-signature.txt" | sort -u | head -n 20; } || true
echo "::endgroup::"
grep -q "jar verified" "$OUT/aab-signature.txt" || fail "AAB signature did not verify"

echo "::group::AAB native libraries per ABI"
unzip -Z1 "$AAB" | grep -E '^base/lib/[^/]+/[^/]+\.so$' | sort | tee "$OUT/aab-native-libs.txt"
echo "::endgroup::"
grep -q '^base/lib/arm64-v8a/libmega_hub_avatarforge.so$' "$OUT/aab-native-libs.txt" || fail "AAB has no arm64-v8a AvatarForge library"

# bundletool is pinned by version AND checksum; it only reads the bundle's manifest here.
BUNDLETOOL_VERSION="1.18.3"
BUNDLETOOL_SHA256="a099cfa1543f55593bc2ed16a70a7c67fe54b1747bb7301f37fdfd6d91028e29"
BUNDLETOOL_JAR="${RUNNER_TEMP:-/tmp}/bundletool-all-${BUNDLETOOL_VERSION}.jar"
if [[ ! -s "$BUNDLETOOL_JAR" ]]; then
  curl --fail --silent --show-error --location --retry 3 -o "$BUNDLETOOL_JAR" \
    "https://github.com/google/bundletool/releases/download/${BUNDLETOOL_VERSION}/bundletool-all-${BUNDLETOOL_VERSION}.jar"
fi
echo "${BUNDLETOOL_SHA256}  ${BUNDLETOOL_JAR}" | sha256sum --check --status || { echo "bundletool checksum mismatch"; exit 1; }
aab_attr() { java -jar "$BUNDLETOOL_JAR" dump manifest --bundle="$AAB" --xpath "$1" | tr -d '\r\n'; }
AAB_ID="$(aab_attr '/manifest/@package')"
AAB_CODE="$(aab_attr '/manifest/@android:versionCode')"
AAB_NAME="$(aab_attr '/manifest/@android:versionName')"
AAB_MIN="$(aab_attr '/manifest/uses-sdk/@android:minSdkVersion')"
AAB_TARGET="$(aab_attr '/manifest/uses-sdk/@android:targetSdkVersion')"
echo "AAB manifest (bundletool $BUNDLETOOL_VERSION): package=$AAB_ID versionCode=$AAB_CODE versionName=$AAB_NAME minSdk=$AAB_MIN targetSdk=$AAB_TARGET" \
  | tee "$OUT/aab-manifest.txt"
[[ "$AAB_ID" == "$EXPECTED_ID" ]] || fail "AAB package is $AAB_ID"
[[ "$AAB_CODE" == "$EXPECTED_CODE" ]] || fail "AAB versionCode is $AAB_CODE"
[[ "$AAB_NAME" == "$EXPECTED_NAME" ]] || fail "AAB versionName is $AAB_NAME"
[[ "$AAB_MIN" == "$EXPECTED_MIN" ]] || fail "AAB minSdk is $AAB_MIN"
[[ "$AAB_TARGET" == "$EXPECTED_TARGET" ]] || fail "AAB targetSdk is $AAB_TARGET"

# ---------------------------------------------------------------- hashes and summary
echo "::group::SHA-256"
( cd "$(dirname "$APK")" && sha256sum "$(basename "$APK")" ) | tee "$OUT/SHA256SUMS"
( cd "$(dirname "$AAB")" && sha256sum "$(basename "$AAB")" ) | tee -a "$OUT/SHA256SUMS"
echo "::endgroup::"
CERT_SHA256="$(grep -m1 'certificate SHA-256 digest' "$OUT/apk-signature.txt" | sed -E 's/.*: //')"
{
  echo "RELEASE VERIFICATION SUMMARY"
  echo "commit=${GITHUB_SHA:-unknown}"
  echo "apk_bytes=$(stat -c%s "$APK") apk_sha256=$(sha256sum "$APK" | cut -d' ' -f1)"
  echo "aab_bytes=$(stat -c%s "$AAB") aab_sha256=$(sha256sum "$AAB" | cut -d' ' -f1)"
  echo "applicationId=$EXPECTED_ID versionCode=$EXPECTED_CODE versionName=$EXPECTED_NAME minSdk=$EXPECTED_MIN targetSdk=$EXPECTED_TARGET"
  echo "apk_abis=$(sed -E 's#^lib/([^/]+)/.*#\1#' "$OUT/apk-native-libs.txt" | sort -u | tr '\n' ' ')"
  echo "aab_abis=$(sed -E 's#^base/lib/([^/]+)/.*#\1#' "$OUT/aab-native-libs.txt" | sort -u | tr '\n' ' ')"
  echo "signer_certificate_sha256=$CERT_SHA256"
  echo "signing_identity=${SIGNING_IDENTITY_LABEL:-unspecified}"
  echo "verification_failures=$FAILURES"
} | tee "$OUT/VERIFICATION-SUMMARY.txt"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  { echo '```'; cat "$OUT/VERIFICATION-SUMMARY.txt"; echo '```'; } >> "$GITHUB_STEP_SUMMARY"
fi
if [[ "$FAILURES" -ne 0 ]]; then
  echo "Release verification FAILED with $FAILURES problem(s)."
  exit 1
fi
echo "Release verification passed: manifest, ABI set, signatures and hashes were read back from the artifacts."
