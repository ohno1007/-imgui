#include "font.h"

#include "imgui.h"

namespace aimgui {

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return ImGui::GetIO().Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
