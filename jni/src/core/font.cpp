#include "font.h"

#include "imgui.h"

#include <unistd.h>

namespace aimgui {

namespace {

const char* kFontCandidates[] = {
    "/system/fonts/NotoSansCJK-Regular.ttc",
    "/system/fonts/NotoSerifCJK-Regular.ttc",
    "/system/fonts/NotoSansCJKsc-Regular.otf",
    "/system/fonts/DroidSansFallbackFull.ttf",
    "/system/fonts/DroidSansFallback.ttf",
    "/system/fonts/NotoSansSC-Regular.otf",
    nullptr,
};

const char* FindSystemFont() {
    for (const char** p = kFontCandidates; *p; ++p) {
        if (access(*p, R_OK) == 0) return *p;
    }
    return nullptr;
}

const ImWchar* MinimalCJKRanges() {
    // Light-weight range: ASCII + CJK Unified Ideographs + CJK Symbols
    // (~21k glyphs at most; ImGui rasterizes lazily so atlas stays small).
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,
        0x2000, 0x206F,
        0x3000, 0x30FF,
        0x31F0, 0x31FF,
        0x4E00, 0x9FFF,
        0xFF00, 0xFFEF,
        0,
    };
    return ranges;
}

} // namespace

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig base;
    base.SizePixels = size_pixels;
    base.OversampleH = base.OversampleV = 1;
    ImFont* font = io.Fonts->AddFontDefault(&base);

    if (const char* path = FindSystemFont()) {
        ImFontConfig merge;
        merge.MergeMode = true;
        merge.SizePixels = size_pixels;
        merge.PixelSnapH = true;
        merge.OversampleH = merge.OversampleV = 1;
        ImFont* cjk = io.Fonts->AddFontFromFileTTF(path, size_pixels, &merge, MinimalCJKRanges());
        if (cjk) font = cjk;
    }
    return font;
}

} // namespace aimgui
