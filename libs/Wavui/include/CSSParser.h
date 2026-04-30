#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <bitset>

struct CSSColor {
    float r, g, b, a;
    CSSColor() : r(0), g(0), b(0), a(1) {}
    CSSColor(float r, float g, float b, float a=1.f) : r(r), g(g), b(b), a(a) {}
    bool operator==(const CSSColor& o) const { return r==o.r&&g==o.g&&b==o.b&&a==o.a; }
};

struct CSSBoxShadow {
    float offsetX = 0, offsetY = 0, blur = 0, spread = 0;
    CSSColor color;
    bool inset = false;
    bool valid = false;
};

// Bit indices for CSSStyle::set — tracks which properties were explicitly set
enum CSSProp {
    P_BG=0, P_BG_BLUR, P_BORDER_COLOR, P_BORDER_WIDTH,
    P_BORDER_RADIUS, P_BORDER_RADIUS_TL, P_BORDER_RADIUS_TR,
    P_BORDER_RADIUS_BL, P_BORDER_RADIUS_BR, P_BOX_SHADOW,
    P_WIDTH, P_HEIGHT, P_MIN_WIDTH, P_MIN_HEIGHT, P_MAX_WIDTH, P_MAX_HEIGHT,
    P_PADDING_T, P_PADDING_R, P_PADDING_B, P_PADDING_L,
    P_MARGIN_T,  P_MARGIN_R,  P_MARGIN_B,  P_MARGIN_L,
    P_DISPLAY, P_FLEX_DIR, P_JUSTIFY, P_ALIGN_ITEMS, P_GAP, P_FLEX,
    P_COLOR, P_FONT_SIZE, P_FONT_FAMILY, P_FONT_WEIGHT, P_TEXT_ALIGN,
    P_LINE_HEIGHT, P_LETTER_SPACING, P_OPACITY, P_CURSOR, P_OVERFLOW,
    P_TRANSITION, P_POSITION, P_TOP, P_LEFT, P_RIGHT, P_BOTTOM,
    P_COUNT
};

struct CSSStyle {
    std::bitset<P_COUNT> set; // which properties were explicitly set

    // Background
    CSSColor        backgroundColor    = {0,0,0,0};
    float           backgroundBlur     = 0.f;

    // Border
    CSSColor        borderColor        = {0,0,0,0};
    float           borderWidth        = 0.f;
    float           borderRadius       = 0.f;
    float           borderRadiusTL     = 0.f;
    float           borderRadiusTR     = 0.f;
    float           borderRadiusBL     = 0.f;
    float           borderRadiusBR     = 0.f;

    // Box shadow
    CSSBoxShadow    boxShadow;

    // Size
    float           width              = -1.f; // -1 = auto
    float           height             = -1.f;
    float           minWidth           = 0.f;
    float           minHeight          = 0.f;
    float           maxWidth           = 1e9f;
    float           maxHeight          = 1e9f;

    // Padding (top right bottom left)
    std::array<float,4> padding        = {0,0,0,0};
    std::array<float,4> margin         = {0,0,0,0};

    // Flex
    std::string     display            = "block";
    std::string     flexDirection      = "row";
    std::string     justifyContent     = "flex-start";
    std::string     alignItems         = "stretch";
    float           gap                = 0.f;
    float           flex               = 0.f;

    // Text
    CSSColor        color              = {1,1,1,1};
    float           fontSize           = 16.f;
    std::string     fontFamily         = "sans";
    std::string     fontWeight         = "normal";
    std::string     textAlign          = "left";
    float           lineHeight         = 1.2f;
    float           letterSpacing      = 0.f;

    // Opacity / visibility
    float           opacity            = 1.f;

    // Cursor / overflow
    std::string     cursor             = "default";
    std::string     overflow           = "visible";

    // Transition (seconds)
    float           transitionDuration = 0.f;

    // Position
    std::string     position           = "relative";
    float           top                = 0.f;
    float           left               = 0.f;
    float           right              = -1e9f;
    float           bottom             = -1e9f;

    // Convenience: was background explicitly set (for has-background check)
    bool hasBackground() const { return set[P_BG]; }

    // Get effective corner radii (individual overrides borderRadius)
    float cornerTL() const { return set[P_BORDER_RADIUS_TL] ? borderRadiusTL : borderRadius; }
    float cornerTR() const { return set[P_BORDER_RADIUS_TR] ? borderRadiusTR : borderRadius; }
    float cornerBL() const { return set[P_BORDER_RADIUS_BL] ? borderRadiusBL : borderRadius; }
    float cornerBR() const { return set[P_BORDER_RADIUS_BR] ? borderRadiusBR : borderRadius; }
};

struct CSSRule {
    std::string     selector;
    std::string     pseudoClass; // "" | "hover" | "focus" | "active"
    CSSStyle        style;
    int             specificity = 0;
};

class CSSParser {
public:
    void parse(const std::string& css);
    bool parseFile(const std::string& path);

    // Resolve styles for a widget — returns merged style for given state
    CSSStyle resolve(const std::string& tag,
                     const std::string& id,
                     const std::vector<std::string>& classes,
                     const std::string& pseudoClass = "") const;

    // Parse inline style="..." string
    static CSSStyle parseInline(const std::string& inlineStyle);

    // Resolve base style then overlay pseudo-class properties on top
    CSSStyle resolveWithPseudo(const std::string& tag,
                                const std::string& id,
                                const std::vector<std::string>& classes,
                                const std::string& pseudoClass) const;

    // Returns true if any rule matches this element with the given pseudo-class
    bool hasPseudo(const std::string& tag,
                   const std::string& id,
                   const std::vector<std::string>& classes,
                   const std::string& pseudoClass) const;

    // Merge src on top of dst (only explicitly-set properties from src override)
    static CSSStyle merge(const CSSStyle& dst, const CSSStyle& src);

    // Merge inline style on top of a computed style
    static CSSStyle mergeInline(const CSSStyle& base, const CSSStyle& inl);

    const std::vector<CSSRule>& getRules() const { return m_rules; }

private:
    std::vector<CSSRule> m_rules;

    void parseRule(const std::string& selector, const std::string& body);
    void parseDeclarations(const std::string& body, CSSStyle& style);
    void parseProperty(const std::string& prop, const std::string& value, CSSStyle& style);

    static CSSColor     parseColor(const std::string& val);
    static float        parsePx(const std::string& val);
    static float        parsePercent(const std::string& val, float base = 100.f);
    static CSSBoxShadow parseBoxShadow(const std::string& val);
    static int          computeSpecificity(const std::string& selector);

    static std::string  trim(const std::string& s);
    static std::string  toLower(std::string s);
    static std::vector<std::string> split(const std::string& s, char delim);
};