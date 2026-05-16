#include "ui/ui.h"

#include "core/ime_bridge.h"
#include "imgui.h"

#include <cmath>
#include <cstdio>

namespace aimgui {

namespace {

// ─── Navigation pages ────────────────────────────────────────────────────
enum class Page { Dashboard, Widgets, Window, Performance, About };

struct PageItem { Page id; const char* label; };
constexpr PageItem kPages[] = {
    { Page::Dashboard,   "Dashboard"   },
    { Page::Widgets,     "Widgets"     },
    { Page::Window,      "Window"      },
    { Page::Performance, "Performance" },
    { Page::About,       "About"       },
};

constexpr int kFpsPresets[] = { 0, 30, 60, 90, 120, 144 };
constexpr const char* kFpsLabels =
    "vsync\0" "30\0" "60\0" "90\0" "120\0" "144\0";

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
}

void KV(const char* key, const char* fmt, ...) {
    ImGui::TextDisabled("%s", key);
    ImGui::SameLine(180.0f);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

// ─── Page contents ───────────────────────────────────────────────────────
void DrawDashboard(const UiState* state) {
    ImGui::SeparatorText("Overview");

    KV("renderer",       "%s", state->renderer_name ? state->renderer_name : "?");
    KV("ImGui",          "%s", ImGui::GetVersion());
    KV("frame",          "%.1f FPS  (%.2f ms)",
                          ImGui::GetIO().Framerate,
                          1000.0f / ImGui::GetIO().Framerate);
    KV("anti-recording", "%s", state->permeate_record ? "on" : "off");

    ImGui::Spacing();
    ImGui::SeparatorText("Tips");
    ImGui::TextWrapped("Press a volume button to fold the window into a "
                       "Dynamic Island at the top of the screen; tap the "
                       "island or press a volume button again to expand it back.");
}

void DrawWidgets() {
    ImGui::SeparatorText("Basic widgets");

    static int    counter = 0;
    static float  slider  = 0.5f;
    static bool   toggle  = false;
    static ImVec4 tint(0.40f, 0.70f, 1.00f, 1.0f);

    if (ImGui::Button("tap me")) counter++;
    ImGui::SameLine();
    ImGui::Text("count = %d", counter);

    static char text[128] = "tap me, then a soft keyboard pops up";
    ImGui::InputText("text", text, IM_ARRAYSIZE(text));
    ImGui::TextDisabled("IME bridge: %s",
                        ime::IsRunning() ? "ready (Java helper attached)"
                                         : "off (push AImGui.dex next to the ELF)");

    ImGui::SliderFloat("slider", &slider, 0.0f, 1.0f, "%.3f");
    ImGui::Checkbox("toggle",  &toggle);
    ImGui::ColorEdit4("color", (float*)&tint);

    ImGui::Spacing();
    ImGui::SeparatorText("List");
    static int selected = -1;
    for (int i = 0; i < 4; ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "item %d", i);
        if (ImGui::Selectable(label, selected == i)) selected = i;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Progress");
    static float progress = 0.0f;
    progress += ImGui::GetIO().DeltaTime * 0.15f;
    if (progress > 1.0f) progress -= 1.0f;
    ImGui::ProgressBar(progress, ImVec2(-1, 0));
}

void DrawWindow(UiState* state) {
    ImGui::SeparatorText("Surface");

    bool perm = state->permeate_record;
    if (ImGui::Checkbox("anti-recording (hide from capture / cast)", &perm)) {
        state->request_permeate_toggle = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", state->permeate_record ? "on" : "off");

    ImGui::TextWrapped(
        "When on, the SurfaceFlinger layer is created with the secure / "
        "skipScreenshot flag, so screen recordings and casts won't see it.");

    ImGui::Spacing();
    ImGui::SeparatorText("Theme");
    static int theme = 0;
    if (ImGui::Combo("##theme", &theme, "dark\0light\0classic\0")) {
        switch (theme) {
            case 0: ImGui::StyleColorsDark();    break;
            case 1: ImGui::StyleColorsLight();   break;
            case 2: ImGui::StyleColorsClassic(); break;
        }
    }
}

void DrawPerformance(UiState* state) {
    ImGui::SeparatorText("Frame-rate cap");

    int fps_idx = FpsToIndex(state->target_fps);
    if (ImGui::Combo("target", &fps_idx, kFpsLabels)) {
        state->target_fps = kFpsPresets[fps_idx];
    }

    ImGui::Spacing();
    KV("current",    "%.1f FPS", ImGui::GetIO().Framerate);
    KV("frame time", "%.2f ms",  1000.0f / ImGui::GetIO().Framerate);

    ImGui::Spacing();
    ImGui::SeparatorText("Rolling FPS");

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
    ImGui::TextWrapped("A minimal Dear ImGui Android ARM64 ELF — no JNI, no APK, "
                       "no Activity. Runs as a native binary on top of "
                       "SurfaceFlinger.");

    ImGui::Spacing();
    ImGui::SeparatorText("Features");
    ImGui::BulletText("Dear ImGui %s", ImGui::GetVersion());
    ImGui::BulletText("Vulkan + OpenGL ES 3 auto-fallback");
    ImGui::BulletText("VSync-locked frame pacer, near-zero CPU spin");
    ImGui::BulletText("Observe-only touch (no /dev/uinput, system unaffected)");
    ImGui::BulletText("Anti-recording surface toggle");
    ImGui::BulletText("Volume-key Dynamic Island fold");
}

// ─── Sidebar ─────────────────────────────────────────────────────────────
void DrawSidebar(Page& current, bool* keep_running, const UiState* state) {
    constexpr float kInnerPadX     = 18.0f;
    constexpr float kInnerPadY     = 14.0f;
    constexpr float kSelectableH   = 42.0f;
    constexpr float kAccentInset   = 10.0f;
    constexpr float kAccentW       = 4.0f;
    constexpr float kFooterH       = 110.0f;
    constexpr float kBottomMargin  = 16.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg,       ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.18f, 0.32f, 0.58f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.28f, 0.35f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.22f, 0.40f, 0.78f, 0.85f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,       ImVec2(kInnerPadX, kInnerPadY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,         ImVec2(0, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,        ImVec2(0, 6));

    ImGui::BeginChild("##sidebar", ImVec2(230, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);

    // Nav items, accent on the active one.
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

    // Footer pinned at the bottom, with breathing room before the screen edge.
    float remaining = ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - kFooterH - kBottomMargin;
    if (remaining > 0) ImGui::Dummy(ImVec2(0, remaining));

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", state->renderer_name ? state->renderer_name : "?");
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 12));
    if (ImGui::Button("exit", ImVec2(-1, 0))) *keep_running = false;
    ImGui::PopStyleVar();

    // Extra bottom margin so the button doesn't kiss the screen edge.
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

// Critically-ish damped spring: pos chases target, mild overshoot for the
// "sticky / 灵动" feel.
void UpdateSpring(float* pos, float* vel, float target, float dt) {
    constexpr float kOmega = 12.0f;   // natural frequency
    constexpr float kZeta  = 0.82f;   // damping ratio (<1 = bouncy)
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

    // ── Spring-driven expand/collapse animation ──────────────────────
    const float target = state->collapsed ? 0.0f : 1.0f;
    UpdateSpring(&state->expand, &state->expand_vel, target, dt);
    const float t = state->expand;
    const float lt = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);  // clamped for layout

    // ── Geometry: island top-center, lerping to/from full window ─────
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

    // Animation in flight: pin position / size every frame. Fully
    // expanded: let ImGui keep the user's drag position.
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

    // Pill ↔ rounded rectangle: rounding lerps with the morph.
    const float rounding = (kIslandH * 0.5f) * (1.0f - lt) + 12.0f * lt;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (!show_chrome) {
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    }

    char title[64];
    std::snprintf(title, sizeof(title), "AImGui  v%s###aimgui_main", ImGui::GetVersion());

    if (ImGui::Begin(title, show_chrome ? keep_running : nullptr, flags)) {
        // Remember the user's drag position whenever we're fully expanded.
        if (lt >= 0.999f && !state->collapsed) {
            state->last_full_pos  = ImGui::GetWindowPos();
            state->last_full_size = ImGui::GetWindowSize();
        }

        // ── Island content (FPS centered, fades out as we expand) ─
        const float island_alpha = 1.0f - (lt < 0.30f ? lt / 0.30f : 1.0f);
        if (island_alpha > 0.01f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, island_alpha);
            DrawIslandContent();
            ImGui::PopStyleVar();

            // Tap the pill to expand back.
            if (!show_chrome && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                state->collapsed = false;
            }
        }

        // ── Full UI (fades in late in the morph) ──────────────────
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
