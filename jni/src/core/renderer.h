#pragma once

#include <android/native_window.h>
#include <memory>

namespace aimgui {

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Init(ANativeWindow* window, int width, int height) = 0;
    virtual void NewFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Shutdown() = 0;
    virtual const char* Name() const = 0;
};

enum class Backend { Auto, Vulkan, OpenGL };

// Creates a renderer. With Backend::Auto it tries Vulkan first and falls back
// to OpenGL ES 3 if Vulkan is unavailable or fails to initialize.
// Returns nullptr if no backend works.
std::unique_ptr<IRenderer> MakeRenderer(ANativeWindow* window,
                                        int width, int height,
                                        Backend preferred = Backend::Auto);

// Factories for the individual backends (mostly for internal use).
std::unique_ptr<IRenderer> MakeGLRenderer();
std::unique_ptr<IRenderer> MakeVKRenderer();

} // namespace aimgui
