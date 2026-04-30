#include "WindowManager.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>

WindowManager* g_activeWindow = nullptr;

void WindowManager::glfwErrorCallback(int code, const char* desc) {
    fprintf(stderr, "[WavesUI] GLFW error %d: %s\n", code, desc);
}

bool WindowManager::create(const WindowConfig& cfg) {
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        fprintf(stderr, "[WavesUI] Failed to initialize GLFW\n");
        return false;
    }

    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    glfwWindowHint(GLFW_RESIZABLE,   cfg.resizable  ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED,   cfg.decorated  ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES,     cfg.msaa);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    // Transparent framebuffer for glass effects
    if (cfg.transparent) {
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    }

    m_window = glfwCreateWindow(cfg.width, cfg.height, cfg.title.c_str(), nullptr, nullptr);
    if (!m_window) {
        fprintf(stderr, "[WavesUI] Failed to create window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(cfg.vsync ? 1 : 0);

    // Init renderer
    if (!m_renderer.init(m_window)) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Create root widget
    m_root = std::make_shared<DivWidget>();
    m_root->id = "__root__";
    m_root->computedStyle.display = "flex";
    m_root->computedStyle.flexDirection = "column";

    // Init event system
    m_events.init(m_window);
    g_rootWidget = m_root;

    g_activeWindow  = this;
    m_lastTime      = glfwGetTime();

    printf("[WavesUI] Window '%s' created (%dx%d)\n", cfg.title.c_str(), cfg.width, cfg.height);
    return true;
}

WindowManager::~WindowManager() {
    destroy();
}

void WindowManager::destroy() {
    m_events.shutdown();
    m_renderer.shutdown();
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    if (g_activeWindow == this) g_activeWindow = nullptr;
}

bool WindowManager::isOpen() const {
    return m_window && !glfwWindowShouldClose(m_window);
}

void WindowManager::setTitle(const std::string& title) {
    if (m_window) glfwSetWindowTitle(m_window, title.c_str());
}

void WindowManager::setSize(int w, int h) {
    if (m_window) glfwSetWindowSize(m_window, w, h);
}

void WindowManager::close() {
    if (m_window) glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

bool WindowManager::loadCSS(const std::string& path) {
    return m_css.parseFile(path);
}

int WindowManager::loadFont(const std::string& name, const std::string& path) {
    return m_renderer.loadFont(name, path);
}

Widget::Ptr WindowManager::root() { return m_root; }

void WindowManager::setRoot(Widget::Ptr w) {
    m_root      = w;
    g_rootWidget = w;
}

Widget::Ptr WindowManager::findById(const std::string& id) {
    return m_root ? m_root->findById(id) : nullptr;
}

bool WindowManager::tick() {
    if (!isOpen()) return false;

    double now = glfwGetTime();
    float  dt  = (float)(now - m_lastTime);
    m_lastTime = now;
    dt = std::min(dt, 0.1f); // clamp to avoid spiral of death

    glfwPollEvents();

    m_events.tick(m_root, dt);

    // Fire per-frame Waves callback
    if (!onFrameCallback.empty() && waves_callbacks && waves_callbacks->has(onFrameCallback.c_str())) {
        WavesValue dtVal = waves_makeFloat(dt);
        waves_callbacks->invoke(onFrameCallback.c_str(), 1, &dtVal);
    }

    m_renderer.frame(m_root, m_css, dt);

    glfwSwapBuffers(m_window);
    return true;
}

void WindowManager::run() {
    while (tick()) {}
}