#include "EventSystem.h"
#include "../WavesBridge.h"
#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdio>
#include <unordered_set>

EventSystem* g_eventSystem = nullptr;
Widget::Ptr  g_rootWidget  = nullptr;

// ── GLFW static callbacks ─────────────────────────────────────────────────────

static void cb_mouseMove(GLFWwindow*, double x, double y) {
    if (g_eventSystem && g_rootWidget)
        g_eventSystem->injectMouseMove(g_rootWidget, (float)x, (float)y);
}

static void cb_mouseButton(GLFWwindow*, int button, int action, int) {
    if (!g_eventSystem || !g_rootWidget) return;
    if (action == GLFW_PRESS)
        g_eventSystem->injectMouseDown(g_rootWidget, g_eventSystem->mouseX, g_eventSystem->mouseY, button);
    else if (action == GLFW_RELEASE)
        g_eventSystem->injectMouseUp(g_rootWidget, g_eventSystem->mouseX, g_eventSystem->mouseY, button);
}

static void cb_scroll(GLFWwindow*, double dx, double dy) {
    if (g_eventSystem && g_rootWidget)
        g_eventSystem->injectScroll(g_rootWidget, (float)dx, (float)dy);
}

static void cb_key(GLFWwindow*, int key, int, int action, int mods) {
    if (!g_eventSystem || !g_rootWidget) return;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        g_eventSystem->injectKeyDown(g_rootWidget, key, mods);
}

static void cb_char(GLFWwindow*, unsigned int codepoint) {
    if (g_eventSystem && g_rootWidget)
        g_eventSystem->injectTextInput(g_rootWidget, codepoint);
}

// ── Init / shutdown ───────────────────────────────────────────────────────────

void EventSystem::init(GLFWwindow* window) {
    m_window = window;
    g_eventSystem = this;

    glfwSetCursorPosCallback  (window, cb_mouseMove);
    glfwSetMouseButtonCallback(window, cb_mouseButton);
    glfwSetScrollCallback     (window, cb_scroll);
    glfwSetKeyCallback        (window, cb_key);
    glfwSetCharCallback       (window, cb_char);
}

void EventSystem::shutdown() {
    if (m_window) {
        glfwSetCursorPosCallback  (m_window, nullptr);
        glfwSetMouseButtonCallback(m_window, nullptr);
        glfwSetScrollCallback     (m_window, nullptr);
        glfwSetKeyCallback        (m_window, nullptr);
        glfwSetCharCallback       (m_window, nullptr);
    }
    g_eventSystem = nullptr;
    m_window = nullptr;
}

// ── Hit testing ───────────────────────────────────────────────────────────────

Widget* EventSystem::hitTest(Widget* w, float x, float y) {
    if (!w->visible) return nullptr;

    // Test children in reverse (last drawn = top)
    for (int i = (int)w->children().size()-1; i >= 0; i--) {
        auto* hit = hitTest(w->children()[i].get(), x, y);
        if (hit) return hit;
    }

    // Test self (root __root__ shouldn't consume hits if it's the viewport)
    if (w->id == "__root__") return nullptr;

    if (x >= w->box.x && x <= w->box.x + w->box.w &&
        y >= w->box.y && y <= w->box.y + w->box.h) {
        return w;
    }
    return nullptr;
}

// ── Waves event object ────────────────────────────────────────────────────────

WavesValue EventSystem::makeEventObject(const UIEvent& ev) {
    if (!waves_object_create) return waves_makeNull();

    WavesValue obj = waves_object_create("UIEvent");
    waves_object_set_field(obj, "mouseX",  waves_makeFloat(ev.mouseX));
    waves_object_set_field(obj, "mouseY",  waves_makeFloat(ev.mouseY));
    waves_object_set_field(obj, "key",     waves_makeInt(ev.key));
    waves_object_set_field(obj, "text",    waves_makeString(ev.text.c_str()));
    waves_object_set_field(obj, "scrollX", waves_makeFloat(ev.scrollX));
    waves_object_set_field(obj, "scrollY", waves_makeFloat(ev.scrollY));
    return obj;
}

void EventSystem::fireCallback(Widget* w, const std::string& eventName, const UIEvent& ev) {
    auto it = w->eventHandlers.find(eventName);
    if (it == w->eventHandlers.end()) return;
    const std::string& cbName = it->second;

    if (!waves_callbacks) {
        fprintf(stderr, "[Event] WARNING: waves_callbacks is null\n");
        return;
    }

    // Try the name as-is first, then with common prefixes the runtime might use
    const char* resolved = nullptr;
    std::string prefixed = "main." + cbName;

    if      (waves_callbacks->has(cbName.c_str()))      resolved = cbName.c_str();
    else if (waves_callbacks->has(prefixed.c_str()))     resolved = prefixed.c_str();

    if (!resolved) {
        // Log once per callback name to avoid spam
        static std::unordered_set<std::string> warned;
        if (warned.find(cbName) == warned.end()) {
            warned.insert(cbName);
            fprintf(stderr, "[Event] WARNING: callback '%s' not registered (tried '%s' and '%s')\n",
                    cbName.c_str(), cbName.c_str(), prefixed.c_str());
        }
        return;
    }

    WavesValue evObj = makeEventObject(ev);
    WavesValue argv[1] = { evObj };
    waves_callbacks->invoke(resolved, 1, argv);
}

// ── Mouse move ────────────────────────────────────────────────────────────────

void EventSystem::injectMouseMove(Widget::Ptr root, float x, float y) {
    mouseX = x; mouseY = y;

    std::function<void(Widget*)> updateHover = [&](Widget* w) {
        bool wasHovered = w->hovered;
        bool isHovered  = (x >= w->box.x && x <= w->box.x + w->box.w &&
                           y >= w->box.y && y <= w->box.y + w->box.h &&
                           w->visible && w->id != "__root__");
        w->hovered = isHovered;

        if (!wasHovered && isHovered) {
            UIEvent ev; ev.type=UIEventType::MouseEnter; ev.mouseX=x; ev.mouseY=y;
            fireCallback(w, "mouseenter", ev);
        } else if (wasHovered && !isHovered) {
            UIEvent ev; ev.type=UIEventType::MouseLeave; ev.mouseX=x; ev.mouseY=y;
            fireCallback(w, "mouseleave", ev);
        }

        for (auto& c : w->children()) updateHover(c.get());
    };
    updateHover(root.get());

    // Update cursor
    Widget* top = hitTest(root.get(), x, y);
    if (top && m_window) {
        std::string cursor = top->computedStyle.cursor;
        if      (cursor=="pointer") glfwSetCursor(m_window, glfwCreateStandardCursor(GLFW_POINTING_HAND_CURSOR));
        else if (cursor=="text")    glfwSetCursor(m_window, glfwCreateStandardCursor(GLFW_IBEAM_CURSOR));
        else                        glfwSetCursor(m_window, nullptr);
    }
}

// ── Mouse down ────────────────────────────────────────────────────────────────

void EventSystem::injectMouseDown(Widget::Ptr root, float x, float y, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    mouseDown = true;

    Widget* hit = hitTest(root.get(), x, y);

    // Blur old focus
    if (focusedWidget && focusedWidget != hit) {
        focusedWidget->focused = false;
        UIEvent ev; ev.type=UIEventType::Blur; ev.mouseX=x; ev.mouseY=y;
        fireCallback(focusedWidget, "blur", ev);
        focusedWidget = nullptr;
    }

    if (hit) {
        hit->active = true;

        if (hit->tag == "input") {
            hit->focused = true;
            focusedWidget = hit;
            UIEvent ev; ev.type=UIEventType::Focus; ev.mouseX=x; ev.mouseY=y;
            fireCallback(hit, "focus", ev);
        }

        UIEvent ev; ev.type=UIEventType::MouseDown; ev.mouseX=x; ev.mouseY=y;
        fireCallback(hit, "mousedown", ev);
    }
}

// ── Mouse up / click ──────────────────────────────────────────────────────────

void EventSystem::injectMouseUp(Widget::Ptr root, float x, float y, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    mouseDown = false;

    Widget* hit = hitTest(root.get(), x, y);
    if (hit) {
        // Fire click if this widget was pressed (active)
        if (hit->active) {
            UIEvent ev; ev.type=UIEventType::Click; ev.mouseX=x; ev.mouseY=y;
            fireCallback(hit, "click", ev);
        }
        UIEvent ev2; ev2.type=UIEventType::MouseUp; ev2.mouseX=x; ev2.mouseY=y;
        fireCallback(hit, "mouseup", ev2);
    }

    // Clear active on all widgets
    std::function<void(Widget*)> clearActive = [&](Widget* w) {
        w->active = false;
        for (auto& c : w->children()) clearActive(c.get());
    };
    clearActive(root.get());
}

// ── Scroll ────────────────────────────────────────────────────────────────────

void EventSystem::injectScroll(Widget::Ptr root, float dx, float dy) {
    Widget* hit = hitTest(root.get(), mouseX, mouseY);
    if (!hit) return;

    Widget* w = hit;
    while (w) {
        if (w->tag == "scroll") {
            auto* sw = static_cast<ScrollWidget*>(w);
            sw->scrollOffsetY = std::max(0.f,
                std::min(sw->contentHeight - sw->box.h, sw->scrollOffsetY - dy * 40.f));
            break;
        }
        w = w->parent();
    }

    UIEvent ev; ev.type=UIEventType::Scroll; ev.scrollX=dx; ev.scrollY=dy;
    fireCallback(hit, "scroll", ev);
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void EventSystem::injectKeyDown(Widget::Ptr root, int key, int mods) {
    if (!focusedWidget) return;

    UIEvent ev; ev.type=UIEventType::KeyDown; ev.key=key;
    fireCallback(focusedWidget, "keydown", ev);

    if (focusedWidget->tag == "input") {
        auto* inp = static_cast<InputWidget*>(focusedWidget);
        if (key == GLFW_KEY_BACKSPACE) {
            inp->deleteChar();
            UIEvent cev; cev.type=UIEventType::Change;
            fireCallback(focusedWidget, "change", cev);
        } else if (key == GLFW_KEY_LEFT && inp->cursorPos > 0) {
            inp->cursorPos--;
        } else if (key == GLFW_KEY_RIGHT && inp->cursorPos < (int)inp->value.size()) {
            inp->cursorPos++;
        } else if (key == GLFW_KEY_HOME) {
            inp->cursorPos = 0;
        } else if (key == GLFW_KEY_END) {
            inp->cursorPos = (int)inp->value.size();
        } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            UIEvent sev; sev.type=UIEventType::Change;
            fireCallback(focusedWidget, "submit", sev);
        }
    }
}

void EventSystem::injectTextInput(Widget::Ptr root, unsigned int codepoint) {
    if (!focusedWidget || focusedWidget->tag != "input") return;
    if (codepoint > 127) return; // ASCII only for now

    auto* inp = static_cast<InputWidget*>(focusedWidget);
    inp->insertChar((char)codepoint);

    UIEvent ev; ev.type=UIEventType::TextInput; ev.text=std::string(1,(char)codepoint);
    fireCallback(focusedWidget, "input", ev);

    UIEvent cev; cev.type=UIEventType::Change;
    fireCallback(focusedWidget, "change", cev);
}

// ── Tick ─────────────────────────────────────────────────────────────────────

void EventSystem::tick(Widget::Ptr root, float dt) {
    std::function<void(Widget*)> tickAll = [&](Widget* w) {
        if (w->tag == "input") {
            auto* inp = static_cast<InputWidget*>(w);
            inp->tick(dt);
        }
        for (auto& c : w->children()) tickAll(c.get());
    };
    tickAll(root.get());
}