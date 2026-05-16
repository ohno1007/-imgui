#include "ui/ui.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace aimgui {

namespace {

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
    s.ScrollbarSize           = 18.0f;
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
    ImGui::SameLine();
    ImGui::Text(u8"计数 = %d", counter);

    SliderFloatGrabValue(u8"滑块", &slider, 0.0f, 1.0f, "%.3f");
    ImGui::Checkbox  (u8"开关",   &toggle);
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
}

void DrawPerformance(UiState* state) {
    ImGui::SeparatorText(u8"帧率限制");

    int fps_idx = FpsToIndex(state->target_fps);
    if (ImGui::Combo(u8"目标帧率", &fps_idx, kFpsLabels)) {
        state->target_fps = kFpsPresets[fps_idx];
    }

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
void DrawSidebar(Page& current, bool* keep_running, const UiState* state) {
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
    if (ImGui::Button(u8"退出", ImVec2(-1, 0))) *keep_running = false;
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, kBottomMargin));

    ImGui::EndChild();

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(4);
}

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
        ImGui::SetNextWindowSize(state->last_full_size, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(700, 560), ImVec2(FLT_MAX, FLT_MAX));
    }

    const float rounding = (kIslandH * 0.5f) * (1.0f - lt) + 12.0f * lt;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (!show_chrome) {
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
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
}

} // namespace aimgui
