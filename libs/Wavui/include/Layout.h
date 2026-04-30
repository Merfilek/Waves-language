#pragma once
#include "Widget.h"

// ── Layout Engine ─────────────────────────────────────────────────────────────
// Implements a flexbox-lite layout pass on the widget tree.
// After this runs, every widget has a valid LayoutBox (x, y, w, h).

class LayoutEngine {
public:
    // Run a full layout pass starting from root within the given viewport
    void layout(Widget::Ptr root, float viewportW, float viewportH);

private:
    // Recursively compute sizes then positions
    void computeSize(Widget* w, float availW, float availH);
    void computePosition(Widget* w, float originX, float originY);

    // Flex layout for children of a flex container
    void layoutFlex(Widget* w);
    // Block layout for children of a block container
    void layoutBlock(Widget* w);

    float resolveWidth (Widget* w, float availW);
    float resolveHeight(Widget* w, float availH);

    float paddingH(const CSSStyle& s); // left + right
    float paddingV(const CSSStyle& s); // top  + bottom
    float marginH (const CSSStyle& s);
    float marginV (const CSSStyle& s);
};