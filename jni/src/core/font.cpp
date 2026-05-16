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

namespace aimgui {

namespace {

// ─── Phase 1: known well-named CJK fonts on common ROMs ──────────────────
const char* kKnownCJKPaths[] = {
    // AOSP / Pixel / generic
    "/system/fonts/NotoSansCJK-Regular.ttc",
    "/system/fonts/NotoSerifCJK-Regular.ttc",
    "/system/fonts/NotoSansCJKsc-Regular.otf",
    "/system/fonts/NotoSansCJKjp-Regular.otf",
    "/system/fonts/NotoSansSC-Regular.otf",
    "/system/fonts/NotoSansSC-Regular.ttf",
    // OEMs
    "/system/fonts/MiSans-Regular.ttf",                // Xiaomi
    "/system/fonts/MiSans-Regular.otf",
    "/system/fonts/MiLanProVF.ttf",
    "/system/fonts/HYQiHei.ttf",                       // some Vivo / Oppo
    "/system/fonts/HYQiHei-65.ttf",
    "/system/fonts/HONOR Sans-Regular.ttf",
    "/system/fonts/HwChinese-Medium.ttf",              // Huawei
    "/system/fonts/HwChinese-Regular.ttf",
    "/system/fonts/HarmonyOS_Sans_SC_Regular.ttf",
    // Legacy AOSP
    "/system/fonts/DroidSansFallback.ttf",
    "/system/fonts/DroidSansFallbackFull.ttf",
    // /product on A/B-partitioned devices
    "/product/fonts/NotoSansCJK-Regular.ttc",
    "/product/fonts/NotoSerifCJK-Regular.ttc",
    "/product/fonts/MiSans-Regular.ttf",
    "/product/fonts/HarmonyOS_Sans_SC_Regular.ttf",
    nullptr,
};

// ─── Phase 2: substrings to look for in filenames when scanning dirs ─────
const char* kCJKHints[] = {
    "CJK", "cjk",
    "Chinese", "chinese",
    "Hei", "hei",
    "Han", "han",
    "MiSans", "misans",
    "HwChinese",
    "HarmonyOS", "Harmony",
    "Noto", "noto",
    "DroidFallback", "DroidSansFallback",
    nullptr,
};

bool ContainsHint(const char* name) {
    for (const char** h = kCJKHints; *h; ++h) {
        if (std::strstr(name, *h)) return true;
    }
    return false;
}

// Walks the given directory once, looking for a file whose name contains
// any of the CJK hint substrings. Returns true and writes into `out` on hit.
bool ScanDirForCJK(const char* dir_path, char* out, size_t out_len) {
    DIR* dir = opendir(dir_path);
    if (!dir) return false;
    bool found = false;
    while (dirent* e = readdir(dir)) {
        if (e->d_name[0] == '.') continue;
        if (!ContainsHint(e->d_name)) continue;
        std::snprintf(out, out_len, "%s/%s", dir_path, e->d_name);
        if (access(out, R_OK) == 0) { found = true; break; }
    }
    closedir(dir);
    return found;
}

const char* FindCJKFont(char* scratch, size_t scratch_len) {
    // Phase 1: known paths
    for (const char** p = kKnownCJKPaths; *p; ++p) {
        if (access(*p, R_OK) == 0) {
            LOGI("found CJK font (known path): %s", *p);
            return *p;
        }
    }
    // Phase 2: scan common font directories
    const char* dirs[] = { "/system/fonts", "/product/fonts", nullptr };
    for (const char** d = dirs; *d; ++d) {
        if (ScanDirForCJK(*d, scratch, scratch_len)) {
            LOGI("found CJK font (scan): %s", scratch);
            return scratch;
        }
    }
    LOGW("no CJK font found in /system/fonts or /product/fonts");
    return nullptr;
}

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

    char scratch[256];
    if (const char* path = FindCJKFont(scratch, sizeof(scratch))) {
        ImFontConfig cfg;
        cfg.SizePixels  = size_pixels;
        cfg.PixelSnapH  = true;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_pixels, &cfg, CJKRanges())) {
            LOGI("loaded CJK font @ %.1fpx", size_pixels);
            return f;
        }
        LOGW("AddFontFromFileTTF failed for %s", path);
    }

    LOGW("falling back to built-in default font (ASCII only)");
    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return io.Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
