#include "font.h"

#include "imgui.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

namespace aimgui {

// ─── Weak-linked Android NDK font-matcher (API 29+) ───────────────────────
// These declarations are weak so the *binary builds and loads* even when
// linking against the API 24 stub of libandroid.so. At runtime the dynamic
// linker resolves them against the device's actual libandroid.so — if the
// device is Android 10+ the symbols light up, otherwise the function
// pointers stay NULL and we fall back. NO dlopen / dlsym is involved.
extern "C" {

struct AFontMatcher;
struct AFont;

__attribute__((weak)) AFontMatcher* AFontMatcher_create();
__attribute__((weak)) void          AFontMatcher_destroy(AFontMatcher*);
__attribute__((weak)) void          AFontMatcher_setLocales(AFontMatcher*, const char*);
__attribute__((weak)) AFont*        AFontMatcher_match(const AFontMatcher*, const char*,
                                                       const uint16_t*, uint32_t, uint32_t*);
__attribute__((weak)) const char*   AFont_getFontFilePath(const AFont*);
__attribute__((weak)) void          AFont_close(AFont*);

} // extern "C"

namespace {

// Ask Android's NDK font-matcher (API 29+) which font it would use to
// render the locale-tagged probe text "中文". Writes the absolute path to
// out_path and returns true on success.
bool QueryAndroidFontMatcher(char* out_path, size_t out_path_len) {
    if (out_path == nullptr || out_path_len < 2) return false;

    // Skip cleanly if any required symbol is missing on this device.
    if (AFontMatcher_create      == nullptr ||
        AFontMatcher_destroy     == nullptr ||
        AFontMatcher_setLocales  == nullptr ||
        AFontMatcher_match       == nullptr ||
        AFont_getFontFilePath    == nullptr ||
        AFont_close              == nullptr) {
        return false;
    }

    AFontMatcher* matcher = AFontMatcher_create();
    if (!matcher) return false;
    AFontMatcher_setLocales(matcher, "zh-Hans,zh-CN,zh");

    const uint16_t probe[] = { 0x4E2D, 0x6587 }; // "中文"
    AFont* font = AFontMatcher_match(matcher, "sans-serif", probe, 2, nullptr);
    AFontMatcher_destroy(matcher);
    if (!font) return false;

    bool ok = false;
    if (const char* p = AFont_getFontFilePath(font)) {
        if (p[0] && access(p, R_OK) == 0) {
            std::snprintf(out_path, out_path_len, "%s", p);
            ok = true;
        }
    }
    AFont_close(font);
    return ok;
}

// ─── Plain file-system fallback for ROMs / API levels without the API ────
const char* kKnownCJKPaths[] = {
    "/system/fonts/NotoSansCJK-Regular.ttc",
    "/system/fonts/NotoSerifCJK-Regular.ttc",
    "/system/fonts/NotoSansCJKsc-Regular.otf",
    "/system/fonts/NotoSansCJKjp-Regular.otf",
    "/system/fonts/NotoSansSC-Regular.otf",
    "/system/fonts/NotoSansSC-Regular.ttf",
    "/system/fonts/MiSans-Regular.ttf",
    "/system/fonts/MiSans-Regular.otf",
    "/system/fonts/MiLanProVF.ttf",
    "/system/fonts/HYQiHei.ttf",
    "/system/fonts/HYQiHei-65.ttf",
    "/system/fonts/HONOR Sans-Regular.ttf",
    "/system/fonts/HwChinese-Medium.ttf",
    "/system/fonts/HwChinese-Regular.ttf",
    "/system/fonts/HarmonyOS_Sans_SC_Regular.ttf",
    "/system/fonts/DroidSansFallback.ttf",
    "/system/fonts/DroidSansFallbackFull.ttf",
    "/product/fonts/NotoSansCJK-Regular.ttc",
    "/product/fonts/NotoSerifCJK-Regular.ttc",
    "/product/fonts/MiSans-Regular.ttf",
    "/product/fonts/HarmonyOS_Sans_SC_Regular.ttf",
    nullptr,
};

const char* kCJKHints[] = {
    "CJK", "cjk", "Chinese", "chinese",
    "Hei", "hei", "Han", "han",
    "MiSans", "misans", "HwChinese",
    "HarmonyOS", "Harmony", "Noto", "noto",
    "DroidFallback", "DroidSansFallback",
    nullptr,
};

bool ContainsHint(const char* name) {
    for (const char** h = kCJKHints; *h; ++h) {
        if (std::strstr(name, *h)) return true;
    }
    return false;
}

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

const char* FindFallbackCJKFont(char* scratch, size_t scratch_len) {
    for (const char** p = kKnownCJKPaths; *p; ++p) {
        if (access(*p, R_OK) == 0) return *p;
    }
    const char* dirs[] = { "/system/fonts", "/product/fonts", nullptr };
    for (const char** d = dirs; *d; ++d) {
        if (ScanDirForCJK(*d, scratch, scratch_len)) return scratch;
    }
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

    char path_buf[256];
    const char* path = nullptr;

    // Preferred: the NDK font-matcher tells us exactly what the system uses.
    if (QueryAndroidFontMatcher(path_buf, sizeof(path_buf))) {
        path = path_buf;
    } else if (const char* fallback = FindFallbackCJKFont(path_buf, sizeof(path_buf))) {
        path = fallback;
    }

    if (path) {
        ImFontConfig cfg;
        cfg.SizePixels  = size_pixels;
        cfg.PixelSnapH  = true;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_pixels, &cfg, CJKRanges())) {
            return f;
        }
    }

    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return io.Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
