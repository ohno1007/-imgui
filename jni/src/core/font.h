#pragma once

struct ImFont;

namespace aimgui {

// Loads a system CJK font (NotoSansCJK / MiSans / HwChinese / DroidSans
// Fallback / ...) as the primary ImGui font, so the UI can render the
// full Latin + CJK Unified Ideographs range. Falls back to the embedded
// default font (ASCII / Latin-1 only) when no system CJK font is found.
ImFont* LoadDefaultAndSystemCJKFont(float size_pixels = 22.0f);

} // namespace aimgui
