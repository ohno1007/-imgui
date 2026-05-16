#pragma once

#include <GLES3/gl3.h>

namespace aimgui {

// Tiny post-process bloom for the OpenGL ES 3 backend.
//
// Usage in IRenderer::EndFrame:
//
//   if (bloom.Ready()) {
//       bloom.BeginScene();                                // bind scene FBO
//       ImGui_ImplOpenGL3_RenderDrawData(...);
//       bloom.EndSceneAndComposite();                      // threshold + blur + composite to default FB
//   } else {
//       glClear(GL_COLOR_BUFFER_BIT);
//       ImGui_ImplOpenGL3_RenderDrawData(...);
//   }
//
// The composite preserves the scene's alpha channel so the SurfaceFlinger
// overlay still shows through where ImGui didn't draw.
class BloomGL {
public:
    bool Init(int width, int height);
    void Shutdown();
    bool Ready() const { return m_Ready; }

    void BeginScene();
    void EndSceneAndComposite();

private:
    bool m_Ready  = false;
    int  m_Width  = 0;
    int  m_Height = 0;
    int  m_BlurW  = 0;
    int  m_BlurH  = 0;

    GLuint m_SceneFBO   = 0;
    GLuint m_SceneTex   = 0;
    GLuint m_BlurFBO[2] = { 0, 0 };
    GLuint m_BlurTex[2] = { 0, 0 };

    GLuint m_QuadVAO = 0;
    GLuint m_QuadVBO = 0;

    GLuint m_ProgThreshold = 0;
    GLuint m_ProgBlur      = 0;
    GLuint m_ProgComposite = 0;

    GLint  m_LocThreshScene = -1;
    GLint  m_LocBlurImage   = -1;
    GLint  m_LocBlurDir     = -1;
    GLint  m_LocCompScene   = -1;
    GLint  m_LocCompBloom   = -1;
    GLint  m_LocCompIntens  = -1;
};

} // namespace aimgui
