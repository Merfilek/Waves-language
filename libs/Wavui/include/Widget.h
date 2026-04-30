#pragma once
#include "CSSParser.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

struct NVGcontext;

// ── Computed layout box ───────────────────────────────────────────────────────
struct LayoutBox {
    float x = 0, y = 0, w = 0, h = 0;
};

// ── Event types passed to Waves callbacks ─────────────────────────────────────
enum class UIEventType {
    Click, MouseEnter, MouseLeave, MouseDown, MouseUp,
    KeyDown, KeyUp, TextInput, Focus, Blur, Change, Scroll
};

struct UIEvent {
    UIEventType type;
    float       mouseX = 0, mouseY = 0;
    int         key    = 0;
    std::string text;
    float       scrollX = 0, scrollY = 0;
};

// ── Base Widget ───────────────────────────────────────────────────────────────
class Widget : public std::enable_shared_from_this<Widget> {
public:
    using Ptr = std::shared_ptr<Widget>;

    explicit Widget(const std::string& tag);
    virtual ~Widget() = default;
    Widget* parent() const { return m_parent; }

    // Tree
    void addChild(Ptr child);
    void removeChild(const std::string& id);
    Ptr  findById(const std::string& id);
    const std::vector<Ptr>& children() const { return m_children; }

    // Identity
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::string inlineStyle;

    // Content (for text widgets)
    std::string text;

    // Input value
    std::string value;

    // Style (resolved each frame or on dirty)
    CSSStyle    computedStyle;

    // Layout result
    LayoutBox   box;

    // State
    bool        hovered  = false;
    bool        focused  = false;
    bool        active   = false;
    bool        disabled = false;
    bool        visible  = true;

    // Event handlers — name of Waves callback to invoke
    std::unordered_map<std::string, std::string> eventHandlers;
    // e.g. eventHandlers["click"] = "onButtonClick"

    // Custom data attributes
    std::unordered_map<std::string, std::string> dataset;

    // Internal: render this widget using NanoVG
    virtual void render(NVGcontext* vg, float parentOpacity = 1.f);

    // Called when style needs recomputing
    void resolveStyle(const CSSParser& parser);

    // Animated style (for transitions)
    CSSStyle    animatedStyle;
    float       transitionProgress = 1.f; // 1 = done

protected:
    std::vector<Ptr> m_children;
    Widget*          m_parent = nullptr;

    void renderBackground(NVGcontext* vg, const CSSStyle& s, float opacity);
    void renderBorder(NVGcontext* vg, const CSSStyle& s, float opacity);
    void renderShadow(NVGcontext* vg, const CSSStyle& s, float opacity);
    void renderText(NVGcontext* vg, const CSSStyle& s, float opacity);
    void renderChildren(NVGcontext* vg, float opacity);

    void pushScissor(NVGcontext* vg, const CSSStyle& s);
    void popScissor(NVGcontext* vg, const CSSStyle& s);

    float getTL(const CSSStyle& s);
    float getTR(const CSSStyle& s);
    float getBL(const CSSStyle& s);
    float getBR(const CSSStyle& s);
};

// ── Div / Container ───────────────────────────────────────────────────────────
class DivWidget : public Widget {
public:
    DivWidget() : Widget("div") {}
};

// ── Text ──────────────────────────────────────────────────────────────────────
class TextWidget : public Widget {
public:
    TextWidget() : Widget("text") {}
    void render(NVGcontext* vg, float parentOpacity = 1.f) override;
};

// ── Button ────────────────────────────────────────────────────────────────────
class ButtonWidget : public Widget {
public:
    ButtonWidget() : Widget("button") {}
    void render(NVGcontext* vg, float parentOpacity = 1.f) override;
};

// ── Input ─────────────────────────────────────────────────────────────────────
class InputWidget : public Widget {
public:
    InputWidget() : Widget("input") {}
    std::string placeholder;
    std::string inputType = "text"; // text | password | number
    int         cursorPos = 0;
    float       cursorBlinkTimer = 0.f;
    void render(NVGcontext* vg, float parentOpacity = 1.f) override;
    void insertChar(char c);
    void deleteChar();
    void tick(float dt);
};

// ── Image ─────────────────────────────────────────────────────────────────────
class ImageWidget : public Widget {
public:
    ImageWidget() : Widget("img") {}
    std::string src;
    int         nvgImageHandle = -1;
    std::string objectFit = "cover"; // cover | contain | fill
    void render(NVGcontext* vg, float parentOpacity = 1.f) override;
};

// ── Scroll Container ──────────────────────────────────────────────────────────
class ScrollWidget : public Widget {
public:
    ScrollWidget() : Widget("scroll") {}
    float scrollOffsetY = 0.f;
    float scrollOffsetX = 0.f;
    float contentHeight = 0.f;
    void render(NVGcontext* vg, float parentOpacity = 1.f) override;
};

// ── Widget Factory ────────────────────────────────────────────────────────────
Widget::Ptr createWidget(const std::string& tag);