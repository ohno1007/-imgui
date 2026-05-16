#include "font.h"

#include "imgui.h"

#include <android/log.h>
#include <unistd.h>

#define LOG_TAG "AImGui_Font"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)

namespace aimgui {

namespace {

// Candidate system fonts, in priority order. First readable path wins.
const char* kFontCandidates[] = {
    // AOSP / Pixel / generic
    "/system/fonts/NotoSansCJK-Regular.ttc",
    "/system/fonts/NotoSerifCJK-Regular.ttc",
    "/system/fonts/NotoSansCJKsc-Regular.otf",
    "/system/fonts/NotoSansCJKjp-Regular.otf",
    "/system/fonts/NotoSansSC-Regular.otf",
    "/system/fonts/NotoSansSC-Regular.ttf",
    // OEM-specific CJK fonts
    "/system/fonts/MiSans-Regular.ttf",          // Xiaomi
    "/system/fonts/MiLanProVF.ttf",
    "/system/fonts/HYQiHei.ttf",                 // some Vivo / Oppo
    "/system/fonts/HONOR Sans-Regular.ttf",
    "/system/fonts/HwChinese-Medium.ttf",        // Huawei
    "/system/fonts/HwChinese-Regular.ttf",
    "/system/fonts/HarmonyOS_Sans_SC_Regular.ttf",
    // Legacy
    "/system/fonts/DroidSansFallback.ttf",
    "/system/fonts/DroidSansFallbackFull.ttf",
    // Mounted under /product on newer A/B partitioned devices
    "/product/fonts/NotoSansCJK-Regular.ttc",
    "/product/fonts/NotoSerifCJK-Regular.ttc",
    nullptr,
};

const char* FindSystemFont() {
    for (const char** p = kFontCandidates; *p; ++p) {
        if (access(*p, R_OK) == 0) return *p;
    }
    return nullptr;
}

// A reasonable Chinese-friendly range: ASCII + CJK Unified Ideographs +
// common punctuation. ImGui 1.92 rasterizes glyphs lazily so listing this
// range doesn't cost atlas space up front.
const ImWchar* CJKRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
        0x2000, 0x206F, // General Punctuation
        0x3000, 0x30FF, // CJK Symbols & Punctuation, Hiragana, Katakana
        0x31F0, 0x31FF, // Katakana Phonetic Extensions
        0x4E00, 0x9FFF, // CJK Unified Ideographs
        0xFF00, 0xFFEF, // Half-/full-width forms
        0,
    };
    return ranges;
}

} // namespace

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();

    if (const char* path = FindSystemFont()) {
        ImFontConfig cfg;
        cfg.SizePixels  = size_pixels;
        cfg.PixelSnapH  = true;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_pixels, &cfg, CJKRanges())) {
            LOGI("loaded system CJK font: %s (%.1fpx)", path, size_pixels);
            return f;
        }
        LOGW("AddFontFromFileTTF failed for %s", path);
    } else {
        LOGW("no /system/fonts CJK candidate found");
    }

    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return io.Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
