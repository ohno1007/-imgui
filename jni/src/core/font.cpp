#include "font.h"

#include "imgui.h"

#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#define LOG_TAG "AImGui_Font"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)

// Probe order:
//   dirs   : /system/fonts -> /system/font -> /data/fonts -> /product/fonts
//   names  : NotoSansCJK / NotoSerifCJK first (AOSP/Pixel), then OEM CJK fonts
//            (MiSans on Xiaomi/MIUI/HyperOS, HwChinese / HarmonyOS_Sans on
//            Huawei/Honor, HYQiHei on Oppo/Vivo, DroidSansFallback as legacy).
namespace {

constexpr const char* kFontDirs[] = {
    "/system/fonts",
    "/system/font",
    "/data/fonts",
    "/product/fonts",
};

constexpr const char* kFontNames[] = {
    // AOSP / Pixel / generic
    "NotoSansCJK-Regular.ttc",
    "NotoSerifCJK-Regular.ttc",
    "NotoSansCJKsc-Regular.otf",
    "NotoSansSC-Regular.otf",
    "NotoSansSC-Regular.ttf",
    // Xiaomi / Redmi (MIUI / HyperOS)
    "MiSans-Regular.ttf",
    "MiSans-Regular.otf",
    "MiSans-Normal.otf",
    "MiSansVF.ttf",
    "MiLanProVF.ttf",
    // Huawei / Honor
    "HarmonyOS_Sans_SC_Regular.ttf",
    "HwChinese-Medium.ttf",
    "HwChinese-Regular.ttf",
    "HONOR Sans-Regular.ttf",
    // Oppo / Vivo
    "HYQiHei.ttf",
    "HYQiHei-65.ttf",
    // Legacy AOSP
    "DroidSansFallback.ttf",
    "DroidSansFallbackFull.ttf",
};

bool ResolveCJKFontPath(char* out_path, size_t out_len) {
    for (const char* dir : kFontDirs) {
        if (access(dir, R_OK) != 0) continue;
        for (const char* name : kFontNames) {
            std::snprintf(out_path, out_len, "%s/%s", dir, name);
            if (access(out_path, R_OK) == 0) return true;
        }
    }
    return false;
}

const ImWchar* CJKRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1
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

namespace ImGui {

bool My_Android_LoadSystemFont(float SizePixels) {
    char path[128];
    if (!ResolveCJKFontPath(path, sizeof(path))) {
        LOGW("no system CJK font found in /system/fonts /system/font /data/fonts /product/fonts");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.SizePixels  = SizePixels;
    cfg.PixelSnapH  = true;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    if (!io.Fonts->AddFontFromFileTTF(path, SizePixels, &cfg, CJKRanges())) {
        LOGW("AddFontFromFileTTF failed for %s", path);
        return false;
    }
    LOGI("loaded system CJK font %s @ %.1fpx", path, SizePixels);
    return true;
}

} // namespace ImGui

namespace aimgui {

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    if (ImGui::My_Android_LoadSystemFont(size_pixels)) {
        return ImGui::GetIO().Fonts->Fonts.back();
    }
    LOGW("falling back to built-in default font (ASCII only)");
    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return ImGui::GetIO().Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
