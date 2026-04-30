#include "Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg/nanovg.h>
#include <nanovg/nanovg_gl.h>
#include <cstdio>

bool Renderer::init(GLFWwindow* window) {
    m_window = window;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "[WavesUI] Failed to initialize GLAD\n");
        return false;
    }

    // Get framebuffer size for DPI scaling
    int fbW, fbH, winW, winH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glfwGetWindowSize(window, &winW, &winH);
    m_dpiScale = (winW > 0) ? (float)fbW / (float)winW : 1.f;

    // Create NanoVG context with OpenGL3 backend
    m_vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!m_vg) {
        fprintf(stderr, "[WavesUI] Failed to create NanoVG context\n");
        return false;
    }

    printf("[WavesUI] Renderer initialized (DPI scale: %.2f)\n", m_dpiScale);
    return true;
}

void Renderer::shutdown() {
    if (m_vg) {
        nvgDeleteGL3(m_vg);
        m_vg = nullptr;
    }
}

int Renderer::loadFont(const std::string& name, const std::string& path) {
    if (!m_vg) return -1;
    int id = nvgCreateFont(m_vg, name.c_str(), path.c_str());
    if (id < 0) {
        fprintf(stderr, "[WavesUI] Failed to load font: %s\n", path.c_str());
    }
    return id;
}

int Renderer::loadImage(const std::string& path) {
    if (!m_vg) return -1;
    int id = nvgCreateImage(m_vg, path.c_str(), 0);
    if (id < 0) {
        fprintf(stderr, "[WavesUI] Failed to load image: %s\n", path.c_str());
    }
    return id;
}

void Renderer::resolveStyles(Widget* w, const CSSParser& css) {
    w->resolveStyle(css);
    for (auto& c : w->children())
        resolveStyles(c.get(), css);
}

void Renderer::frame(Widget::Ptr root, const CSSParser& css, float dt) {
    if (!m_vg || !root) return;

    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    m_vpW = (float)fbW / m_dpiScale;
    m_vpH = (float)fbH / m_dpiScale;

    // OpenGL clear
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Resolve CSS styles for entire tree
    resolveStyles(root.get(), css);

    // Layout pass
    m_layout.layout(root, m_vpW, m_vpH);

    // NanoVG frame
    nvgBeginFrame(m_vg, m_vpW, m_vpH, m_dpiScale);

    // Render widget tree
    root->render(m_vg, 1.f);

    nvgEndFrame(m_vg);
}