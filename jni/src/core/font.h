#pragma once

struct ImFont;

namespace aimgui {

// Loads Dear ImGui's default font then merges a system CJK font (if found on
// the device) on top of it. Searches /system/fonts for common candidates:
//   NotoSansCJK-Regular.ttc, NotoSerifCJK-Regular.ttc, DroidSansFallbackFull.ttf,
//   DroidSansFallback.ttf, ...
//
// Falls back to default-only if no system font is available. The returned
// pointer is the merged font (or the default font), and is always non-null.
ImFont* LoadDefaultAndSystemCJKFont(float size_pixels = 22.0f);

} // namespace aimgui
