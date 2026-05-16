#!/usr/bin/env bash
# AImGui build script.
# Requires Android NDK (r25+ recommended). Set ANDROID_NDK_HOME or NDK_ROOT.
set -euo pipefail

NDK="${ANDROID_NDK_HOME:-${NDK_ROOT:-}}"
if [[ -z "$NDK" ]]; then
    echo "error: set ANDROID_NDK_HOME to your NDK root" >&2
    exit 1
fi

cd "$(dirname "$0")"

"$NDK/ndk-build" -j"$(nproc 2>/dev/null || echo 4)" \
    NDK_PROJECT_PATH=. \
    APP_BUILD_SCRIPT=jni/Android.mk \
    NDK_APPLICATION_MK=jni/Application.mk \
    "$@"

OUT=libs/arm64-v8a/AImGui
if [[ -f "$OUT" ]]; then
    SIZE=$(stat -c '%s' "$OUT" 2>/dev/null || wc -c < "$OUT")
    printf '\n  output : %s\n  size   : %s bytes (%.1f KB)\n' "$OUT" "$SIZE" "$(awk -v s="$SIZE" 'BEGIN{print s/1024}')"
fi
