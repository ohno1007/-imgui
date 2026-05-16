#pragma once

#include <android/native_window.h>

namespace aimgui {

class Renderer {
public:
    bool Init(ANativeWindow* window, int width, int height);
    void NewFrame();
    void EndFrame();
    void Shutdown();

    const char* Name() const { return "OpenGL ES 3"; }

private:
    ANativeWindow* m_Window = nullptr;
    void* m_Display = nullptr;
    void* m_Surface = nullptr;
    void* m_Context = nullptr;
    int m_Width = 0;
    int m_Height = 0;
};

} // namespace aimgui
