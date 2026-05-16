#include "font.h"

#include "imgui.h"

#include <android/log.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

#define LOG_TAG "AImGui_Font"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)

// Opaque API 29+ types referenced only through dlsym, so this builds with
// APP_PLATFORM=24 and gracefully no-ops on older devices / stripped ROMs.
struct AFontMatcher;
struct AFont;

namespace aimgui {

namespace {

using PFN_AFontMatcher_create     = AFontMatcher* (*)();
using PFN_AFontMatcher_destroy    = void (*)(AFontMatcher*);
using PFN_AFontMatcher_setLocales = void (*)(AFontMatcher*, const char*);
using PFN_AFontMatcher_match      = AFont* (*)(const AFontMatcher*, const char*, const uint16_t*, uint32_t, uint32_t*);
using PFN_AFont_getFontFilePath   = const char* (*)(const AFont*);
using PFN_AFont_close             = void (*)(AFont*);

// Ask Android (API 29+) which font it would actually use for "中文". Writes
// the absolute path into `out_path` and returns true on success. Doesn't
// touch ImGui — just produces a path the caller can hand to AddFontFromFileTTF.
bool QueryAFontMatcherForCJK(char* out_path, size_t out_path_len) {
    if (!out_path || out_path_len < 2) return false;
    out_path[0] = '\0';

    void* h = dlopen("libandroid.so", RTLD_NOW);
    if (!h) {
        LOGW("dlopen libandroid.so failed: %s", dlerror());
        return false;
    }

    auto create     = (PFN_AFontMatcher_create)     dlsym(h, "AFontMatcher_create");
    auto destroy    = (PFN_AFontMatcher_destroy)    dlsym(h, "AFontMatcher_destroy");
    auto setLocales = (PFN_AFontMatcher_setLocales) dlsym(h, "AFontMatcher_setLocales");
    auto match      = (PFN_AFontMatcher_match)      dlsym(h, "AFontMatcher_match");
    auto getPath    = (PFN_AFont_getFontFilePath)   dlsym(h, "AFont_getFontFilePath");
    auto closeFont  = (PFN_AFont_close)             dlsym(h, "AFont_close");
    if (!create || !destroy || !setLocales || !match || !getPath || !closeFont) {
        LOGW("AFontMatcher API not fully available — skipping");
        return false;
    }

    AFontMatcher* matcher = create();
    if (!matcher) return false;
    setLocales(matcher, "zh-Hans,zh-CN,zh");

    const uint16_t probe[] = { 0x4E2D, 0x6587 }; // "中文"
    AFont* font = match(matcher, "sans-serif", probe, 2, nullptr);
    destroy(matcher);
    if (!font) return false;

    bool ok = false;
    if (const char* p = getPath(font)) {
        if (p[0] && access(p, R_OK) == 0) {
            std::snprintf(out_path, out_path_len, "%s", p);
            ok = true;
            LOGI("AFontMatcher: %s", out_path);
        } else {
            LOGW("AFont path empty or unreadable: %s", p ? p : "(null)");
        }
    }
    closeFont(font);
    return ok;
}

const char* kFallbackPaths[] = {
    "/system/fonts/NotoSansCJK-Regular.ttc",
    "/system/fonts/NotoSerifCJK-Regular.ttc",
    "/system/fonts/DroidSansFallback.ttf",
    "/system/fonts/DroidSansFallbackFull.ttf",
    "/product/fonts/NotoSansCJK-Regular.ttc",
    nullptr,
};

const char* FindFallbackPath() {
    for (const char** p = kFallbackPaths; *p; ++p) {
        if (access(*p, R_OK) == 0) return *p;
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

ImFont* TryLoad(const char* path, float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.SizePixels  = size_pixels;
    cfg.PixelSnapH  = true;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    // Deliberately leaving FontNo at its default (0). All CJK ideographs are
    // present in face 0 of NotoSansCJK / DroidSansFallback, and passing a
    // non-zero face index has been observed to crash stb_truetype on some
    // OEM-shipped .ttc files in the past.
    ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_pixels, &cfg, CJKRanges());
    if (f) LOGI("loaded CJK font: %s", path);
    else   LOGW("AddFontFromFileTTF failed: %s", path);
    return f;
}

} // namespace

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();

    // 1) Ask the system (Android 10+) what CJK font it ships with.
    char afm_path[256];
    if (QueryAFontMatcherForCJK(afm_path, sizeof(afm_path))) {
        if (ImFont* f = TryLoad(afm_path, size_pixels)) return f;
    }

    // 2) Try a small list of well-known paths for older devices.
    if (const char* path = FindFallbackPath()) {
        if (ImFont* f = TryLoad(path, size_pixels)) return f;
    }

    // 3) No CJK font available — ImGui's embedded default (ASCII only).
    LOGW("no CJK font available — using built-in default");
    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return io.Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
