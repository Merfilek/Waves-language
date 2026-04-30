#include "Layout.h"
#include <algorithm>
#include <cmath>

float LayoutEngine::paddingH(const CSSStyle& s) { return s.padding[1] + s.padding[3]; }
float LayoutEngine::paddingV(const CSSStyle& s) { return s.padding[0] + s.padding[2]; }
float LayoutEngine::marginH (const CSSStyle& s) { return s.margin[1]  + s.margin[3];  }
float LayoutEngine::marginV (const CSSStyle& s) { return s.margin[0]  + s.margin[2];  }

void LayoutEngine::layout(Widget::Ptr root, float viewportW, float viewportH) {
    root->box = {0, 0, viewportW, viewportH};
    computeSize(root.get(), viewportW, viewportH);
    computePosition(root.get(), 0, 0);
}

// ── Intrinsic height for leaf text/input nodes ────────────────────────────────
// Since we don't have font metrics in the layout pass, use fontSize * lineHeight.
// This gives a reasonable single-line height. Multi-line wrapping is handled
// by the renderer (nvgTextBox) but layout still needs a height estimate.
static float intrinsicHeight(Widget* w) {
    const CSSStyle& s = w->computedStyle;
    const std::string& t = w->tag;

    // Explicit height always wins
    if (s.height >= 0) return s.height;

    bool isTextLeaf = (t=="text"||t=="label"||t=="p"||t=="span") && w->children().empty();
    bool isInput    = (t=="input"||t=="textfield");
    bool isButton   = (t=="button"||t=="btn");

    if (isTextLeaf) {
        // Single-line height: fontSize * lineHeight + vertical padding
        return s.fontSize * s.lineHeight + s.padding[0] + s.padding[2];
    }
    if (isInput || isButton) {
        // These have explicit heights in CSS (.text-input: 48px, .btn-primary: 52px)
        // If not set, use a sensible default
        return s.fontSize * 2.5f + s.padding[0] + s.padding[2];
    }
    return -1.f; // auto — will be resolved from availH or children
}

float LayoutEngine::resolveWidth(Widget* w, float availW) {
    const CSSStyle& s = w->computedStyle;
    if (s.width >= 0) return std::clamp(s.width, s.minWidth, s.maxWidth);
    // 100% width = fill available (minus margins)
    return std::clamp(availW - marginH(s), s.minWidth, s.maxWidth);
}

float LayoutEngine::resolveHeight(Widget* w, float availH) {
    const CSSStyle& s = w->computedStyle;

    // Explicit height
    if (s.height >= 0) return std::clamp(s.height, s.minHeight, s.maxHeight);

    // Intrinsic height for leaves
    float ih = intrinsicHeight(w);
    if (ih >= 0) return std::clamp(ih, s.minHeight, s.maxHeight);

    // Container: auto = shrink to children (return 0 now, fixed up later)
    // Use availH only for root/fill contexts
    return 0.f;
}

void LayoutEngine::computeSize(Widget* w, float availW, float availH) {
    const CSSStyle& s = w->computedStyle;

    w->box.w = resolveWidth(w, availW);

    // For height: try explicit first, then intrinsic, then auto
    float ih = intrinsicHeight(w);
    if (s.height >= 0) {
        w->box.h = std::clamp(s.height, s.minHeight, s.maxHeight);
    } else if (ih >= 0) {
        w->box.h = std::clamp(ih, s.minHeight, s.maxHeight);
    } else {
        // Auto container — will be computed from children below
        w->box.h = 0;
    }

    // Layout children
    if (s.display == "flex") {
        layoutFlex(w);
    } else {
        layoutBlock(w);
    }

    // Auto height: shrink-wrap children
    if (s.height < 0 && ih < 0) {
        float contentH = 0;
        if (s.display == "flex" && s.flexDirection == "column") {
            for (auto& c : w->children()) {
                if (!c->visible || c->computedStyle.position == "absolute") continue;
                contentH += c->box.h + marginV(c->computedStyle);
            }
            // Add gaps
            int count = 0;
            for (auto& c : w->children())
                if (c->visible && c->computedStyle.position != "absolute") count++;
            if (count > 1) contentH += s.gap * (count - 1);
        } else if (s.display == "flex" && s.flexDirection == "row") {
            for (auto& c : w->children()) {
                if (!c->visible || c->computedStyle.position == "absolute") continue;
                contentH = std::max(contentH, c->box.h + marginV(c->computedStyle));
            }
        } else {
            // Block
            for (auto& c : w->children()) {
                if (!c->visible || c->computedStyle.position == "absolute") continue;
                contentH = std::max(contentH, c->box.y + c->box.h + c->computedStyle.margin[2]);
            }
        }
        w->box.h = std::max(0.f, contentH + paddingV(s));
        w->box.h = std::clamp(w->box.h, s.minHeight, s.maxHeight);
    }
}

void LayoutEngine::layoutBlock(Widget* w) {
    const CSSStyle& s = w->computedStyle;
    float innerW = w->box.w - paddingH(s);
    float innerH = w->box.h - paddingV(s);

    float curY = s.padding[0];
    for (auto& child : w->children()) {
        if (!child->visible) continue;
        const CSSStyle& cs = child->computedStyle;

        if (cs.position == "absolute") {
            child->box.w = resolveWidth(child.get(), w->box.w);
            child->box.h = resolveHeight(child.get(), w->box.h);
            computeSize(child.get(), child->box.w, child->box.h);
            continue;
        }

        child->box.w = resolveWidth(child.get(), innerW - marginH(cs));
        computeSize(child.get(), child->box.w, innerH); // pass innerH for context

        curY += cs.margin[0];
        child->box.y = curY;
        child->box.x = s.padding[3] + cs.margin[3];
        curY += child->box.h + cs.margin[2] + s.gap;
    }
}

void LayoutEngine::layoutFlex(Widget* w) {
    const CSSStyle& s = w->computedStyle;
    bool isRow = (s.flexDirection != "column");
    float innerW = w->box.w - paddingH(s);
    float innerH = w->box.h - paddingV(s);

    // Collect in-flow children (skip absolute)
    std::vector<Widget*> children;
    for (auto& c : w->children()) {
        if (c->visible && c->computedStyle.position != "absolute")
            children.push_back(c.get());
    }

    // First pass: size all non-flex children
    float fixedMain = 0;
    float totalFlex = 0;

    for (auto* c : children) {
        const CSSStyle& cs = c->computedStyle;
        if (cs.flex > 0) {
            totalFlex += cs.flex;
        } else {
            if (isRow) {
                c->box.w = resolveWidth(c, innerW);
                c->box.h = resolveHeight(c, innerH - marginV(cs));
                if (c->box.h == 0) { computeSize(c, c->box.w, innerH); }
                fixedMain += c->box.w + marginH(cs);
            } else {
                c->box.w = resolveWidth(c, innerW - marginH(cs));
                c->box.h = resolveHeight(c, innerH);
                if (c->box.h == 0) { computeSize(c, c->box.w, innerH); }
                fixedMain += c->box.h + marginV(cs);
            }
        }
    }

    float totalGap  = (children.size() > 1) ? s.gap * (float)(children.size() - 1) : 0.f;
    float remaining = (isRow ? innerW : innerH) - fixedMain - totalGap;

    // Second pass: flex children
    for (auto* c : children) {
        const CSSStyle& cs = c->computedStyle;
        if (cs.flex > 0) {
            float share = (totalFlex > 0) ? (cs.flex / totalFlex) * remaining : 0;
            if (isRow) {
                c->box.w = std::max(0.f, share - marginH(cs));
                c->box.h = resolveHeight(c, innerH - marginV(cs));
            } else {
                c->box.h = std::max(0.f, share - marginV(cs));
                c->box.w = resolveWidth(c, innerW - marginH(cs));
            }
        }
        computeSize(c, c->box.w, c->box.h);
    }

    // Third pass: position along main axis (justify-content)
    float mainSize = 0;
    for (auto* c : children)
        mainSize += isRow ? (c->box.w + marginH(c->computedStyle))
                          : (c->box.h + marginV(c->computedStyle));
    mainSize += totalGap;

    float mainAvail   = isRow ? innerW : innerH;
    float startOffset = isRow ? s.padding[3] : s.padding[0];
    float spacing     = 0;

    if (s.justifyContent == "center") {
        startOffset += (mainAvail - mainSize) * 0.5f;
    } else if (s.justifyContent == "flex-end") {
        startOffset += mainAvail - mainSize;
    } else if (s.justifyContent == "space-between" && children.size() > 1) {
        spacing = (mainAvail - mainSize + totalGap) / (float)(children.size() - 1);
    } else if (s.justifyContent == "space-around" && !children.empty()) {
        spacing = (mainAvail - mainSize + totalGap) / (float)children.size();
        startOffset += spacing * 0.5f;
    } else if (s.justifyContent == "space-evenly" && !children.empty()) {
        spacing = (mainAvail - mainSize + totalGap) / (float)(children.size() + 1);
        startOffset += spacing;
    }

    float cursor = startOffset;
    for (auto* c : children) {
        const CSSStyle& cs = c->computedStyle;

        // Cross-axis alignment
        float crossStart = isRow ? s.padding[0] : s.padding[3];
        float crossAvail = isRow ? innerH : innerW;

        float crossPos = crossStart + (isRow ? cs.margin[0] : cs.margin[3]);
        if (s.alignItems == "center") {
            float crossSize = isRow ? (c->box.h + marginV(cs)) : (c->box.w + marginH(cs));
            crossPos = crossStart + (crossAvail - crossSize) * 0.5f + (isRow ? cs.margin[0] : cs.margin[3]);
        } else if (s.alignItems == "flex-end") {
            crossPos = crossStart + crossAvail - (isRow ? c->box.h + cs.margin[2] : c->box.w + cs.margin[1]);
        } else if (s.alignItems == "stretch") {
            if (isRow) {
                c->box.h = std::max(c->box.h, innerH - marginV(cs));
            } else {
                c->box.w = std::max(c->box.w, innerW - marginH(cs));
            }
        }

        if (isRow) {
            c->box.x = cursor + cs.margin[3];
            c->box.y = crossPos;
            cursor += c->box.w + marginH(cs) + s.gap + spacing;
        } else {
            c->box.y = cursor + cs.margin[0];
            c->box.x = crossPos;
            cursor += c->box.h + marginV(cs) + s.gap + spacing;
        }
    }

    // Handle absolutely positioned children (size only — positioned in computePosition)
    for (auto& ch : w->children()) {
        const CSSStyle& cs = ch->computedStyle;
        if (cs.position == "absolute") {
            ch->box.w = resolveWidth(ch.get(), w->box.w);
            ch->box.h = resolveHeight(ch.get(), w->box.h);
            computeSize(ch.get(), ch->box.w, ch->box.h);
        }
    }
}

void LayoutEngine::computePosition(Widget* w, float originX, float originY) {
    for (auto& child : w->children()) {
        if (!child->visible) continue;
        const CSSStyle& cs = child->computedStyle;

        if (cs.position == "absolute") {
            // Position relative to parent's origin (top-left of parent's border box)
            float ax = originX;
            float ay = originY;

            // left/top take precedence over right/bottom
            if (cs.set[P_LEFT])        ax = originX + cs.left;
            else if (cs.set[P_RIGHT])  ax = originX + w->box.w - child->box.w - cs.right;

            if (cs.set[P_TOP])         ay = originY + cs.top;
            else if (cs.set[P_BOTTOM]) ay = originY + w->box.h - child->box.h - cs.bottom;

            child->box.x = ax;
            child->box.y = ay;
        } else {
            // Relative: local position is relative to parent, add parent's global origin
            child->box.x += originX;
            child->box.y += originY;
        }

        computePosition(child.get(), child->box.x, child->box.y);
    }
}