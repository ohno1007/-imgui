// AImGui: a minimal Dear ImGui Android ARM64 ELF.
#include "app.h"
#include "core/renderer.h"
#include "imgui.h"
#include "platform/ANativeWindowCreator.h"
#include "platform/TouchHelperA.h"

#include <chrono>
#include <thread>

int main() {
    using namespace android;
    using aimgui::Renderer;

    auto info = ANativeWindowCreator::GetDisplayInfo();
    const int W = info.width  > info.height ? info.width  : info.height;
    const int H = info.width  > info.height ? info.height : info.width;

    ANativeWindow* window = ANativeWindowCreator::Create("AImGui", W, W, false);
    if (!window) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    aimgui::AppInit();

    Renderer renderer;
    if (!renderer.Init(window, W, W)) {
        ImGui::DestroyContext();
        ANativeWindowCreator::Destroy(window);
        return 2;
    }

    Touch::Init({(float)W, (float)H}, false);
    Touch::setOrientation((int)info.orientation);

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    bool running = true;
    uint32_t cached_orientation = info.orientation;
    while (running) {
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        if (dt < 1.0f / 120.0f) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }
        last = now;
        ImGui::GetIO().DeltaTime = dt > 0.f ? dt : 1.0f / 60.0f;

        info = ANativeWindowCreator::GetDisplayInfo();
        if (info.orientation != cached_orientation) {
            cached_orientation = info.orientation;
            Touch::setOrientation((int)info.orientation);
        }

        ANativeWindowCreator::ProcessMirrorDisplay();

        renderer.NewFrame();
        ImGui::NewFrame();
        aimgui::AppFrame(&running);
        renderer.EndFrame();
    }

    renderer.Shutdown();
    ImGui::DestroyContext();
    ANativeWindowCreator::Destroy(window);
    return 0;
}
