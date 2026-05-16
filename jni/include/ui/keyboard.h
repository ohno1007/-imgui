#pragma once

// On-screen QWERTY keyboard for native ELF builds, where no Android IME is
// available to feed key events into ImGui::InputText. Usage:
//
//   ImGui::InputText("text", buf, sizeof(buf));
//   aimgui::kbd::TrackInputItem();          // call right after each InputText
//
//   // ... at the end of your UI:
//   aimgui::kbd::Draw();
//
// When any tracked InputText is active (io.WantTextInput == true), Draw()
// renders a soft keyboard pinned to the bottom of the screen. Tapping a key
// queues the character via ImGui::GetIO().AddInputCharacter() and re-arms
// the tracked InputText so it stays the active widget across frames.

namespace aimgui::kbd {

void TrackInputItem();
void Draw();

} // namespace aimgui::kbd
