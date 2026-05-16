#!/usr/bin/env bash
# Regenerate jni/src/core/bloom_vk_spv.h from jni/src/core/shaders/*.{vert,frag}.
# Requires the glslc binary that ships with Android NDK r25+ at
# $ANDROID_NDK_HOME/shader-tools/<host>/glslc.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${NDK_ROOT:-}}"
if [[ -z "$NDK" ]]; then
    echo "error: set ANDROID_NDK_HOME to your NDK root" >&2
    exit 1
fi

GLSLC="$(find "$NDK/shader-tools" -name glslc -type f | head -n1)"
if [[ -z "$GLSLC" || ! -x "$GLSLC" ]]; then
    echo "error: glslc not found under $NDK/shader-tools" >&2
    exit 1
fi

SRC_DIR="$ROOT/jni/src/core/shaders"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

"$GLSLC" "$SRC_DIR/fs_quad.vert"   -o "$TMP/vert.spv"
"$GLSLC" "$SRC_DIR/threshold.frag" -o "$TMP/thresh.spv"
"$GLSLC" "$SRC_DIR/blur.frag"      -o "$TMP/blur.spv"
"$GLSLC" "$SRC_DIR/composite.frag" -o "$TMP/comp.spv"

python3 - "$TMP" > "$ROOT/jni/src/core/bloom_vk_spv.h" <<'PY'
import sys, struct, os
tmp = sys.argv[1]
def emit(name, path):
    data = open(os.path.join(tmp, path), 'rb').read()
    n = len(data) // 4
    words = struct.unpack(f'<{n}I', data)
    lines = [f"inline constexpr uint32_t {name}[{n}] = {{"]
    for i in range(0, n, 8):
        chunk = words[i:i+8]
        lines.append("    " + ", ".join(f"0x{w:08x}" for w in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)
print("// AUTO-GENERATED from jni/src/core/shaders/*.{vert,frag} via glslc.")
print("// Regenerate via scripts/compile_bloom_vk_shaders.sh.")
print("#pragma once")
print("#include <cstdint>")
print()
print("namespace aimgui::bloom_vk_spv {")
print()
print(emit('kVS', 'vert.spv')); print()
print(emit('kFS_Threshold', 'thresh.spv')); print()
print(emit('kFS_Blur', 'blur.spv')); print()
print(emit('kFS_Composite', 'comp.spv')); print()
print("} // namespace aimgui::bloom_vk_spv")
PY

echo "Regenerated jni/src/core/bloom_vk_spv.h"
