#pragma once
#include "Renderer.h"
#include "EventSystem.h"
#include "CSSParser.h"
#include "Widget.h"
#include <string>
#include <functional>

// ── WindowManager ─────────────────────────────────────────────────────────────
// One instance per OS window. Owns GLFW window + renderer + event system.

struct WindowConfig {
    std::string title   = "WavesUI";
    int         width   = 1280;
    int         height  = 720;
    bool        resizable    = true;
    bool        decorated    = true;  // false = borderless
    bool        transparent  = false; // framebuffer alpha (for glass)
    bool        vsync        = true;
    int         msaa         = 4;     // samples
};

class WindowManager {
public:
    ~WindowManager();

    bool create(const WindowConfig& cfg);
    void destroy();

    bool  isOpen() const;
    void  setTitle(const std::string& title);
    void  setSize(int w, int h);
    void  close();

    // Load CSS file to apply globally
    bool  loadCSS(const std::string& path);

    // Load a font (name used in CSS font-family)
    int   loadFont(const std::string& name, const std::string& path);

    // Root widget — add children to this
    Widget::Ptr root();

    // Set root directly (replaces widget tree)
    void setRoot(Widget::Ptr w);

    // Find a widget by id anywhere in the tree
    Widget::Ptr findById(const std::string& id);

    // Run one frame — returns false when window should close
    bool tick();

    // Block until window closes (simple run loop)
    void run();

    // Waves callback: called at end of each frame (for Waves-side animation)
    std::string onFrameCallback; // name of Waves callback

    Renderer&    renderer()    { return m_renderer; }
    CSSParser&   cssParser()   { return m_css; }
    EventSystem& eventSystem() { return m_events; }

private:
    GLFWwindow*  m_window   = nullptr;
    Renderer     m_renderer;
    CSSParser    m_css;
    EventSystem  m_events;
    Widget::Ptr  m_root;
    double       m_lastTime = 0.0;

    static void glfwErrorCallback(int code, const char* desc);
};

// Global registry (supports multiple windows, but usually just one)
extern WindowManager* g_activeWindow;