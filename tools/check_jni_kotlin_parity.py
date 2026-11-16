#!/usr/bin/env python3
"""
Check that every Kotlin `external fun` in AnimationEngine.kt has a matching JNI symbol in
avatarforge_animation_jni.cpp, with the same arity and the same types -- and the other way round.

Why this exists: a JNI mismatch is NOT a link error. The shared library builds, the APK installs, and
the failure appears at the first call as an UnsatisfiedLinkError on a device, which in practice means
in CI at best and in a user's hands at worst. Renaming one Kotlin method or changing one argument from
Int to Long is enough. This script turns that class of bug into a one-second check.

Usage:
    python3 tools/check_jni_kotlin_parity.py [--kotlin PATH] [--cpp PATH]

Exit status is 0 when the two sides agree, 1 when they do not.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_KOTLIN = REPO / "avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/AnimationEngine.kt"
DEFAULT_CPP = REPO / "avatarforge-engine/src/main/cpp/avatarforge_animation_jni.cpp"

# The JNI class prefix the Kotlin package and object name imply. Checked, not assumed: renaming the
# Kotlin object without renaming the C symbols is exactly the breakage this script exists to catch.
EXPECTED_PREFIX = "Java_com_davemega_hub_avatarforge_AnimationNative_"

# Kotlin type -> JNI type, for arguments.
ARG_TYPES = {
    "Long": "jlong",
    "Int": "jint",
    "Float": "jfloat",
    "Boolean": "jboolean",
    "Double": "jdouble",
    "Short": "jshort",
    "Byte": "jbyte",
    "Char": "jchar",
    "ByteArray": "jbyteArray",
    "FloatArray": "jfloatArray",
    "IntArray": "jintArray",
    "DoubleArray": "jdoubleArray",
    "LongArray": "jlongArray",
    "String": "jstring",
}

# Kotlin type -> JNI type, for returns. A nullable String still crosses as jstring.
RET_TYPES = dict(ARG_TYPES)
RET_TYPES.update({
    "Unit": "void",
    "String?": "jstring",
    "Array<String>": "jobjectArray",
    "Any?": "jobject",
})

KOTLIN_DECL = re.compile(r"external fun (\w+)\s*\(([^)]*)\)\s*(:\s*[^\n{]+)?")
JNI_DEF = re.compile(
    r"JNIEXPORT\s+(\w+)\s+JNICALL\s*\n(" + EXPECTED_PREFIX + r"\w+)\s*\(\s*([^)]*)\)"
)


def parse_kotlin(text: str) -> dict[str, tuple[list[str], str]]:
    out: dict[str, tuple[list[str], str]] = {}
    for name, args, ret in KOTLIN_DECL.findall(text):
        parsed = []
        for arg in args.split(","):
            arg = arg.strip()
            if not arg:
                continue
            if ":" not in arg:
                raise SystemExit(f"{name}: argument '{arg}' has no type annotation")
            parsed.append(arg.split(":", 1)[1].strip())
        declared_return = ret.strip()[1:].strip() if ret else "Unit"
        out[name] = (parsed, declared_return)
    return out


def parse_jni(text: str) -> dict[str, tuple[list[str], str]]:
    out: dict[str, tuple[list[str], str]] = {}
    for ret, symbol, args in JNI_DEF.findall(text):
        parts = [p.strip() for p in args.split(",") if p.strip()]
        # Drop the JNIEnv* and the jobject/jclass receiver; what remains are the declared arguments.
        native = parts[2:]
        types = [p.split()[0] for p in native]
        out[symbol[len(EXPECTED_PREFIX):]] = (types, ret)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--kotlin", type=Path, default=DEFAULT_KOTLIN)
    parser.add_argument("--cpp", type=Path, default=DEFAULT_CPP)
    args = parser.parse_args()

    for path in (args.kotlin, args.cpp):
        if not path.is_file():
            print(f"FAIL: {path} does not exist", file=sys.stderr)
            return 1

    kotlin = parse_kotlin(args.kotlin.read_text(encoding="utf-8"))
    jni = parse_jni(args.cpp.read_text(encoding="utf-8"))

    problems: list[str] = []

    missing_impl = sorted(set(kotlin) - set(jni))
    missing_decl = sorted(set(jni) - set(kotlin))
    for name in missing_impl:
        problems.append(f"{name}: declared `external fun` in Kotlin but no {EXPECTED_PREFIX}{name} in C++")
    for name in missing_decl:
        problems.append(f"{name}: implemented in C++ but not declared in Kotlin (dead JNI symbol)")

    for name in sorted(set(kotlin) & set(jni)):
        kargs, kret = kotlin[name]
        jargs, jret = jni[name]

        if kret not in RET_TYPES:
            problems.append(f"{name}: Kotlin return type '{kret}' has no JNI mapping in this checker")
        elif RET_TYPES[kret] != jret:
            problems.append(f"{name}: Kotlin returns {kret} ({RET_TYPES[kret]}) but JNI returns {jret}")

        if len(kargs) != len(jargs):
            problems.append(f"{name}: Kotlin takes {len(kargs)} argument(s), JNI takes {len(jargs)}")
            continue

        for index, (ktype, jtype) in enumerate(zip(kargs, jargs)):
            if ktype not in ARG_TYPES:
                problems.append(f"{name} argument {index}: Kotlin type '{ktype}' has no JNI mapping here")
            elif ARG_TYPES[ktype] != jtype:
                problems.append(
                    f"{name} argument {index}: Kotlin {ktype} ({ARG_TYPES[ktype]}) but JNI {jtype}"
                )

    total = len(kotlin)
    if problems:
        print(f"JNI/Kotlin parity: {len(problems)} problem(s) across {total} entry point(s)")
        for problem in problems:
            print(f"  - {problem}")
        return 1

    print(f"JNI/Kotlin parity OK: {total} entry point(s), names, arity and types all agree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
