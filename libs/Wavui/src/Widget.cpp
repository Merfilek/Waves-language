#include "Widget.h"
#include "CSSParser.h"
#include <nanovg/nanovg.h>
#include <algorithm>
#include <cmath>

// ── Widget base ───────────────────────────────────────────────────────────────

Widget::Widget(const std::string& tag_) : tag(tag_) {}

void Widget::addChild(Ptr child) {
    child->m_parent = this;
    m_children.push_back(child);
}

void Widget::removeChild(const std::string& wid) {
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [&](const Ptr& p){ return p->id == wid; }),
        m_children.end());
}

Widget::Ptr Widget::findById(const std::string& wid) {
    if (id == wid) return shared_from_this();
    for (auto& c : m_children) {
        auto r = c->findById(wid);
        if (r) return r;
    }
    return nullptr;
}

void Widget::resolveStyle(const CSSParser& parser) {
    // 1. Base style (no pseudo-class)
    computedStyle = parser.resolve(tag, id, classes, "");

    // 2. Overlay pseudo-class styles in priority order (active > focus > hover)
    //    resolveWithPseudo merges base + pseudo, only overwriting explicitly-set props
    if (active && parser.hasPseudo(tag, id, classes, "active")) {
        computedStyle = parser.resolveWithPseudo(tag, id, classes, "active");
    } else if (focused && parser.hasPseudo(tag, id, classes, "focus")) {
        computedStyle = parser.resolveWithPseudo(tag, id, classes, "focus");
    } else if (hovered && parser.hasPseudo(tag, id, classes, "hover")) {
        computedStyle = parser.resolveWithPseudo(tag, id, classes, "hover");
    }

    // 3. Inline style has highest priority
    if (!inlineStyle.empty()) {
        CSSStyle inl = CSSParser::parseInline(inlineStyle);
        computedStyle = CSSParser::merge(computedStyle, inl);
    }
}

// ── Corner radii helpers ──────────────────────────────────────────────────────

float Widget::getTL(const CSSStyle& s) { return s.set[P_BORDER_RADIUS_TL] ? s.borderRadiusTL : s.borderRadius; }
float Widget::getTR(const CSSStyle& s) { return s.set[P_BORDER_RADIUS_TR] ? s.borderRadiusTR : s.borderRadius; }
float Widget::getBL(const CSSStyle& s) { return s.set[P_BORDER_RADIUS_BL] ? s.borderRadiusBL : s.borderRadius; }
float Widget::getBR(const CSSStyle& s) { return s.set[P_BORDER_RADIUS_BR] ? s.borderRadiusBR : s.borderRadius; }

// ── Render helpers ────────────────────────────────────────────────────────────

static void roundedRect(NVGcontext* vg, float x, float y, float w, float h,
                         float tl, float tr, float br, float bl) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + tl, y);
    nvgLineTo(vg, x + w - tr, y);
    nvgArcTo (vg, x+w, y,     x+w,   y+tr,  tr);
    nvgLineTo(vg, x + w, y + h - br);
    nvgArcTo (vg, x+w, y+h,   x+w-br,y+h,   br);
    nvgLineTo(vg, x + bl, y + h);
    nvgArcTo (vg, x,   y+h,   x,     y+h-bl,bl);
    nvgLineTo(vg, x, y + tl);
    nvgArcTo (vg, x,   y,     x+tl,  y,     tl);
    nvgClosePath(vg);
}

void Widget::renderShadow(NVGcontext* vg, const CSSStyle& s, float opacity) {
    if (!s.boxShadow.valid) return;
    auto& sh = s.boxShadow;
    float x = box.x + sh.offsetX - sh.spread;
    float y = box.y + sh.offsetY - sh.spread;
    float w = box.w + sh.spread*2;
    float h = box.h + sh.spread*2;
    float r = s.borderRadius + sh.spread;

    NVGpaint paint = nvgBoxGradient(vg, x, y, w, h, r, sh.blur,
        nvgRGBAf(sh.color.r, sh.color.g, sh.color.b, sh.color.a * opacity),
        nvgRGBAf(sh.color.r, sh.color.g, sh.color.b, 0));

    nvgBeginPath(vg);
    nvgRect(vg, x - sh.blur, y - sh.blur, w + sh.blur*2, h + sh.blur*2);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

void Widget::renderBackground(NVGcontext* vg, const CSSStyle& s, float opacity) {
    // Use hasBackground() method — returns true if background was explicitly set
    if (!s.hasBackground() && s.backgroundColor.a == 0) return;
    auto& c = s.backgroundColor;
    nvgFillColor(vg, nvgRGBAf(c.r, c.g, c.b, c.a * opacity));
    roundedRect(vg, box.x, box.y, box.w, box.h, getTL(s), getTR(s), getBR(s), getBL(s));
    nvgFill(vg);
}

void Widget::renderBorder(NVGcontext* vg, const CSSStyle& s, float opacity) {
    if (s.borderWidth <= 0) return;
    auto& c = s.borderColor;
    nvgStrokeColor(vg, nvgRGBAf(c.r, c.g, c.b, c.a * opacity));
    nvgStrokeWidth(vg, s.borderWidth);
    float half = s.borderWidth * 0.5f;
    roundedRect(vg, box.x+half, box.y+half, box.w-s.borderWidth, box.h-s.borderWidth,
                getTL(s), getTR(s), getBR(s), getBL(s));
    nvgStroke(vg);
}

void Widget::renderText(NVGcontext* vg, const CSSStyle& s, float opacity) {
    if (text.empty()) return;

    std::string face = s.fontFamily;
    if (s.fontWeight=="bold"||s.fontWeight=="700"||s.fontWeight=="800"||s.fontWeight=="900")
        face += "-bold";

    nvgFontFace(vg, face.c_str());
    nvgFontSize(vg, s.fontSize);
    nvgTextLetterSpacing(vg, s.letterSpacing);

    auto& c = s.color;
    nvgFillColor(vg, nvgRGBAf(c.r, c.g, c.b, c.a * opacity));

    int halign = NVG_ALIGN_LEFT;
    if (s.textAlign=="center") halign = NVG_ALIGN_CENTER;
    if (s.textAlign=="right")  halign = NVG_ALIGN_RIGHT;
    nvgTextAlign(vg, halign | NVG_ALIGN_TOP);

    float tx = box.x + s.padding[3];
    float ty = box.y + s.padding[0];
    float maxW = box.w - s.padding[1] - s.padding[3];

    if (s.textAlign=="center") tx = box.x + box.w * 0.5f;
    if (s.textAlign=="right")  tx = box.x + box.w - s.padding[1];

    nvgTextBox(vg, tx, ty, maxW, text.c_str(), nullptr);
}

void Widget::pushScissor(NVGcontext* vg, const CSSStyle& s) {
    if (s.overflow=="hidden"||s.overflow=="scroll")
        nvgScissor(vg, box.x, box.y, box.w, box.h);
}

void Widget::popScissor(NVGcontext* vg, const CSSStyle& s) {
    if (s.overflow=="hidden"||s.overflow=="scroll")
        nvgResetScissor(vg);
}

void Widget::renderChildren(NVGcontext* vg, float opacity) {
    for (auto& child : m_children) {
        if (child->visible)
            child->render(vg, opacity);
    }
}

void Widget::render(NVGcontext* vg, float parentOpacity) {
    if (!visible) return;
    const CSSStyle& s = computedStyle;
    float opacity = s.opacity * parentOpacity;

    renderShadow(vg, s, opacity);
    renderBackground(vg, s, opacity);
    renderBorder(vg, s, opacity);
    renderText(vg, s, opacity);
    pushScissor(vg, s);
    renderChildren(vg, opacity);
    popScissor(vg, s);
}

// ── TextWidget ────────────────────────────────────────────────────────────────

void TextWidget::render(NVGcontext* vg, float parentOpacity) {
    const CSSStyle& s = computedStyle;
    float opacity = s.opacity * parentOpacity;
    renderText(vg, s, opacity);
    renderChildren(vg, opacity);
}

// ── ButtonWidget ──────────────────────────────────────────────────────────────

void ButtonWidget::render(NVGcontext* vg, float parentOpacity) {
    if (!visible) return;
    const CSSStyle& s = computedStyle;
    float opacity = s.opacity * parentOpacity;

    renderShadow(vg, s, opacity);
    renderBackground(vg, s, opacity);
    renderBorder(vg, s, opacity);

    if (!text.empty()) {
        std::string face = s.fontFamily;
        if (s.fontWeight=="bold") face += "-bold";
        nvgFontFace(vg, face.c_str());
        nvgFontSize(vg, s.fontSize);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        auto& c = s.color;
        nvgFillColor(vg, nvgRGBAf(c.r, c.g, c.b, c.a * opacity));
        nvgText(vg, box.x + box.w*0.5f, box.y + box.h*0.5f, text.c_str(), nullptr);
    }

    pushScissor(vg, s);
    renderChildren(vg, opacity);
    popScissor(vg, s);
}

// ── InputWidget ───────────────────────────────────────────────────────────────

void InputWidget::insertChar(char c) {
    if (cursorPos <= (int)value.size()) {
        value.insert(value.begin() + cursorPos, c);
        cursorPos++;
    }
}

void InputWidget::deleteChar() {
    if (cursorPos > 0 && !value.empty()) {
        value.erase(value.begin() + cursorPos - 1);
        cursorPos--;
    }
}

void InputWidget::tick(float dt) {
    cursorBlinkTimer += dt;
}

void InputWidget::render(NVGcontext* vg, float parentOpacity) {
    if (!visible) return;
    const CSSStyle& s = computedStyle;
    float opacity = s.opacity * parentOpacity;

    renderShadow(vg, s, opacity);
    renderBackground(vg, s, opacity);
    renderBorder(vg, s, opacity);

    std::string display = value;
    if (inputType == "password") display = std::string(value.size(), '*');
    bool showPlaceholder = display.empty() && !placeholder.empty();

    nvgScissor(vg, box.x + s.padding[3], box.y, box.w - s.padding[1] - s.padding[3], box.h);

    nvgFontFace(vg, s.fontFamily.c_str());
    nvgFontSize(vg, s.fontSize);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    float ty = box.y + box.h * 0.5f;
    float tx = box.x + s.padding[3];

    if (showPlaceholder) {
        auto& c = s.color;
        nvgFillColor(vg, nvgRGBAf(c.r, c.g, c.b, c.a * 0.4f * opacity));
        nvgText(vg, tx, ty, placeholder.c_str(), nullptr);
    } else {
        auto& c = s.color;
        nvgFillColor(vg, nvgRGBAf(c.r, c.g, c.b, c.a * opacity));
        nvgText(vg, tx, ty, display.c_str(), nullptr);
    }

    if (focused && (int)(cursorBlinkTimer * 2) % 2 == 0) {
        float cursorX = tx;
        if (cursorPos > 0) {
            float bounds[4];
            std::string sub = display.substr(0, cursorPos);
            nvgTextBounds(vg, tx, ty, sub.c_str(), nullptr, bounds);
            cursorX = bounds[2];
        }
        auto& c = s.color;
        nvgBeginPath(vg);
        nvgStrokeColor(vg, nvgRGBAf(c.r, c.g, c.b, opacity));
        nvgStrokeWidth(vg, 1.5f);
        nvgMoveTo(vg, cursorX, box.y + s.padding[0]);
        nvgLineTo(vg, cursorX, box.y + box.h - s.padding[2]);
        nvgStroke(vg);
    }

    nvgResetScissor(vg);
    renderBorder(vg, s, opacity); // redraw border on top of text
    renderChildren(vg, opacity);
}

// ── ImageWidget ───────────────────────────────────────────────────────────────

void ImageWidget::render(NVGcontext* vg, float parentOpacity) {
    if (!visible) return;
    const CSSStyle& s = computedStyle;
    float opacity = s.opacity * parentOpacity;

    renderShadow(vg, s, opacity);

    if (nvgImageHandle >= 0) {
        int imgW, imgH;
        nvgImageSize(vg, nvgImageHandle, &imgW, &imgH);

        float sx = box.w / (float)imgW;
        float sy = box.h / (float)imgH;
        float scale, ox=0, oy=0;

        if (objectFit=="cover") {
            scale = std::max(sx,sy);
            ox = (box.w - imgW*scale) * 0.5f;
            oy = (box.h - imgH*scale) * 0.5f;
        } else if (objectFit=="contain") {
            scale = std::min(sx,sy);
            ox = (box.w - imgW*scale) * 0.5f;
            oy = (box.h - imgH*scale) * 0.5f;
        } else {
            scale = 1;
        }

        NVGpaint imgPaint = nvgImagePattern(vg,
            box.x + ox, box.y + oy,
            imgW*scale, imgH*scale,
            0, nvgImageHandle, opacity);

        nvgBeginPath(vg);
        roundedRect(vg, box.x, box.y, box.w, box.h, getTL(s), getTR(s), getBR(s), getBL(s));
        nvgFillPaint(vg, imgPaint);
        nvgFill(vg);
    } else {
        renderBackground(vg, s, opacity);
    }

    renderBorder(vg, s, opacity);
    renderChildren(vg, opacity);
}

// ── ScrollWidget ──────────────────────────────────────────────────────────────

void ScrollWidget::render(NVGcontext* vg, float parentOpacity) {
    if (!visible) return;
    const CSSStyle& s = computedStyle;
    float opacity = s.opacity * parentOpacity;

    renderShadow(vg, s, opacity);
    renderBackground(vg, s, opacity);

    nvgScissor(vg, box.x, box.y, box.w, box.h);
    nvgSave(vg);
    nvgTranslate(vg, -scrollOffsetX, -scrollOffsetY);
    renderChildren(vg, opacity);
    nvgRestore(vg);
    nvgResetScissor(vg);

    if (contentHeight > box.h) {
        float trackH = box.h;
        float thumbH = std::max(30.f, box.h * (box.h / contentHeight));
        float thumbY = box.y + (scrollOffsetY / (contentHeight - box.h)) * (trackH - thumbH);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, box.x + box.w - 6, box.y, 4, box.h, 2);
        nvgFillColor(vg, nvgRGBAf(0,0,0,0.1f*opacity));
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, box.x + box.w - 6, thumbY, 4, thumbH, 2);
        nvgFillColor(vg, nvgRGBAf(0,0,0,0.3f*opacity));
        nvgFill(vg);
    }

    renderBorder(vg, s, opacity);
}

// ── Factory ───────────────────────────────────────────────────────────────────

Widget::Ptr createWidget(const std::string& tag) {
    if (tag=="div"||tag=="container"||tag=="view") return std::make_shared<DivWidget>();
    if (tag=="button"||tag=="btn")                 return std::make_shared<ButtonWidget>();
    if (tag=="text"||tag=="label"||tag=="p"||tag=="span") return std::make_shared<TextWidget>();
    if (tag=="input"||tag=="textfield")            return std::make_shared<InputWidget>();
    if (tag=="img"||tag=="image")                  return std::make_shared<ImageWidget>();
    if (tag=="scroll"||tag=="scrollview")          return std::make_shared<ScrollWidget>();
    return std::make_shared<DivWidget>();
}