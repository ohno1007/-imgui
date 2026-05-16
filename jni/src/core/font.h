#pragma once

struct ImFont;

namespace aimgui {

// Loads ImGui's embedded default font (ASCII / Latin-1 only) at the given
// pixel size. No filesystem access, no system API calls.
ImFont* LoadDefaultAndSystemCJKFont(float size_pixels = 22.0f);

} // namespace aimgui
