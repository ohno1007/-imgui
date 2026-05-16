#!/usr/bin/env bash
# Builds java/src/**/*.java into java/build/classes.dex.
# Requires ANDROID_HOME pointing at an Android SDK with at least one
# `platforms/android-*/android.jar` and `cmdline-tools/latest/bin/d8`.
set -euo pipefail

cd "$(dirname "$0")"

SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [[ -z "$SDK" ]]; then
    echo "error: set ANDROID_HOME to your Android SDK" >&2
    exit 1
fi

ANDROID_JAR=$(find "$SDK/platforms" -name android.jar -print -quit 2>/dev/null || true)
if [[ -z "$ANDROID_JAR" ]]; then
    echo "error: no android.jar under $SDK/platforms" >&2
    exit 1
fi

D8="$SDK/cmdline-tools/latest/bin/d8"
if [[ ! -x "$D8" ]]; then
    echo "error: $D8 not executable" >&2
    exit 1
fi

rm -rf build
mkdir -p build/classes

javac -source 8 -target 8 \
      -bootclasspath "$ANDROID_JAR" \
      -d build/classes \
      $(find src -name '*.java')

"$D8" --release --output build $(find build/classes -name '*.class')

echo
echo "  dex   : $(realpath build/classes.dex)"
echo "  size  : $(stat -c '%s' build/classes.dex) bytes"
