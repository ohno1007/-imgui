#include "font.h"

#include "imgui.h"

#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

#define LOG_TAG "AImGui_Font"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)

namespace {

constexpr const char* kFontDirs[] = {
    "/system/fonts",
    "/system/font",
    "/data/fonts",
    "/product/fonts",
    "/system_ext/fonts",
    "/mnt/system/system/fonts",
};

// Known CJK-capable font filenames, checked in order across kFontDirs.
constexpr const char* kKnownNames[] = {
    // AOSP / Pixel / generic
    "NotoSansCJK-Regular.ttc",
    "NotoSerifCJK-Regular.ttc",
    "NotoSansCJKsc-Regular.otf",
    "NotoSansCJKjp-Regular.otf",
    "NotoSansSC-Regular.otf",
    "NotoSansSC-Regular.ttf",
    "NotoSerifSC-Regular.otf",
    // Xiaomi / Redmi (MIUI / HyperOS)
    "MiSans-Regular.ttf",
    "MiSans-Normal.ttf",
    "MiSans-Medium.ttf",
    "MiSans-Regular.otf",
    "MiSansVF.ttf",
    "MiSans-VF.ttf",
    "MiLanProVF.ttf",
    "MiLanPro_VF.ttf",
    // Huawei / Honor
    "HarmonyOS_Sans_SC_Regular.ttf",
    "HarmonyOS_Sans_SC_Medium.ttf",
    "HarmonyOS_Sans_Regular.ttf",
    "HwChinese-Medium.ttf",
    "HwChinese-Regular.ttf",
    "HONOR Sans-Regular.ttf",
    "HONOR Sans CN-Regular.ttf",
    // Oppo / Vivo / OnePlus
    "OPSans-Regular.ttf",
    "OPSans-Medium.ttf",
    "OPSansVF.ttf",
    "HYQiHei.ttf",
    "HYQiHei-65.ttf",
    "DOUYIN-SansFont.ttf",
    // Samsung
    "SamsungOneUI-Regular.ttf",
    "SECCJK-Regular.ttc",
    // Legacy AOSP
    "DroidSansFallback.ttf",
    "DroidSansFallbackFull.ttf",
    "DroidSans.ttf",
};

// Substrings hinting that a font file likely contains CJK glyphs. Kept
// specific to avoid matching Latin-only siblings like NotoSans-Regular.ttf,
// DroidSans.ttf, HarmonyOS_Sans (the non-SC variant), etc.
constexpr const char* kCJKHints[] = {
    "CJK",   "cjk",
    "NotoSansSC", "NotoSerifSC", "NotoSansTC", "NotoSerifTC",
    "NotoSansHK", "NotoSerifHK",
    "MiSans", "misans",
    "MiLan",
    "HwChinese",
    "HarmonyOS_Sans_SC", "HarmonyOS_Sans_TC",
    "OPSansCN",
    "Hans",                  // *Hans*, e.g. SourceHanSans
    "HYQiHei", "FangZhengHei",
    "DroidSansFallback",
    "Chinese", "chinese",
};

bool NameLooksCJK(const char* name) {
    for (const char* hint : kCJKHints) {
        if (std::strstr(name, hint)) return true;
    }
    return false;
}

bool ProbeKnownNames(char* out, size_t out_len) {
    for (const char* dir : kFontDirs) {
        if (access(dir, R_OK) != 0) continue;
        for (const char* name : kKnownNames) {
            std::snprintf(out, out_len, "%s/%s", dir, name);
            if (access(out, R_OK) == 0) {
                LOGI("known-name match: %s", out);
                return true;
            }
        }
    }
    return false;
}

bool ProbeByDirScan(char* out, size_t out_len) {
    for (const char* dir : kFontDirs) {
        DIR* d = opendir(dir);
        if (!d) continue;
        while (dirent* e = readdir(d)) {
            const char* n = e->d_name;
            if (n[0] == '.') continue;
            const char* dot = std::strrchr(n, '.');
            if (!dot) continue;
            if (std::strcmp(dot, ".ttc") != 0 &&
                std::strcmp(dot, ".ttf") != 0 &&
                std::strcmp(dot, ".otf") != 0) continue;
            if (!NameLooksCJK(n)) continue;
            std::snprintf(out, out_len, "%s/%s", dir, n);
            if (access(out, R_OK) == 0) {
                closedir(d);
                LOGI("dir-scan match: %s", out);
                return true;
            }
        }
        closedir(d);
    }
    return false;
}

bool ResolveCJKFontPath(char* out, size_t out_len) {
    return ProbeKnownNames(out, out_len) || ProbeByDirScan(out, out_len);
}

} // namespace

namespace ImGui {

// Locate a system CJK font and let ImGui's dynamic font atlas (1.92+ with
// ImGuiBackendFlags_RendererHasTextures, which both imgui_impl_opengl3 and
// imgui_impl_vulkan set) rasterize glyphs on demand. No glyph_ranges
// argument — under the dynamic atlas it's a no-op and historically caused
// '?'-renderings or atlas overflow when used with the full CJK range.
bool My_Android_LoadSystemFont(float SizePixels) {
    char path[256];
    if (!ResolveCJKFontPath(path, sizeof(path))) {
        LOGW("no CJK font found in /system/fonts /system/font /data/fonts "
             "/product/fonts /system_ext/fonts /mnt/system/system/fonts");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.SizePixels  = SizePixels;
    cfg.PixelSnapH  = true;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;

    ImFont* f = io.Fonts->AddFontFromFileTTF(path, SizePixels, &cfg);
    if (!f) {
        LOGW("AddFontFromFileTTF failed for %s", path);
        return false;
    }
    LOGI("loaded CJK font %s @ %.1fpx (dynamic atlas)", path, SizePixels);
    return true;
}

} // namespace ImGui

namespace aimgui {

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::My_Android_LoadSystemFont(size_pixels)) {
        return io.Fonts->Fonts.back();
    }
    LOGW("falling back to built-in default font (ASCII only)");
    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return io.Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
