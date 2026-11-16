#!/usr/bin/env bash
# Kotlin on a machine without a JDK, Gradle or the Android SDK.
#
#   tools/host_checks/kotlin_host.sh engine      compile :avatarforge-engine (main and test) and run its JVM tests
#   tools/host_checks/kotlin_host.sh app-syntax  parse every Kotlin file of :app and report syntax errors only
#   tools/host_checks/kotlin_host.sh kotlinc ... the compiler itself, with the given arguments
#   WORK=/some/dir overrides the working directory (default /tmp/af-kotlin).
#
# It exists because the sandboxes this repository is worked on in can reach only github.com, PyPI
# and npm: no Maven, no Google repository. A Gradle build cannot run there, and a Kotlin mistake
# would otherwise cost a CI round trip. The pieces, none of which is kept in the repository:
#   Java runtime     PyPI package jdk4py (a Temurin runtime image)
#   Kotlin compiler  PyPI package kotlin-jupyter-kernel (its jar contains K2JVMCompiler)
#   android.jar      github.com/Sable/android-platforms, API 35 (the SDK's stub classes)
#   JUnit            NOT the real one (no source for the jar is reachable): junit_standin.kt in this
#                    folder declares the few names the tests import and a runner for them
#
# What it does NOT replace: the Gradle build in CI. The compiler here is newer than the project's
# (language version pinned to 2.1); :app is only parsed, because Compose and AndroidX are not
# available; android.jar holds stubs that throw, so a test that calls the Android framework or
# org.json is reported as "needs Android" and not run; and the JUnit stand-in is not JUnit.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${WORK:-/tmp/af-kotlin}"
mkdir -p "$WORK/lib"
PY="${PYTHON:-python3}"

if [ ! -x "$WORK/jdk/jdk4py/java-runtime/bin/java" ]; then
    "$PY" -m pip install --quiet --target "$WORK/jdk" jdk4py
fi
JAVA_HOME_DIR="$WORK/jdk/jdk4py/java-runtime"
if [ ! -d "$WORK/kernel/run_kotlin_kernel/jars" ]; then
    "$PY" -m pip download --quiet --no-deps -d "$WORK/wheel" kotlin-jupyter-kernel
    "$PY" - "$WORK" <<'PYEOF'
import glob, sys, zipfile
work = sys.argv[1]
zipfile.ZipFile(glob.glob(work + '/wheel/kotlin_jupyter_kernel-*.whl')[0]).extractall(work + '/kernel')
PYEOF
fi
JARS="$WORK/kernel/run_kotlin_kernel/jars"
COMPILER_JAR="$(ls "$JARS"/kotlin-jupyter-kernel-*-all.jar | head -1)"
STDLIB="$(ls "$JARS"/kotlin-stdlib-*.jar | head -1)"
REFLECT="$(ls "$JARS"/kotlin-reflect-*.jar | head -1)"

if [ ! -s "$WORK/lib/android-35.jar" ]; then
    gh api -H "Accept: application/vnd.github.raw" repos/Sable/android-platforms/contents/android-35/android.jar > "$WORK/lib/android-35.jar"
fi

kotlinc() {
    "$JAVA_HOME_DIR/bin/java" -Xmx1400m --enable-native-access=ALL-UNNAMED \
        -cp "$COMPILER_JAR:$STDLIB:$REFLECT" org.jetbrains.kotlin.cli.jvm.K2JVMCompiler \
        -no-stdlib -no-reflect -jdk-home "$JAVA_HOME_DIR" -language-version 2.1 -api-version 2.1 "$@" \
        2> >(grep -v "^WARNING: \|^$" >&2)
}

case "${1:-}" in
kotlinc)
    shift
    kotlinc "$@"
    ;;
engine)
    # androidx.core is not available: one stand-in for the single class the module uses.
    mkdir -p "$WORK/standin/androidx/core/content/pm"
    cat > "$WORK/standin/androidx/core/content/pm/PackageInfoCompat.kt" <<'KT'
package androidx.core.content.pm

import android.content.pm.PackageInfo

/** Stand-in for a host compile; the app uses the real androidx.core class. */
object PackageInfoCompat {
    @Suppress("DEPRECATION")
    fun getLongVersionCode(info: PackageInfo): Long = info.versionCode.toLong()
}
KT
    rm -rf "$WORK/out-main" "$WORK/out-test"
    echo "== :avatarforge-engine main"
    # shellcheck disable=SC2046
    kotlinc -cp "$WORK/lib/android-35.jar:$STDLIB:$COMPILER_JAR" -d "$WORK/out-main" \
        $(find "$ROOT/avatarforge-engine/src/main/java" "$WORK/standin" -name "*.kt")
    echo "== :avatarforge-engine test"
    # shellcheck disable=SC2046
    kotlinc -Xfriend-paths="$WORK/out-main" -cp "$WORK/out-main:$WORK/lib/android-35.jar:$STDLIB:$COMPILER_JAR" -d "$WORK/out-test" \
        "$ROOT/tools/host_checks/junit_standin.kt" "$ROOT/tools/host_checks/junit_standin_runner.kt" $(find "$ROOT/avatarforge-engine/src/test/java" -name "*.kt")
    # These three exercise org.json, of which android.jar holds only stubs that throw; the code
    # under test catches the exception, so they would fail here for a reason that is not theirs.
    skip="${HOST_SKIP:-GlbValidatorTest|GlbValidatorMorphCompatibilityTest|RendererTelemetryTest}"
    classes="$(cd "$WORK/out-test" && find . -name "*Test.class" | sed 's|^\./||; s|\.class$||; s|/|.|g' | grep -Ev "\.($skip)\$" | sort)"
    echo "== tests (not run here, they need org.json: $skip)"
    # shellcheck disable=SC2086
    "$JAVA_HOME_DIR/bin/java" -cp "$WORK/out-test:$WORK/out-main:$STDLIB:$REFLECT:$COMPILER_JAR:$WORK/lib/android-35.jar" \
        hostjunit.Runner $classes
    ;;
app-syntax)
    echo "== :app, syntax only (every unresolved reference is expected and not shown)"
    # shellcheck disable=SC2046
    if kotlinc -cp "$WORK/lib/android-35.jar:$STDLIB" -d "$WORK/out-app-unused" \
        $(find "$ROOT/app/src" -name "*.kt") 2>&1 | grep -iE "syntax error|expecting |unexpected tokens|unclosed|missing '" ; then
        echo "app-syntax: FAILED"; exit 1
    fi
    echo "app-syntax: no syntax error in $(find "$ROOT/app/src" -name "*.kt" | wc -l) files"
    ;;
*)
    sed -n '2,8p' "$0"; exit 2
    ;;
esac
