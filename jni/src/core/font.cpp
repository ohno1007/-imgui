#include "font.h"

#include "imgui.h"

#include <android/log.h>
#include <cstring>
#include <unistd.h>

#define LOG_TAG "AImGui_Font"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)

namespace aimgui {

namespace {

bool FindAndroidSystemFont(char* out_path, size_t out_len) {
    const char* font_dirs[] = { "/system/fonts", "/system/font", "/data/fonts" };

    char path[64] = {0};
    char* filename = nullptr;
    for (const char* dir : font_dirs) {
        if (access(dir, R_OK) == 0) {
            std::strcpy(path, dir);
            filename = path + std::strlen(dir);
            break;
        }
    }
    if (!filename) return false;

    *filename++ = '/';
    std::strcpy(filename, "NotoSansCJK-Regular.ttc");
    if (access(path, R_OK) != 0) {
        std::strcpy(filename, "NotoSerifCJK-Regular.ttc");
        if (access(path, R_OK) != 0) return false;
    }

    if (std::strlen(path) + 1 > out_len) return false;
    std::strcpy(out_path, path);
    return true;
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

ImFont* LoadDefaultAndSystemCJKFont(float size_pixels) {
    ImGuiIO& io = ImGui::GetIO();

    char path[64];
    if (FindAndroidSystemFont(path, sizeof(path))) {
        ImFontConfig cfg;
        cfg.SizePixels  = size_pixels;
        cfg.PixelSnapH  = true;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_pixels, &cfg, CJKRanges())) {
            LOGI("loaded CJK font %s @ %.1fpx", path, size_pixels);
            return f;
        }
        LOGW("AddFontFromFileTTF failed for %s", path);
    } else {
        LOGW("no NotoSansCJK / NotoSerifCJK font found in known dirs");
    }

    LOGW("falling back to built-in default font (ASCII only)");
    ImFontConfig cfg;
    cfg.SizePixels = size_pixels;
    return io.Fonts->AddFontDefault(&cfg);
}

} // namespace aimgui
