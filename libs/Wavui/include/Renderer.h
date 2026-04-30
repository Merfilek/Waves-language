#pragma once
#include "Widget.h"
#include "Layout.h"
#include "CSSParser.h"

struct NVGcontext;
struct GLFWwindow;

// ── Renderer ──────────────────────────────────────────────────────────────────
// Owns the NanoVG context. Each frame: clear → layout → render tree.

class Renderer {
public:
    bool init(GLFWwindow* window);
    void shutdown();

    // Full frame: layout + draw
    void frame(Widget::Ptr root, const CSSParser& css, float dt);

    // Load a font from file, returns font id
    int  loadFont(const std::string& name, const std::string& path);

    // Load an image from file, returns nvg image handle
    int  loadImage(const std::string& path);

    NVGcontext* nvg() const { return m_vg; }

    float viewportW() const { return m_vpW; }
    float viewportH() const { return m_vpH; }

private:
    NVGcontext* m_vg     = nullptr;
    GLFWwindow* m_window = nullptr;
    LayoutEngine m_layout;

    float m_vpW = 0, m_vpH = 0;
    float m_dpiScale = 1.f;

    void resolveStyles(Widget* w, const CSSParser& css);
};