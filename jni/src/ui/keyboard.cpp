#include "ui/keyboard.h"

#include "imgui.h"
#include "imgui_internal.h"   // for ActivateItemByID

#include <cstdio>

namespace aimgui::kbd {

namespace {

// Last InputText/InputTextMultiline ID seen as active. Used to re-activate
// the field after a virtual key tap (which would otherwise steal focus).
ImGuiID g_target_id = 0;

// Frames remaining to keep the keyboard visible after io.WantTextInput goes
// false. Gives us a window where the tap-induced focus loss is healed by
// ActivateItemByID() before the keyboard disappears.
int g_grace = 0;

bool g_shift     = false;
bool g_symbols   = false;

void Refocus() {
    if (g_target_id) ImGui::ActivateItemByID(g_target_id);
}

void EmitChar(unsigned int c) {
    ImGui::GetIO().AddInputCharacter(c);
    if (g_shift) g_shift = false;
    Refocus();
}

void EmitKey(ImGuiKey k) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(k, true);
    io.AddKeyEvent(k, false);
    Refocus();
}

bool DrawKeyButton(const char* label, float w, float h, bool toggled = false) {
    if (toggled) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    bool clicked = ImGui::Button(label, ImVec2(w, h));
    if (toggled) ImGui::PopStyleColor();
    return clicked;
}

void DrawLettersRow(const char* row, float kw, float kh, float left_pad = 0) {
    if (left_pad > 0) { ImGui::Dummy(ImVec2(left_pad, 0)); ImGui::SameLine(); }
    for (int i = 0; row[i]; ++i) {
        if (i) ImGui::SameLine();
        char ch  = g_shift ? (char)(row[i] - 32) : row[i];
        char buf[2] = { ch, 0 };
        if (DrawKeyButton(buf, kw, kh)) EmitChar((unsigned int)ch);
    }
}

void DrawCharRow(const char* row, float kw, float kh, float left_pad = 0) {
    if (left_pad > 0) { ImGui::Dummy(ImVec2(left_pad, 0)); ImGui::SameLine(); }
    for (int i = 0; row[i]; ++i) {
        if (i) ImGui::SameLine();
        char buf[2] = { row[i], 0 };
        if (DrawKeyButton(buf, kw, kh)) EmitChar((unsigned int)row[i]);
    }
}

void Dismiss() {
    g_target_id = 0;
    g_grace     = 0;
    g_shift     = false;
    g_symbols   = false;
}

} // namespace

void TrackInputItem() {
    if (ImGui::IsItemActive() || ImGui::IsItemActivated()) {
        g_target_id = ImGui::GetItemID();
    }
}

void Draw() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)        g_grace = 6;          // keep alive while typing
    else if (g_grace > 0)        --g_grace;
    if (g_grace <= 0) { g_target_id = 0; return; }

    // ── geometry: 10 keys per row, square-ish, snap to display width ──
    constexpr float kPad      = 16.0f;
    constexpr float kKeyGap   = 6.0f;
    constexpr int   kKeysWide = 10;
    const float total_w = io.DisplaySize.x;
    const float kw = (total_w - 2 * kPad - (kKeysWide - 1) * kKeyGap) / (float)kKeysWide;
    const float kh = kw * 0.78f;
    const float kbd_h = 4 * kh + 3 * kKeyGap + 2 * kPad;

    ImGui::SetNextWindowPos (ImVec2(0, io.DisplaySize.y - kbd_h));
    ImGui::SetNextWindowSize(ImVec2(total_w, kbd_h));
    ImGui::SetNextWindowBgAlpha(0.94f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(kPad, kPad));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(kKeyGap, kKeyGap));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     ImVec2(0, 0));

    ImGui::Begin("##kbd", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!g_symbols) {
        DrawLettersRow("qwertyuiop", kw, kh);
        DrawLettersRow("asdfghjkl",  kw, kh, kw * 0.5f);

        if (DrawKeyButton(g_shift ? "SHIFT" : "shift", kw * 1.5f, kh, g_shift)) {
            g_shift = !g_shift;
            Refocus();
        }
        ImGui::SameLine();
        DrawLettersRow("zxcvbnm", kw, kh);
        ImGui::SameLine();
        if (DrawKeyButton("<-", kw * 1.5f, kh)) EmitKey(ImGuiKey_Backspace);

        if (DrawKeyButton("123",   kw * 1.5f, kh)) { g_symbols = true; Refocus(); }
        ImGui::SameLine();
        if (DrawKeyButton("space", kw * 5.0f + 4 * kKeyGap, kh)) EmitChar(' ');
        ImGui::SameLine();
        if (DrawKeyButton("enter", kw * 1.5f, kh)) EmitKey(ImGuiKey_Enter);
        ImGui::SameLine();
        if (DrawKeyButton("x",     kw, kh)) Dismiss();
    } else {
        DrawCharRow("1234567890", kw, kh);
        DrawCharRow("-/:;()$&@\"", kw, kh);

        if (DrawKeyButton("#+=", kw * 1.5f, kh)) { /* future second symbol page */ }
        ImGui::SameLine();
        DrawCharRow(".,?!'", kw, kh);
        ImGui::SameLine();
        if (DrawKeyButton("<-", kw * 1.5f, kh)) EmitKey(ImGuiKey_Backspace);

        if (DrawKeyButton("abc",   kw * 1.5f, kh)) { g_symbols = false; Refocus(); }
        ImGui::SameLine();
        if (DrawKeyButton("space", kw * 5.0f + 4 * kKeyGap, kh)) EmitChar(' ');
        ImGui::SameLine();
        if (DrawKeyButton("enter", kw * 1.5f, kh)) EmitKey(ImGuiKey_Enter);
        ImGui::SameLine();
        if (DrawKeyButton("x",     kw, kh)) Dismiss();
    }

    ImGui::End();
    ImGui::PopStyleVar(6);
}

} // namespace aimgui::kbd
