#include "renderer.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

namespace aimgui {

bool Renderer::Init(ANativeWindow* window, int width, int height) {
    m_Window = window;
    m_Width = width;
    m_Height = height;

    const EGLint cfg_attribs[] = {
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE,
    };
    const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(dpy, nullptr, nullptr)) return false;

    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &n) || n < 1) return false;

    EGLint visual = 0;
    eglGetConfigAttrib(dpy, cfg, EGL_NATIVE_VISUAL_ID, &visual);
    ANativeWindow_setBuffersGeometry(window, 0, 0, visual);

    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
    EGLSurface sfc = eglCreateWindowSurface(dpy, cfg, window, nullptr);
    if (ctx == EGL_NO_CONTEXT || sfc == EGL_NO_SURFACE) return false;
    if (!eglMakeCurrent(dpy, sfc, sfc, ctx)) return false;

    m_Display = dpy;
    m_Surface = sfc;
    m_Context = ctx;

    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 0.f);

    ImGui_ImplOpenGL3_Init("#version 300 es");
    return true;
}

void Renderer::NewFrame() {
    ImGui_ImplOpenGL3_NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)m_Width, (float)m_Height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void Renderer::EndFrame() {
    ImGui::Render();
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers((EGLDisplay)m_Display, (EGLSurface)m_Surface);
}

void Renderer::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    if (m_Display) {
        eglMakeCurrent((EGLDisplay)m_Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_Context) eglDestroyContext((EGLDisplay)m_Display, (EGLContext)m_Context);
        if (m_Surface) eglDestroySurface((EGLDisplay)m_Display, (EGLSurface)m_Surface);
        eglTerminate((EGLDisplay)m_Display);
    }
    m_Display = m_Surface = m_Context = nullptr;
}

} // namespace aimgui
