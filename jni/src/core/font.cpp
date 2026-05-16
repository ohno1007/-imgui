#include "font.h"

#include "imgui.h"

#include <android/font.h>
#include <android/font_matcher.h>
#include <android/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#define LOG_TAG "AImGui_Font"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)

namespace aimgui {

namespace {

struct SystemFontMatch {
    char   path[256];
    size_t collection_index;
    bool   ok;
};

// Ask Android (API 29+) for the system-chosen font that renders "中文" — this
// works no matter how the OEM has renamed or relocated their CJK font.
SystemFontMatch QuerySystemFontForCJK() {
    SystemFontMatch out{};

    AFontMatcher* matcher = AFontMatcher_create();
    if (!matcher) {
        LOGW("AFontMatcher_create() failed");
        return out;
    }
    AFontMatcher_setLocales(matcher, "zh-Hans,zh-CN,zh");

    const uint16_t probe[] = { 0x4E2D, 0x6587 }; // "中文"
    AFont* font = AFontMatcher_match(matcher, "sans-serif", probe, 2, nullptr);
    AFontMatcher_destroy(matcher);

    if (!font) {
        LOGW("AFontMatcher_match() returned null");
        return out;
    }

    if (const char* p = AFont_getFontFilePath(font)) {
        std::snprintf(out.path, sizeof(out.path), "%s", p);
        out.collection_index = AFont_getCollectionIndex(font);
        out.ok = (out.path[0] != '\0');
    }
    AFont_close(font);

    if (out.ok) LOGI("system CJK font: %s [collection=%zu]", out.path, out.collection_index);
    else        LOGW("AFont returned an empty path");
    return out;
}

// Last-resort fallback for OEMs that disable the font-matcher API.
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

ImFont* TryLoad(const char* path, int collection_index, float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.SizePixels  = size_pixels;
    cfg.PixelSnapH  = true;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.FontNo      = collection_index; // picks the right face in .ttc files
    ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_pixels, &cfg, CJKRanges());
    if (!f) LOGW("AddFontFromFileTTF failed: %s [%d]", path, collection_index);
    return f;
}

} // namespace

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    SystemFontMatch q = QuerySystemFontForCJK();
    if (q.ok) {
        if (ImFont* f = TryLoad(q.path, (int)q.collection_index, size_pixels)) {
            return f;
        }
    }

    if (const char* path = FindFallbackPath()) {
        LOGI("falling back to %s", path);
        if (ImFont* f = TryLoad(path, 0, size_pixels)) {
            return f;
        }
    }

    LOGW("no CJK font available — using built-in default (ASCII only)");
    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return ImGui::GetIO().Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
