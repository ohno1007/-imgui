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

// Forward-declare the API 29+ font-matcher types without including the
// headers, so we can build with APP_PLATFORM=24 and resolve at runtime.
struct AFontMatcher;
struct AFont;

namespace aimgui {

namespace {

using PFN_AFontMatcher_create     = AFontMatcher* (*)();
using PFN_AFontMatcher_destroy    = void (*)(AFontMatcher*);
using PFN_AFontMatcher_setLocales = void (*)(AFontMatcher*, const char*);
using PFN_AFontMatcher_match      = AFont* (*)(const AFontMatcher*, const char*, const uint16_t*, uint32_t, uint32_t*);
using PFN_AFont_getFontFilePath   = const char* (*)(const AFont*);
using PFN_AFont_getCollectionIndex= size_t (*)(const AFont*);
using PFN_AFont_close             = void (*)(AFont*);

struct FontMatcherApi {
    PFN_AFontMatcher_create      create      = nullptr;
    PFN_AFontMatcher_destroy     destroy     = nullptr;
    PFN_AFontMatcher_setLocales  setLocales  = nullptr;
    PFN_AFontMatcher_match       match       = nullptr;
    PFN_AFont_getFontFilePath    getPath     = nullptr;
    PFN_AFont_getCollectionIndex getIndex    = nullptr;
    PFN_AFont_close              closeFont   = nullptr;
    bool ok = false;
};

FontMatcherApi LoadFontMatcherApi() {
    FontMatcherApi a;
    void* h = dlopen("libandroid.so", RTLD_NOW);
    if (!h) {
        LOGW("dlopen libandroid.so failed: %s", dlerror());
        return a;
    }
    a.create     = (PFN_AFontMatcher_create)     dlsym(h, "AFontMatcher_create");
    a.destroy    = (PFN_AFontMatcher_destroy)    dlsym(h, "AFontMatcher_destroy");
    a.setLocales = (PFN_AFontMatcher_setLocales) dlsym(h, "AFontMatcher_setLocales");
    a.match      = (PFN_AFontMatcher_match)      dlsym(h, "AFontMatcher_match");
    a.getPath    = (PFN_AFont_getFontFilePath)   dlsym(h, "AFont_getFontFilePath");
    a.getIndex   = (PFN_AFont_getCollectionIndex)dlsym(h, "AFont_getCollectionIndex");
    a.closeFont  = (PFN_AFont_close)             dlsym(h, "AFont_close");
    a.ok = a.create && a.destroy && a.setLocales && a.match && a.getPath && a.getIndex && a.closeFont;
    if (!a.ok) LOGW("AFontMatcher symbols not all resolved (need Android 10+)");
    return a;
}

struct SystemFontMatch {
    char   path[256];
    size_t collection_index;
    bool   ok;
};

SystemFontMatch QuerySystemFontForCJK() {
    SystemFontMatch out{};
    static FontMatcherApi api = LoadFontMatcherApi();
    if (!api.ok) return out;

    AFontMatcher* matcher = api.create();
    if (!matcher) {
        LOGW("AFontMatcher_create() returned null");
        return out;
    }
    api.setLocales(matcher, "zh-Hans,zh-CN,zh");

    const uint16_t probe[] = { 0x4E2D, 0x6587 }; // "中文"
    AFont* font = api.match(matcher, "sans-serif", probe, 2, nullptr);
    api.destroy(matcher);
    if (!font) {
        LOGW("AFontMatcher_match() returned null");
        return out;
    }

    if (const char* p = api.getPath(font)) {
        std::snprintf(out.path, sizeof(out.path), "%s", p);
        out.collection_index = api.getIndex(font);
        if (out.path[0] && access(out.path, R_OK) == 0) {
            out.ok = true;
            LOGI("system CJK font: %s [collection=%zu]", out.path, out.collection_index);
        } else {
            LOGW("AFont path not readable: %s", out.path);
        }
    }
    api.closeFont(font);
    return out;
}

// Last-resort fallback for ROMs that disable the font-matcher API.
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

ImFont* TryLoad(const char* path, unsigned collection_index, float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.SizePixels  = size_pixels;
    cfg.PixelSnapH  = true;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.FontNo      = collection_index;
    ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_pixels, &cfg, CJKRanges());
    if (!f) LOGW("AddFontFromFileTTF failed: %s [%u]", path, collection_index);
    else    LOGI("loaded font: %s [%u]", path, collection_index);
    return f;
}

} // namespace

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    // Always seed the atlas with the embedded default first, so we have
    // *something* renderable even if every CJK lookup below fails.
    ImFontConfig base;
    base.SizePixels = size_pixels;
    base.OversampleH = base.OversampleV = 1;
    ImFont* fallback = ImGui::GetIO().Fonts->AddFontDefault(&base);

    SystemFontMatch q = QuerySystemFontForCJK();
    if (q.ok) {
        if (ImFont* f = TryLoad(q.path, (unsigned)q.collection_index, size_pixels)) {
            return f;
        }
    }

    if (const char* path = FindFallbackPath()) {
        LOGI("trying hard-coded fallback: %s", path);
        if (ImFont* f = TryLoad(path, 0u, size_pixels)) {
            return f;
        }
    }

    LOGW("no CJK font available — using built-in default (ASCII only)");
    return fallback;
}

} // namespace aimgui
