#include "ui/ui.h"

#include "imgui.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace aimgui {

namespace {

// ─── MD3 ripple manager ──────────────────────────────────────────────────
// Captures the tap position and rect of the last-drawn ImGui item when it
// becomes active, then paints an expanding clipped white tint on the
// foreground draw list. DrawAll() is called once per frame at the end of
// the UI to advance and render all live ripples.
namespace ripple {

struct Entry {
    ImGuiID id;
    ImVec2  origin;
    ImVec2  rect_min;
    ImVec2  rect_max;
    float   start;
};

std::vector<Entry> g_ripples;

void TouchLastItem() {
    if (!ImGui::IsItemActivated()) return;
    Entry e;
    e.id       = ImGui::GetItemID();
    e.origin   = ImGui::GetIO().MousePos;
    e.rect_min = ImGui::GetItemRectMin();
    e.rect_max = ImGui::GetItemRectMax();
    e.start    = (float)ImGui::GetTime();
    g_ripples.push_back(e);
}

void DrawAll() {
    const float now      = (float)ImGui::GetTime();
    const float duration = 0.55f;

    auto& v = g_ripples;
    for (auto it = v.begin(); it != v.end(); ) {
        const float t = (now - it->start) / duration;
        if (t >= 1.0f) { it = v.erase(it); continue; }

        const float ease   = 1.0f - (1.0f - t) * (1.0f - t); // quadratic ease-out
        const float dx     = it->rect_max.x - it->rect_min.x;
        const float dy     = it->rect_max.y - it->rect_min.y;
        const float max_r  = std::sqrt(dx * dx + dy * dy);
        const float radius = ease * max_r;
        const float alpha  = (1.0f - t) * 0.22f;

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->PushClipRect(it->rect_min, it->rect_max, true);
        dl->AddCircleFilled(it->origin, radius,
                            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)),
                            48);
        dl->PopClipRect();
        ++it;
    }
}

} // namespace ripple

// ─── Exit fragmentation animation ────────────────────────────────────────
// On 退出 click we synthesize ~90 small rotating, gravity-affected chips
// covering the current main window rect, fade the rest of the UI to zero,
// then signal the main loop to quit after the animation has played out.
namespace shatter {

struct Chip {
    ImVec2 pos;       // current screen-space center
    ImVec2 vel;
    float  rot;
    float  rot_vel;
    ImVec2 size;
    ImU32  color;     // fallback solid color if no snapshot texture
    ImVec2 uv0;       // top-left UV on the scene snapshot (set at spawn)
    ImVec2 uv1;       // bottom-right UV (set at spawn)
};

std::vector<Chip> g_chips;

uint32_t g_seed = 0x9e3779b9;
uint32_t Rand() {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}
float Frand(float lo, float hi) {
    return lo + ((Rand() & 0xFFFF) / 65535.0f) * (hi - lo);
}

// Recursive longest-axis split with jittered midpoint and a random early
// termination chance — produces tile-covered-but-irregular chips that
// don't look like a regular grid.
void SubdivideChip(const ImVec2& mn, const ImVec2& mx, int depth,
                   float dw, float dh, const ImU32* palette) {
    const float w = mx.x - mn.x;
    const float h = mx.y - mn.y;
    constexpr float kMinSize  = 18.0f;
    constexpr int   kMaxDepth = 7;

    const bool stop = depth >= kMaxDepth
                   || w < kMinSize * 2.0f
                   || h < kMinSize * 2.0f
                   || (depth > 2 && Frand(0.0f, 1.0f) < 0.30f);

    if (stop) {
        Chip c;
        c.pos     = ImVec2((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
        c.size    = ImVec2(w, h);
        c.uv0     = ImVec2(mn.x / dw, mn.y / dh);
        c.uv1     = ImVec2(mx.x / dw, mx.y / dh);

        // Mass proxy: bigger area => heavier piece => falls harder + faster,
        // less initial kick, slower spin. Smaller pieces fling further up,
        // spin more, and drift down softly.
        const float area  = w * h;
        const float kRef  = 60.0f * 60.0f;
        float mass        = std::sqrt(area / kRef);
        if (mass < 0.5f) mass = 0.5f;
        if (mass > 1.8f) mass = 1.8f;
        const float inv   = 1.0f / mass;

        c.vel     = ImVec2(Frand(-280.0f, 280.0f) * inv,
                           Frand(-540.0f, -90.0f) * inv);
        c.rot     = 0.0f;
        c.rot_vel = Frand(-5.5f, 5.5f) * inv;
        c.color   = palette[(uint32_t)(c.pos.x + c.pos.y) & 3u];
        g_chips.push_back(c);
        return;
    }

    if (w > h) {
        const float t  = 0.5f + Frand(-0.18f, 0.18f);
        const float sx = mn.x + w * t;
        SubdivideChip(mn, ImVec2(sx, mx.y), depth + 1, dw, dh, palette);
        SubdivideChip(ImVec2(sx, mn.y), mx, depth + 1, dw, dh, palette);
    } else {
        const float t  = 0.5f + Frand(-0.18f, 0.18f);
        const float sy = mn.y + h * t;
        SubdivideChip(mn, ImVec2(mx.x, sy), depth + 1, dw, dh, palette);
        SubdivideChip(ImVec2(mn.x, sy), mx, depth + 1, dw, dh, palette);
    }
}

void Begin(const ImVec2& origin, const ImVec2& size,
           float display_w, float display_h) {
    g_chips.clear();
    g_chips.reserve(200);

    const ImU32 palette[4] = {
        ImGui::GetColorU32(ImGuiCol_TitleBgActive),
        ImGui::GetColorU32(ImVec4(0.22f, 0.40f, 0.78f, 1.0f)),
        ImGui::GetColorU32(ImGuiCol_FrameBg),
        ImGui::GetColorU32(ImVec4(0.30f, 0.62f, 1.0f, 1.0f)),
    };

    SubdivideChip(origin,
                  ImVec2(origin.x + size.x, origin.y + size.y),
                  0, display_w, display_h, palette);
}

// Advance + draw chips. If `snapshot_tex` is non-zero each chip is drawn as a
// textured quad sampling the prev-frame scene snapshot at its pinned UVs
// (real UI fragments). Otherwise each chip falls back to its solid colour.
void Step(float dt, float t01, ImTextureID snapshot_tex) {
    constexpr float kGravity = 1800.0f; // px / s^2

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    for (auto& c : g_chips) {
        // Bigger chips fall faster (heavier => higher effective gravity);
        // small fragments stay aloft longer.
        const float area = c.size.x * c.size.y;
        float mass = std::sqrt(area / (60.0f * 60.0f));
        if (mass < 0.5f) mass = 0.5f;
        if (mass > 1.8f) mass = 1.8f;

        c.vel.y += kGravity * mass * dt;
        c.pos.x += c.vel.x * dt;
        c.pos.y += c.vel.y * dt;
        c.rot   += c.rot_vel * dt;

        const float cs = std::cos(c.rot);
        const float sn = std::sin(c.rot);
        const float hx = c.size.x * 0.5f;
        const float hy = c.size.y * 0.5f;
        const ImVec2 corners[4] = {
            ImVec2(c.pos.x + (-hx * cs - -hy * sn), c.pos.y + (-hx * sn + -hy * cs)),
            ImVec2(c.pos.x + ( hx * cs - -hy * sn), c.pos.y + ( hx * sn + -hy * cs)),
            ImVec2(c.pos.x + ( hx * cs -  hy * sn), c.pos.y + ( hx * sn +  hy * cs)),
            ImVec2(c.pos.x + (-hx * cs -  hy * sn), c.pos.y + (-hx * sn +  hy * cs)),
        };

        const float fade = 1.0f - t01;
        const uint32_t a = (uint32_t)(255.0f * fade);

        if (snapshot_tex) {
            // UVs are corners 0,1,2,3 in screen-pinned order → top-left,
            // top-right, bottom-right, bottom-left of the chip's tile.
            const ImU32 tint = 0x00FFFFFFu | (a << 24);
            dl->AddImageQuad(snapshot_tex,
                             corners[0], corners[1], corners[2], corners[3],
                             ImVec2(c.uv0.x, c.uv0.y),
                             ImVec2(c.uv1.x, c.uv0.y),
                             ImVec2(c.uv1.x, c.uv1.y),
                             ImVec2(c.uv0.x, c.uv1.y),
                             tint);
        } else {
            const ImU32 col = (c.color & 0x00FFFFFFu) | (a << 24);
            dl->AddQuadFilled(corners[0], corners[1], corners[2], corners[3], col);
        }
    }
}

} // namespace shatter


enum class Page { Dashboard, Widgets, Window, Performance, About };

struct PageItem { Page id; const char* label; };
constexpr PageItem kPages[] = {
    { Page::Dashboard,   u8"概览"      },
    { Page::Widgets,     u8"控件"      },
    { Page::Window,      u8"窗口"      },
    { Page::Performance, u8"性能"      },
    { Page::About,       u8"关于"      },
};

constexpr int kFpsPresets[] = { 0, 30, 60, 90, 120, 144 };
constexpr const char* kFpsLabels =
    u8"垂直同步\0" "30\0" "60\0" "90\0" "120\0" "144\0";

int FpsToIndex(int fps) {
    for (int i = 0; i < IM_ARRAYSIZE(kFpsPresets); ++i)
        if (kFpsPresets[i] == fps) return i;
    return 0;
}

void ApplyStyleOnce() {
    static bool done = false;
    if (done) return;
    done = true;
    auto& s = ImGui::GetStyle();
    s.WindowRounding          = 12.0f;
    s.ChildRounding           = 10.0f;
    s.FrameRounding           = 6.0f;
    s.GrabRounding            = 6.0f;
    s.PopupRounding           = 6.0f;
    s.ScrollbarRounding       = 10.0f;
    s.WindowBorderSize        = 1.0f;
    s.FrameBorderSize         = 0.0f;
    s.WindowPadding           = ImVec2(0, 0);
    s.ItemSpacing             = ImVec2(14, 10);
    s.ItemInnerSpacing        = ImVec2(8, 6);
    s.FramePadding            = ImVec2(14, 10);
    s.ScrollbarSize           = 26.0f;
    s.GrabMinSize             = 16.0f;
    s.SeparatorTextBorderSize = 3.0f;
    s.SeparatorTextPadding    = ImVec2(28, 8);

    // Keep the title bar painted with the focused/active color even when the
    // window loses focus (we only have one window).
    s.Colors[ImGuiCol_TitleBg]          = s.Colors[ImGuiCol_TitleBgActive];
    s.Colors[ImGuiCol_TitleBgCollapsed] = s.Colors[ImGuiCol_TitleBgActive];
}

// SliderFloat with a pill-shaped grab whose width hugs the formatted value
// text. The default rectangular grab is suppressed; we draw our own pill on
// top and center the number inside it.
bool SliderFloatGrabValue(const char* label, float* v, float v_min, float v_max,
                          const char* fmt = "%.3f") {
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, IM_COL32(0, 0, 0, 0));
    bool changed = ImGui::SliderFloat(label, v, v_min, v_max, "");
    ImGui::PopStyleColor(2);

    const ImVec2 totalMin = ImGui::GetItemRectMin();
    const ImVec2 totalMax = ImGui::GetItemRectMax();

    // Item rect includes the trailing label; strip it to get the bar rect.
    const char* hash = std::strstr(label, "##");
    const char* visible_end = hash ? hash : label + std::strlen(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, visible_end);
    const float  inner      = ImGui::GetStyle().ItemInnerSpacing.x;
    const float  bar_w      = (totalMax.x - totalMin.x) -
                              (label_size.x > 0.0f ? label_size.x + inner : 0.0f);
    const ImVec2 barMin = totalMin;
    const ImVec2 barMax(totalMin.x + bar_w, totalMax.y);

    char buf[32];
    std::snprintf(buf, sizeof(buf), fmt, *v);
    const ImVec2 ts = ImGui::CalcTextSize(buf);

    // Pill width hugs the value text; height tracks the font so the digits
    // sit comfortably inside it.
    const float pad_x  = 18.0f;
    const float min_w  = ImGui::GetStyle().GrabMinSize;
    const float grab_w = (ts.x + pad_x > min_w) ? ts.x + pad_x : min_w;
    const float grab_h = ts.y + 10.0f;

    float t = (v_max != v_min) ? (*v - v_min) / (v_max - v_min) : 0.0f;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    const float xL = barMin.x + grab_w * 0.5f;
    const float xR = barMax.x - grab_w * 0.5f;
    const float cx = xL + t * (xR - xL);
    const float cy = (barMin.y + barMax.y) * 0.5f;
    const ImVec2 gMin(cx - grab_w * 0.5f, cy - grab_h * 0.5f);
    const ImVec2 gMax(cx + grab_w * 0.5f, cy + grab_h * 0.5f);

    const ImU32 col = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    dl->AddRectFilled(gMin, gMax, col, grab_h * 0.5f);
    dl->AddText(ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), IM_COL32_WHITE, buf);

    return changed;
}

void KV(const char* key, const char* fmt, ...) {
    ImGui::TextDisabled("%s", key);
    ImGui::SameLine(200.0f);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

// ─── Page contents ───────────────────────────────────────────────────────
void DrawDashboard(const UiState* state) {
    ImGui::SeparatorText(u8"概览");

    KV(u8"渲染后端",   "%s", state->renderer_name ? state->renderer_name : "?");
    KV(u8"ImGui 版本", "%s", ImGui::GetVersion());
    KV(u8"帧率",       "%.1f FPS  (%.2f ms)",
                       ImGui::GetIO().Framerate,
                       1000.0f / ImGui::GetIO().Framerate);
    KV(u8"防录屏",     "%s", state->permeate_record ? u8"已开启" : u8"已关闭");

    ImGui::Spacing();
    ImGui::SeparatorText(u8"提示");
    ImGui::TextWrapped(u8"按音量键可以把窗口折叠成屏幕顶部的灵动岛，再按一次或点击灵动岛展开回来。");
}

void DrawWidgets() {
    ImGui::SeparatorText(u8"基础控件");

    static int    counter = 0;
    static float  slider  = 0.5f;
    static bool   toggle  = false;
    static ImVec4 tint(0.40f, 0.70f, 1.00f, 1.0f);

    if (ImGui::Button(u8"点我"))  counter++;
    ripple::TouchLastItem();
    ImGui::SameLine();
    ImGui::Text(u8"计数 = %d", counter);

    SliderFloatGrabValue(u8"滑块", &slider, 0.0f, 1.0f, "%.3f");
    ImGui::Checkbox  (u8"开关",   &toggle); ripple::TouchLastItem();
    ImGui::ColorEdit4(u8"取色器", (float*)&tint);

    ImGui::Spacing();

    // Collapsible list with animated expand/collapse + shared-element row
    // transition. The header drives an exponential-eased "open factor" t in
    // [0..1]; the items are then wrapped in a BeginChild whose height is
    // `t * full_height` so they slide in/out under the header. A single
    // rounded highlight rect lerps from the old selected row to the new one
    // and is drawn beneath the row text via DrawList channels.
    {
        static bool   list_open       = true;
        static float  list_t          = 1.0f;
        static float  list_full_h     = 160.0f;
        static int    selected         = 0;
        static ImVec2 anim_min         = ImVec2(0, 0);
        static ImVec2 anim_max         = ImVec2(0, 0);
        static bool   anim_initialized = false;

        ImGui::SetNextItemOpen(list_open, ImGuiCond_FirstUseEver);
        list_open = ImGui::CollapsingHeader(u8"列表");
        ripple::TouchLastItem();

        const float dt    = ImGui::GetIO().DeltaTime;
        const float alpha = 1.0f - std::exp(-14.0f * dt);
        list_t += ((list_open ? 1.0f : 0.0f) - list_t) * alpha;

        if (list_t > 0.005f) {
            const float child_h = list_full_h * list_t;
            ImGui::BeginChild("##list_content", ImVec2(0, child_h),
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar);

            const char* items[] = { u8"第一项", u8"第二项", u8"第三项", u8"第四项" };
            const int N = IM_ARRAYSIZE(items);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->ChannelsSplit(2);
            dl->ChannelsSetCurrent(1);

            ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(0, 0, 0, 0));

            const float y_start = ImGui::GetCursorPosY();
            ImVec2 sel_min(0, 0), sel_max(0, 0);
            for (int i = 0; i < N; ++i) {
                if (ImGui::Selectable(items[i], selected == i)) selected = i;
                ripple::TouchLastItem();
                if (selected == i) {
                    sel_min = ImGui::GetItemRectMin();
                    sel_max = ImGui::GetItemRectMax();
                }
            }
            const float y_end = ImGui::GetCursorPosY();

            ImGui::PopStyleColor(3);

            dl->ChannelsSetCurrent(0);
            if (sel_max.y > sel_min.y) {
                if (!anim_initialized) {
                    anim_min = sel_min;
                    anim_max = sel_max;
                    anim_initialized = true;
                } else {
                    const float a2 = 1.0f - std::exp(-15.0f * dt);
                    anim_min.x += (sel_min.x - anim_min.x) * a2;
                    anim_min.y += (sel_min.y - anim_min.y) * a2;
                    anim_max.x += (sel_max.x - anim_max.x) * a2;
                    anim_max.y += (sel_max.y - anim_max.y) * a2;
                }
                const ImU32 col = ImGui::GetColorU32(ImVec4(0.22f, 0.40f, 0.78f, 0.55f));
                dl->AddRectFilled(anim_min, anim_max, col, 6.0f);
            }
            dl->ChannelsMerge();

            // Re-measure the natural list height once the expand animation
            // has fully settled so list_full_h tracks any future item count
            // changes.
            if (list_t > 0.99f && y_end > y_start) {
                list_full_h = y_end - y_start;
            }
            ImGui::EndChild();

            // ImGui adds a full ItemSpacing.y after EndChild regardless of
            // child height; without this adjustment the gap before the next
            // section would snap by one ItemSpacing.y at the moment we
            // stopped drawing the child, producing a visible "卡一下".
            const float spacing_y = ImGui::GetStyle().ItemSpacing.y;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (1.0f - list_t) * spacing_y);
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText(u8"进度");
    static float progress = 0.0f;
    progress += ImGui::GetIO().DeltaTime * 0.15f;
    if (progress > 1.0f) progress -= 1.0f;
    ImGui::ProgressBar(progress, ImVec2(-1, 0));
}

void DrawWindow(UiState* state) {
    ImGui::SeparatorText(u8"窗口表面");

    bool perm = state->permeate_record;
    if (ImGui::Checkbox(u8"防录屏(对屏幕录制 / 投屏隐藏)", &perm)) {
        state->request_permeate_toggle = true;
    }
    ripple::TouchLastItem();
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", state->permeate_record ? u8"已开启" : u8"已关闭");

    ImGui::TextWrapped(
        u8"开启后，SurfaceFlinger 图层使用 skipScreenshot 标志创建，"
        u8"屏幕录制和投屏不会捕获到本窗口。");

    ImGui::Spacing();
    ImGui::SeparatorText(u8"辉光");
    SliderFloatGrabValue(u8"辉光强度", &state->bloom_intensity, 0.0f, 2.5f, "%.2f");
    ImGui::TextWrapped(
        u8"调到 0 关闭后处理；默认 0.75。亮元素（白字、蓝高亮）会按"
        u8"luma > 0.6 阈值参与抽亮、双 pass 高斯模糊后回叠到画面上。");

    ImGui::Spacing();
    ImGui::SeparatorText(u8"主题");
    static int theme = 0;
    if (ImGui::Combo(u8"##theme", &theme, u8"深色\0浅色\0经典\0")) {
        switch (theme) {
            case 0: ImGui::StyleColorsDark();    break;
            case 1: ImGui::StyleColorsLight();   break;
            case 2: ImGui::StyleColorsClassic(); break;
        }
    }
    ripple::TouchLastItem();
}

void DrawPerformance(UiState* state) {
    ImGui::SeparatorText(u8"帧率限制");

    int fps_idx = FpsToIndex(state->target_fps);
    if (ImGui::Combo(u8"目标帧率", &fps_idx, kFpsLabels)) {
        state->target_fps = kFpsPresets[fps_idx];
    }
    ripple::TouchLastItem();

    ImGui::Spacing();
    KV(u8"当前帧率", "%.1f FPS", ImGui::GetIO().Framerate);
    KV(u8"帧时间",   "%.2f ms",  1000.0f / ImGui::GetIO().Framerate);

    ImGui::Spacing();
    ImGui::SeparatorText(u8"帧率曲线");

    constexpr int N = 240;
    static float history[N] = {};
    static int   offset     = 0;
    history[offset] = ImGui::GetIO().Framerate;
    offset = (offset + 1) % N;

    char overlay[32];
    std::snprintf(overlay, sizeof(overlay), "%.1f", ImGui::GetIO().Framerate);
    ImGui::PlotLines("##fps_plot", history, N, offset, overlay,
                     0.0f, 165.0f, ImVec2(-1, 90));
}

void DrawAbout() {
    ImGui::SeparatorText("AImGui");
    ImGui::TextWrapped(u8"一个极简的 Dear ImGui Android ARM64 ELF —— "
                       u8"无 JNI、无 APK、无 Activity，直接以原生二进制运行在 SurfaceFlinger 之上。");

    ImGui::Spacing();
    ImGui::SeparatorText(u8"特性");
    ImGui::BulletText(u8"Dear ImGui %s", ImGui::GetVersion());
    ImGui::BulletText(u8"Vulkan + OpenGL ES 3 自动回落");
    ImGui::BulletText(u8"VSync 锁帧的弹簧帧率器，几乎不烧 CPU");
    ImGui::BulletText(u8"只读触摸 (不创建 /dev/uinput，不影响系统)");
    ImGui::BulletText(u8"防录屏 SurfaceFlinger 标志");
    ImGui::BulletText(u8"音量键折叠/展开灵动岛");
    ImGui::BulletText(u8"自动加载系统中文字体 (NotoSansCJK / MiSans / HwChinese ...)");
}

// ─── Sidebar ─────────────────────────────────────────────────────────────
void DrawSidebar(Page& current, bool* keep_running, UiState* state) {
    constexpr float kInnerPadX     = 18.0f;
    constexpr float kInnerPadY     = 14.0f;
    constexpr float kSelectableH   = 44.0f;
    constexpr float kAccentInset   = 10.0f;
    constexpr float kAccentW       = 4.0f;
    constexpr float kFooterH       = 110.0f;
    constexpr float kBottomMargin  = 16.0f;

    // Click on a sidebar entry should land directly on the selected color —
    // no intermediate hover-gray or transient pressed-blue. Push the same
    // color into all three slots.
    const ImVec4 sel_bg(0.22f, 0.40f, 0.78f, 0.55f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,       ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header,        sel_bg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, sel_bg);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  sel_bg);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,       ImVec2(kInnerPadX, kInnerPadY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,         ImVec2(0, 4));
    // Inset the label inside the highlight rect so text doesn't touch the
    // selected (blue) background's left edge.
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.08f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,        ImVec2(0, 6));

    ImGui::BeginChild("##sidebar", ImVec2(230, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);

    const ImU32 accent = ImGui::GetColorU32(ImVec4(0.30f, 0.62f, 1.0f, 1.0f));
    for (const auto& p : kPages) {
        bool selected = (current == p.id);
        if (ImGui::Selectable(p.label, selected, 0, ImVec2(0, kSelectableH))) {
            current = p.id;
        }
        ripple::TouchLastItem();
        if (selected) {
            ImVec2 a = ImGui::GetItemRectMin();
            ImVec2 b = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(a.x - kAccentInset,            a.y + 8),
                ImVec2(a.x - kAccentInset + kAccentW, b.y - 8),
                accent, kAccentW * 0.5f);
        }
    }

    float remaining = ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - kFooterH - kBottomMargin;
    if (remaining > 0) ImGui::Dummy(ImVec2(0, remaining));

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", state->renderer_name ? state->renderer_name : "?");
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 12));
    if (ImGui::Button(u8"退出", ImVec2(-1, 0))) {
        if (!state->exit_anim_active) {
            // UV normalisation must use the *snapshot texture* dimensions —
            // which equal io.DisplaySize because the renderer sizes its
            // scene image to that. state->display_w/h are the physical
            // screen dimensions and would mis-map most chips off-frame.
            const ImGuiIO& io2 = ImGui::GetIO();
            shatter::Begin(state->last_full_pos, state->last_full_size,
                           io2.DisplaySize.x, io2.DisplaySize.y);
            state->exit_anim_active      = true;
            state->exit_anim_first_frame = true;
            state->exit_anim_start       = (float)ImGui::GetTime();
        }
    }
    ripple::TouchLastItem();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, kBottomMargin));

    ImGui::EndChild();

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(4);
}

// Forward decl — body lives further down, but DrawContent invokes it.
void DrawResizeGrip(const UiState* state);

void DrawContent(UiState* state, Page page) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 18));

    ImGui::BeginChild("##content", ImVec2(0, 0),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    switch (page) {
        case Page::Dashboard:   DrawDashboard(state);   break;
        case Page::Widgets:     DrawWidgets();          break;
        case Page::Window:      DrawWindow(state);      break;
        case Page::Performance: DrawPerformance(state); break;
        case Page::About:       DrawAbout();            break;
    }
    // Pips + preview frame only (input handled before Begin in DrawUi).
    DrawResizeGrip(state);
    ImGui::EndChild();

    ImGui::PopStyleVar();
}

void DrawIslandContent() {
    ImGuiIO& io = ImGui::GetIO();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f FPS", io.Framerate);

    ImVec2 ts = ImGui::CalcTextSize(buf);
    ImVec2 ws = ImGui::GetWindowSize();
    ImGui::SetCursorPos(ImVec2((ws.x - ts.x) * 0.5f,
                               (ws.y - ts.y) * 0.5f));
    ImGui::TextUnformatted(buf);
}

// Bottom-right grip rect (in screen coords) anchored to the main window.
ImVec2 GripMin(const UiState* state) {
    constexpr float kGrip = 40.0f;
    return ImVec2(state->last_full_pos.x + state->last_full_size.x - kGrip,
                  state->last_full_pos.y + state->last_full_size.y - kGrip);
}
ImVec2 GripMax(const UiState* state) {
    return ImVec2(state->last_full_pos.x + state->last_full_size.x,
                  state->last_full_pos.y + state->last_full_size.y);
}

// Resize input is detected BEFORE Begin so that the ImGuiWindowFlags_NoMove
// can be applied this very frame to stop ImGui from also interpreting the
// touch as a window-drag-start. Without this, a press on the grip would
// kick off both the grip drag (our code) and the main window's title-bar
// move (ImGui's built-in), and the window would slide around under the
// finger as the size grew.
//
// Hit-test is done with raw math (not ImGui::IsMouseHoveringRect) — that
// helper defaults to clipping against the *current window*'s ClipRect, and
// we're called outside any Begin/End so its clip rect is empty / wrong,
// which silently produces "no hit" forever.
void HandleResizeInput(UiState* state, const ImGuiIO& io) {
    const ImVec2 grip_min = GripMin(state);
    const ImVec2 grip_max = GripMax(state);

    const bool inside = io.MousePos.x >= grip_min.x && io.MousePos.x < grip_max.x &&
                        io.MousePos.y >= grip_min.y && io.MousePos.y < grip_max.y;

    if (io.MouseClicked[0] && !state->resizing && inside) {
        state->resizing                = true;
        state->resize_drag_start_mouse = io.MousePos;
        state->resize_drag_start_size  = state->last_full_size;
        state->resize_target_size      = state->last_full_size;
    }
    if (state->resizing && io.MouseDown[0]) {
        const ImVec2 d(io.MousePos.x - state->resize_drag_start_mouse.x,
                       io.MousePos.y - state->resize_drag_start_mouse.y);
        state->resize_target_size = ImVec2(
            std::max(700.0f, state->resize_drag_start_size.x + d.x),
            std::max(560.0f, state->resize_drag_start_size.y + d.y));
    }
    if (state->resizing && !io.MouseDown[0]) {
        state->resizing        = false;
        state->resize_anim_vel = ImVec2(0, 0);
    }
}

// Visual-only: pips at the corner + preview frame while resizing. Input
// is handled by HandleResizeInput at the top of DrawUi.
void DrawResizeGrip(const UiState* state) {
    const ImVec2 grip_min = GripMin(state);
    const ImVec2 grip_max = GripMax(state);

    const bool inside = ImGui::IsMouseHoveringRect(grip_min, grip_max);
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    const ImU32 col = ImGui::GetColorU32(
        state->resizing ? ImGuiCol_ResizeGripActive
                        : (inside ? ImGuiCol_ResizeGripHovered : ImGuiCol_ResizeGrip));

    for (int i = 0; i < 3; ++i) {
        const float o = 8.0f + i * 7.0f;
        fg->AddLine(ImVec2(grip_max.x - o, grip_max.y - 5),
                    ImVec2(grip_max.x - 5, grip_max.y - o),
                    col, 3.0f);
    }

    if (state->resizing) {
        const ImVec2 a = state->last_full_pos;
        const ImVec2 b(a.x + state->resize_target_size.x,
                       a.y + state->resize_target_size.y);
        fg->AddRect(a, b,
                    ImGui::GetColorU32(ImVec4(0.30f, 0.62f, 1.0f, 0.95f)),
                    12.0f, 0, 5.0f);
    }
}

// Critically-ish damped spring with mild overshoot for the "灵动" feel.
void UpdateSpring(float* pos, float* vel, float target, float dt) {
    constexpr float kOmega = 12.0f;
    constexpr float kZeta  = 0.82f;
    const float diff  = target - *pos;
    const float accel = kOmega * kOmega * diff - 2.0f * kZeta * kOmega * (*vel);
    *vel += accel * dt;
    *pos += (*vel) * dt;
    if (std::abs(diff) < 0.001f && std::abs(*vel) < 0.005f) {
        *pos = target;
        *vel = 0.0f;
    }
}

} // namespace

void DrawUi(UiState* state, bool* keep_running) {
    ApplyStyleOnce();

    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f;

    // Past the click frame the entire main window stops rendering — the
    // shatter chips, which sample the frozen pre-click scene snapshot,
    // visually replace the UI. Where a chip has flown off, the rest of
    // the system surface shows through (no mask, no leftover frame).
    if (state->exit_anim_active && !state->exit_anim_first_frame) {
        const float now     = (float)ImGui::GetTime();
        const float t01     = (now - state->exit_anim_start) / 1.2f;
        const float clamped = t01 < 0.0f ? 0.0f : (t01 > 1.0f ? 1.0f : t01);

        ripple::DrawAll();
        shatter::Step(dt, clamped,
                      (ImTextureID)(uintptr_t)state->scene_snapshot_id);

        if (t01 >= 1.0f) {
            state->exit_anim_active = false;
            *keep_running = false;
        }
        return;
    }

    // Handle grip press/drag/release first so the NoMove flag below sees
    // an up-to-date state->resizing this frame.
    HandleResizeInput(state, io);

    // Resize spring: when no drag is in progress, ease last_full_size
    // toward whatever target the previous drag (if any) parked it at.
    if (!state->resizing) {
        UpdateSpring(&state->last_full_size.x, &state->resize_anim_vel.x,
                     state->resize_target_size.x, dt);
        UpdateSpring(&state->last_full_size.y, &state->resize_anim_vel.y,
                     state->resize_target_size.y, dt);
    } else {
        // Hold the live window at the size it had when the drag started;
        // the preview frame reads from resize_target_size.
        state->last_full_size = state->resize_drag_start_size;
    }

    const float target = state->collapsed ? 0.0f : 1.0f;
    UpdateSpring(&state->expand, &state->expand_vel, target, dt);
    const float t = state->expand;
    const float lt = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

    constexpr float kIslandW   = 280.0f;
    constexpr float kIslandH   = 56.0f;
    constexpr float kIslandTop = 28.0f;

    const float dw = state->display_w > 0 ? (float)state->display_w : io.DisplaySize.x;
    const ImVec2 island_pos (dw * 0.5f - kIslandW * 0.5f, kIslandTop);
    const ImVec2 island_size(kIslandW, kIslandH);

    auto lerp = [](ImVec2 a, ImVec2 b, float u) {
        return ImVec2(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u);
    };
    const ImVec2 win_pos  = lerp(island_pos,  state->last_full_pos,  lt);
    const ImVec2 win_size = lerp(island_size, state->last_full_size, lt);

    const bool show_chrome    = (lt > 0.55f);
    const bool overriding_pos = (lt < 0.999f);

    if (overriding_pos) {
        ImGui::SetNextWindowPos (win_pos);
        ImGui::SetNextWindowSize(win_size);
    } else {
        ImGui::SetNextWindowPos (state->last_full_pos,  ImGuiCond_FirstUseEver);
        // last_full_size is now driven by the custom resize spring, so
        // push it every frame instead of only once.
        ImGui::SetNextWindowSize(state->last_full_size, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(700, 560), ImVec2(FLT_MAX, FLT_MAX));
    }

    const float rounding = (kIslandH * 0.5f) * (1.0f - lt) + 12.0f * lt;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);

    // Suppress ImGui's built-in resize handle: the custom DrawResizeGrip
    // (with preview-then-animate behaviour) owns resizing. NoMove is also
    // applied during a grip drag to stop ImGui from interpreting the same
    // touch as a window-drag-start and sliding the window under the finger.
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoResize;
    if (!show_chrome || state->resizing) {
        flags |= ImGuiWindowFlags_NoMove;
    }
    if (!show_chrome) {
        flags |= ImGuiWindowFlags_NoTitleBar;
    }

    char title[64];
    std::snprintf(title, sizeof(title), "AImGui  v%s###aimgui_main", ImGui::GetVersion());

    if (ImGui::Begin(title, show_chrome ? keep_running : nullptr, flags)) {
        if (lt >= 0.999f && !state->collapsed) {
            state->last_full_pos  = ImGui::GetWindowPos();
            state->last_full_size = ImGui::GetWindowSize();
        }

        const float island_alpha = 1.0f - (lt < 0.30f ? lt / 0.30f : 1.0f);
        if (island_alpha > 0.01f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, island_alpha);
            DrawIslandContent();
            ImGui::PopStyleVar();

            if (!show_chrome && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                state->collapsed = false;
            }
        }

        const float full_alpha = lt > 0.70f ? (lt - 0.70f) / 0.30f : 0.0f;
        if (full_alpha > 0.01f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, full_alpha);

            static Page page = Page::Dashboard;
            DrawSidebar(page, keep_running, state);
            ImGui::SameLine(0, 0);
            DrawContent(state, page);

            ImGui::PopStyleVar();
        }
    }
    ImGui::End();

    ImGui::PopStyleVar();   // WindowRounding

    // Foreground overlays: ripples on every clickable widget.
    ripple::DrawAll();

    // On the click frame the chips still need to be advanced/drawn so the
    // visual is continuous with the next frame, but the UI under them is
    // still the real one (so the user perceives the surface itself
    // shattering). After this frame the early-return path takes over.
    if (state->exit_anim_active && state->exit_anim_first_frame) {
        const float now     = (float)ImGui::GetTime();
        const float t01     = (now - state->exit_anim_start) / 1.2f;
        const float clamped = t01 < 0.0f ? 0.0f : (t01 > 1.0f ? 1.0f : t01);
        shatter::Step(dt, clamped,
                      (ImTextureID)(uintptr_t)state->scene_snapshot_id);
        state->exit_anim_first_frame = false;
    }
}

} // namespace aimgui
