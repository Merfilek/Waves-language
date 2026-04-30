#pragma once
#include "Widget.h"
#include "../WavesBridge.h"
#include <functional>

struct GLFWwindow;

// ── Event System ──────────────────────────────────────────────────────────────
// Translates GLFW callbacks into UIEvents, hit-tests the widget tree,
// and invokes registered Waves callbacks.

class EventSystem {
public:
    void init(GLFWwindow* window);
    void shutdown();

    // Called every frame — handles hover state, cursor blinking, transitions
    void tick(Widget::Ptr root, float dt);

    // Hit test the tree and fire events
    void injectMouseMove (Widget::Ptr root, float x, float y);
    void injectMouseDown (Widget::Ptr root, float x, float y, int button);
    void injectMouseUp   (Widget::Ptr root, float x, float y, int button);
    void injectScroll    (Widget::Ptr root, float dx, float dy);
    void injectKeyDown   (Widget::Ptr root, int key, int mods);
    void injectTextInput (Widget::Ptr root, unsigned int codepoint);

    // Currently focused widget
    Widget* focusedWidget = nullptr;

    // Mouse state
    float mouseX = 0, mouseY = 0;
    bool  mouseDown = false;

private:
    GLFWwindow* m_window = nullptr;

    // Walk tree, return topmost widget under (x,y)
    Widget* hitTest(Widget* w, float x, float y);

    // Fire a Waves callback for a widget event
    void fireCallback(Widget* w, const std::string& eventName, const UIEvent& ev);

    // Build a WavesValue object from UIEvent
    WavesValue makeEventObject(const UIEvent& ev);
};

// Global instance (accessed from GLFW static callbacks)
extern EventSystem* g_eventSystem;
extern Widget::Ptr  g_rootWidget;