#include "CSSParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string CSSParser::trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\n\r");
    size_t b = s.find_last_not_of(" \t\n\r");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

std::string CSSParser::toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::vector<std::string> CSSParser::split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, delim))
        out.push_back(trim(tok));
    return out;
}

// ── Color parsing ─────────────────────────────────────────────────────────────

static float h2(const std::string& s, size_t pos) {
    auto hex = [](char c) -> int {
        if (c>='0'&&c<='9') return c-'0';
        if (c>='a'&&c<='f') return c-'a'+10;
        if (c>='A'&&c<='F') return c-'A'+10;
        return 0;
    };
    if (pos+1 >= s.size()) return 0;
    return (hex(s[pos])*16 + hex(s[pos+1])) / 255.f;
}

CSSColor CSSParser::parseColor(const std::string& raw) {
    std::string v = trim(raw);

    if (!v.empty() && v[0] == '#') {
        std::string h = v.substr(1);
        if (h.size() == 3)
            h = {h[0],h[0],h[1],h[1],h[2],h[2]};
        if (h.size() == 6) return { h2(h,0), h2(h,2), h2(h,4), 1.f };
        if (h.size() == 8) return { h2(h,0), h2(h,2), h2(h,4), h2(h,6) };
    }

    if (v.substr(0,4) == "rgb(" || v.substr(0,5) == "rgba(") {
        size_t s = v.find('('), e = v.find(')');
        if (s != std::string::npos && e != std::string::npos) {
            auto parts = split(v.substr(s+1, e-s-1), ',');
            float r = parts.size()>0 ? std::stof(parts[0])/255.f : 0;
            float g = parts.size()>1 ? std::stof(parts[1])/255.f : 0;
            float b = parts.size()>2 ? std::stof(parts[2])/255.f : 0;
            float a = parts.size()>3 ? std::stof(parts[3])        : 1.f;
            return {r,g,b,a};
        }
    }

    std::string lo = toLower(v);
    if (lo=="transparent") return {0,0,0,0};
    if (lo=="white")       return {1,1,1,1};
    if (lo=="black")       return {0,0,0,1};
    if (lo=="red")         return {1,0,0,1};
    if (lo=="green")       return {0,0.5f,0,1};
    if (lo=="blue")        return {0,0,1,1};
    if (lo=="none")        return {0,0,0,0};
    if (lo=="gray"||lo=="grey") return {0.5f,0.5f,0.5f,1};

    return {0,0,0,0}; // unknown = transparent
}

// ── Value parsing ─────────────────────────────────────────────────────────────

float CSSParser::parsePx(const std::string& val) {
    std::string v = trim(val);
    if (v.empty()) return 0;
    size_t unit = v.find_first_not_of("0123456789.-");
    if (unit == 0) return 0;
    try { return std::stof(v.substr(0, unit == std::string::npos ? v.size() : unit)); }
    catch (...) { return 0; }
}

float CSSParser::parsePercent(const std::string& val, float base) {
    std::string v = trim(val);
    if (!v.empty() && v.back() == '%')
        return std::stof(v.substr(0, v.size()-1)) / 100.f * base;
    return parsePx(v);
}

CSSBoxShadow CSSParser::parseBoxShadow(const std::string& val) {
    CSSBoxShadow s;
    std::string v = trim(val);
    if (v == "none") return s;
    s.valid = true;

    bool inset = false;
    if (v.substr(0,5) == "inset") { inset = true; v = trim(v.substr(5)); }
    s.inset = inset;

    std::vector<std::string> tokens;
    std::string cur;
    int depth = 0;
    for (char c : v) {
        if (c=='(') depth++;
        if (c==')') depth--;
        if (c==' ' && depth==0) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else cur += c;
    }
    if (!cur.empty()) tokens.push_back(cur);

    std::vector<float> nums;
    std::string colorStr;
    for (auto& t : tokens) {
        if (!t.empty() && (std::isdigit((unsigned char)t[0]) || t[0]=='-' || t[0]=='.')) {
            nums.push_back(parsePx(t));
        } else if (!t.empty()) {
            colorStr = t;
        }
    }

    if (nums.size()>=2) { s.offsetX=nums[0]; s.offsetY=nums[1]; }
    if (nums.size()>=3)   s.blur   =nums[2];
    if (nums.size()>=4)   s.spread =nums[3];
    s.color = colorStr.empty() ? CSSColor{0,0,0,0.3f} : parseColor(colorStr);
    return s;
}

int CSSParser::computeSpecificity(const std::string& sel) {
    int score = 0;
    for (char c : sel) {
        if (c == '#') score += 100;
        if (c == '.') score += 10;
        if (c == ':') score += 1;
    }
    if (!sel.empty() && sel[0] != '#' && sel[0] != '.') score += 1;
    return score;
}

// ── Property dispatcher ───────────────────────────────────────────────────────
// Every branch that sets a field also sets the corresponding bit in s.set.

void CSSParser::parseProperty(const std::string& prop, const std::string& value, CSSStyle& s) {
    std::string p = toLower(trim(prop));
    std::string v = trim(value);
    if (p.empty() || v.empty()) return;

    if (p=="background"||p=="background-color") {
        if (v.find("gradient")!=std::string::npos) return;
        s.backgroundColor = parseColor(v);
        s.set[P_BG] = true;
    }
    else if (p=="backdrop-filter"||p=="background-blur") {
        size_t a=v.find('('), b=v.find(')');
        if (a!=std::string::npos&&b!=std::string::npos) {
            s.backgroundBlur = parsePx(v.substr(a+1,b-a-1));
            s.set[P_BG_BLUR] = true;
        }
    }
    else if (p=="opacity") {
        try { s.opacity=std::stof(v); s.set[P_OPACITY]=true; } catch(...){}
    }
    else if (p=="border-radius") {
        auto parts = split(v,' ');
        if (parts.size()==1) {
            s.borderRadius=parsePx(parts[0]); s.set[P_BORDER_RADIUS]=true;
        } else if (parts.size()==4) {
            s.borderRadiusTL=parsePx(parts[0]); s.set[P_BORDER_RADIUS_TL]=true;
            s.borderRadiusTR=parsePx(parts[1]); s.set[P_BORDER_RADIUS_TR]=true;
            s.borderRadiusBR=parsePx(parts[2]); s.set[P_BORDER_RADIUS_BR]=true;
            s.borderRadiusBL=parsePx(parts[3]); s.set[P_BORDER_RADIUS_BL]=true;
        }
    }
    else if (p=="border-top-left-radius")     { s.borderRadiusTL=parsePx(v); s.set[P_BORDER_RADIUS_TL]=true; }
    else if (p=="border-top-right-radius")    { s.borderRadiusTR=parsePx(v); s.set[P_BORDER_RADIUS_TR]=true; }
    else if (p=="border-bottom-left-radius")  { s.borderRadiusBL=parsePx(v); s.set[P_BORDER_RADIUS_BL]=true; }
    else if (p=="border-bottom-right-radius") { s.borderRadiusBR=parsePx(v); s.set[P_BORDER_RADIUS_BR]=true; }
    else if (p=="border-color") { s.borderColor=parseColor(v); s.set[P_BORDER_COLOR]=true; }
    else if (p=="border-width") { s.borderWidth=parsePx(v);   s.set[P_BORDER_WIDTH]=true; }
    else if (p=="border") {
        if (v=="none") { s.borderWidth=0; s.set[P_BORDER_WIDTH]=true; return; }
        auto parts = split(v,' ');
        for (auto& pt : parts) {
            std::string pl = toLower(pt);
            if (pl=="solid"||pl=="dashed"||pl=="dotted"||pl=="none") continue;
            if (!pt.empty()&&(pt[0]=='#'||pt.substr(0,3)=="rgb")) {
                s.borderColor=parseColor(pt); s.set[P_BORDER_COLOR]=true;
            } else {
                float w=parsePx(pt);
                if (w>=0) { s.borderWidth=w; s.set[P_BORDER_WIDTH]=true; }
            }
        }
    }
    else if (p=="box-shadow") { s.boxShadow=parseBoxShadow(v); s.set[P_BOX_SHADOW]=true; }
    else if (p=="width")      { if(v!="auto"){s.width =parsePx(v);s.set[P_WIDTH]=true;} }
    else if (p=="height")     { if(v!="auto"){s.height=parsePx(v);s.set[P_HEIGHT]=true;} }
    else if (p=="min-width")  { s.minWidth =parsePx(v); s.set[P_MIN_WIDTH]=true; }
    else if (p=="min-height") { s.minHeight=parsePx(v); s.set[P_MIN_HEIGHT]=true; }
    else if (p=="max-width")  { s.maxWidth =parsePx(v); s.set[P_MAX_WIDTH]=true; }
    else if (p=="max-height") { s.maxHeight=parsePx(v); s.set[P_MAX_HEIGHT]=true; }
    else if (p=="padding") {
        auto pts=split(v,' ');
        if (pts.size()==1) {
            float x=parsePx(pts[0]);
            s.padding={x,x,x,x};
            s.set[P_PADDING_T]=s.set[P_PADDING_R]=s.set[P_PADDING_B]=s.set[P_PADDING_L]=true;
        } else if (pts.size()==2) {
            float tb=parsePx(pts[0]),lr=parsePx(pts[1]);
            s.padding={tb,lr,tb,lr};
            s.set[P_PADDING_T]=s.set[P_PADDING_R]=s.set[P_PADDING_B]=s.set[P_PADDING_L]=true;
        } else if (pts.size()>=4) {
            s.padding={parsePx(pts[0]),parsePx(pts[1]),parsePx(pts[2]),parsePx(pts[3])};
            s.set[P_PADDING_T]=s.set[P_PADDING_R]=s.set[P_PADDING_B]=s.set[P_PADDING_L]=true;
        }
    }
    else if (p=="padding-top")    { s.padding[0]=parsePx(v); s.set[P_PADDING_T]=true; }
    else if (p=="padding-right")  { s.padding[1]=parsePx(v); s.set[P_PADDING_R]=true; }
    else if (p=="padding-bottom") { s.padding[2]=parsePx(v); s.set[P_PADDING_B]=true; }
    else if (p=="padding-left")   { s.padding[3]=parsePx(v); s.set[P_PADDING_L]=true; }
    else if (p=="margin") {
        auto pts=split(v,' ');
        if (pts.size()==1) {
            float x=parsePx(pts[0]);
            s.margin={x,x,x,x};
            s.set[P_MARGIN_T]=s.set[P_MARGIN_R]=s.set[P_MARGIN_B]=s.set[P_MARGIN_L]=true;
        } else if (pts.size()==2) {
            float tb=parsePx(pts[0]),lr=parsePx(pts[1]);
            s.margin={tb,lr,tb,lr};
            s.set[P_MARGIN_T]=s.set[P_MARGIN_R]=s.set[P_MARGIN_B]=s.set[P_MARGIN_L]=true;
        } else if (pts.size()>=4) {
            s.margin={parsePx(pts[0]),parsePx(pts[1]),parsePx(pts[2]),parsePx(pts[3])};
            s.set[P_MARGIN_T]=s.set[P_MARGIN_R]=s.set[P_MARGIN_B]=s.set[P_MARGIN_L]=true;
        }
    }
    else if (p=="margin-top")    { s.margin[0]=parsePx(v); s.set[P_MARGIN_T]=true; }
    else if (p=="margin-right")  { s.margin[1]=parsePx(v); s.set[P_MARGIN_R]=true; }
    else if (p=="margin-bottom") { s.margin[2]=parsePx(v); s.set[P_MARGIN_B]=true; }
    else if (p=="margin-left")   { s.margin[3]=parsePx(v); s.set[P_MARGIN_L]=true; }
    else if (p=="display")         { s.display       =toLower(v); s.set[P_DISPLAY]=true; }
    else if (p=="flex-direction")  { s.flexDirection =toLower(v); s.set[P_FLEX_DIR]=true; }
    else if (p=="justify-content") { s.justifyContent=toLower(v); s.set[P_JUSTIFY]=true; }
    else if (p=="align-items")     { s.alignItems    =toLower(v); s.set[P_ALIGN_ITEMS]=true; }
    else if (p=="gap")             { s.gap =parsePx(v); s.set[P_GAP]=true; }
    else if (p=="flex")            { try{s.flex=std::stof(v);s.set[P_FLEX]=true;}catch(...){} }
    else if (p=="color")           { s.color=parseColor(v);   s.set[P_COLOR]=true; }
    else if (p=="font-size")       { s.fontSize=parsePx(v);   s.set[P_FONT_SIZE]=true; }
    else if (p=="font-family")     { s.fontFamily=trim(v);    s.set[P_FONT_FAMILY]=true; }
    else if (p=="font-weight")     { s.fontWeight=toLower(v); s.set[P_FONT_WEIGHT]=true; }
    else if (p=="text-align")      { s.textAlign=toLower(v);  s.set[P_TEXT_ALIGN]=true; }
    else if (p=="line-height")     { try{s.lineHeight=std::stof(v);s.set[P_LINE_HEIGHT]=true;}catch(...){} }
    else if (p=="letter-spacing")  { s.letterSpacing=parsePx(v); s.set[P_LETTER_SPACING]=true; }
    else if (p=="cursor")          { s.cursor  =toLower(v); s.set[P_CURSOR]=true; }
    else if (p=="overflow")        { s.overflow=toLower(v); s.set[P_OVERFLOW]=true; }
    else if (p=="transition"||p=="transition-duration") {
        float dur = 0;
        if (v.find("ms")!=std::string::npos) {
            try { dur=std::stof(v)/1000.f; } catch(...) {}
        } else {
            try { dur=std::stof(v); } catch(...) {}
        }
        s.transitionDuration=dur; s.set[P_TRANSITION]=true;
    }
    else if (p=="position") { s.position=toLower(v); s.set[P_POSITION]=true; }
    else if (p=="top")    { s.top   =parsePx(v); s.set[P_TOP]=true; }
    else if (p=="left")   { s.left  =parsePx(v); s.set[P_LEFT]=true; }
    else if (p=="right")  { s.right =parsePx(v); s.set[P_RIGHT]=true; }
    else if (p=="bottom") { s.bottom=parsePx(v); s.set[P_BOTTOM]=true; }
}

// ── Declaration block parser ──────────────────────────────────────────────────

void CSSParser::parseDeclarations(const std::string& body, CSSStyle& s) {
    auto decls = split(body, ';');
    for (auto& decl : decls) {
        auto colon = decl.find(':');
        if (colon == std::string::npos) continue;
        std::string prop  = trim(decl.substr(0, colon));
        std::string value = trim(decl.substr(colon+1));
        if (!prop.empty() && !value.empty())
            parseProperty(prop, value, s);
    }
}

// ── Rule parser ───────────────────────────────────────────────────────────────

void CSSParser::parseRule(const std::string& selector, const std::string& body) {
    auto selectors = split(selector, ',');
    for (auto& sel : selectors) {
        CSSRule rule;
        std::string s = trim(sel);

        size_t colon = s.find(':');
        if (colon != std::string::npos) {
            rule.pseudoClass = s.substr(colon+1);
            s = s.substr(0, colon);
        }
        rule.selector    = s;
        rule.specificity = computeSpecificity(sel);
        parseDeclarations(body, rule.style);
        m_rules.push_back(rule);
    }
}

// ── Parse CSS string ──────────────────────────────────────────────────────────

void CSSParser::parse(const std::string& css) {
    std::string src;
    size_t i = 0;
    while (i < css.size()) {
        if (i+1 < css.size() && css[i]=='/' && css[i+1]=='*') {
            size_t end = css.find("*/", i+2);
            i = (end==std::string::npos) ? css.size() : end+2;
        } else {
            src += css[i++];
        }
    }

    size_t pos = 0;
    while (pos < src.size()) {
        size_t open  = src.find('{', pos);
        if (open == std::string::npos) break;
        size_t close = src.find('}', open);
        if (close == std::string::npos) break;

        std::string selector = trim(src.substr(pos, open - pos));
        std::string body     = src.substr(open+1, close-open-1);

        if (!selector.empty())
            parseRule(selector, body);

        pos = close + 1;
    }
}

bool CSSParser::parseFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        fprintf(stderr, "[CSS] Failed to open: %s\n", path.c_str());
        return false;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    parse(buf.str());
    fprintf(stderr, "[CSS] Loaded %s (%zu rules)\n", path.c_str(), m_rules.size());
    return true;
}

// ── Inline style parser ───────────────────────────────────────────────────────

CSSStyle CSSParser::parseInline(const std::string& inlineStyle) {
    CSSStyle s;
    CSSParser p;
    p.parseDeclarations(inlineStyle, s);
    return s;
}

// ── Merge: apply src on top of dst, only for explicitly-set properties ────────

static void mergeStyle(CSSStyle& dst, const CSSStyle& src) {
    if (src.set[P_BG])              { dst.backgroundColor=src.backgroundColor; dst.set[P_BG]=true; }
    if (src.set[P_BG_BLUR])         { dst.backgroundBlur=src.backgroundBlur;   dst.set[P_BG_BLUR]=true; }
    if (src.set[P_BORDER_COLOR])    { dst.borderColor=src.borderColor;          dst.set[P_BORDER_COLOR]=true; }
    if (src.set[P_BORDER_WIDTH])    { dst.borderWidth=src.borderWidth;          dst.set[P_BORDER_WIDTH]=true; }
    if (src.set[P_BORDER_RADIUS])   { dst.borderRadius=src.borderRadius;        dst.set[P_BORDER_RADIUS]=true; }
    if (src.set[P_BORDER_RADIUS_TL]){ dst.borderRadiusTL=src.borderRadiusTL;   dst.set[P_BORDER_RADIUS_TL]=true; }
    if (src.set[P_BORDER_RADIUS_TR]){ dst.borderRadiusTR=src.borderRadiusTR;   dst.set[P_BORDER_RADIUS_TR]=true; }
    if (src.set[P_BORDER_RADIUS_BL]){ dst.borderRadiusBL=src.borderRadiusBL;   dst.set[P_BORDER_RADIUS_BL]=true; }
    if (src.set[P_BORDER_RADIUS_BR]){ dst.borderRadiusBR=src.borderRadiusBR;   dst.set[P_BORDER_RADIUS_BR]=true; }
    if (src.set[P_BOX_SHADOW])      { dst.boxShadow=src.boxShadow;             dst.set[P_BOX_SHADOW]=true; }
    if (src.set[P_WIDTH])           { dst.width=src.width;                     dst.set[P_WIDTH]=true; }
    if (src.set[P_HEIGHT])          { dst.height=src.height;                   dst.set[P_HEIGHT]=true; }
    if (src.set[P_MIN_WIDTH])       { dst.minWidth=src.minWidth;               dst.set[P_MIN_WIDTH]=true; }
    if (src.set[P_MIN_HEIGHT])      { dst.minHeight=src.minHeight;             dst.set[P_MIN_HEIGHT]=true; }
    if (src.set[P_MAX_WIDTH])       { dst.maxWidth=src.maxWidth;               dst.set[P_MAX_WIDTH]=true; }
    if (src.set[P_MAX_HEIGHT])      { dst.maxHeight=src.maxHeight;             dst.set[P_MAX_HEIGHT]=true; }
    if (src.set[P_PADDING_T])       { dst.padding[0]=src.padding[0];           dst.set[P_PADDING_T]=true; }
    if (src.set[P_PADDING_R])       { dst.padding[1]=src.padding[1];           dst.set[P_PADDING_R]=true; }
    if (src.set[P_PADDING_B])       { dst.padding[2]=src.padding[2];           dst.set[P_PADDING_B]=true; }
    if (src.set[P_PADDING_L])       { dst.padding[3]=src.padding[3];           dst.set[P_PADDING_L]=true; }
    if (src.set[P_MARGIN_T])        { dst.margin[0]=src.margin[0];             dst.set[P_MARGIN_T]=true; }
    if (src.set[P_MARGIN_R])        { dst.margin[1]=src.margin[1];             dst.set[P_MARGIN_R]=true; }
    if (src.set[P_MARGIN_B])        { dst.margin[2]=src.margin[2];             dst.set[P_MARGIN_B]=true; }
    if (src.set[P_MARGIN_L])        { dst.margin[3]=src.margin[3];             dst.set[P_MARGIN_L]=true; }
    if (src.set[P_DISPLAY])         { dst.display=src.display;                 dst.set[P_DISPLAY]=true; }
    if (src.set[P_FLEX_DIR])        { dst.flexDirection=src.flexDirection;     dst.set[P_FLEX_DIR]=true; }
    if (src.set[P_JUSTIFY])         { dst.justifyContent=src.justifyContent;   dst.set[P_JUSTIFY]=true; }
    if (src.set[P_ALIGN_ITEMS])     { dst.alignItems=src.alignItems;           dst.set[P_ALIGN_ITEMS]=true; }
    if (src.set[P_GAP])             { dst.gap=src.gap;                         dst.set[P_GAP]=true; }
    if (src.set[P_FLEX])            { dst.flex=src.flex;                       dst.set[P_FLEX]=true; }
    if (src.set[P_COLOR])           { dst.color=src.color;                     dst.set[P_COLOR]=true; }
    if (src.set[P_FONT_SIZE])       { dst.fontSize=src.fontSize;               dst.set[P_FONT_SIZE]=true; }
    if (src.set[P_FONT_FAMILY])     { dst.fontFamily=src.fontFamily;           dst.set[P_FONT_FAMILY]=true; }
    if (src.set[P_FONT_WEIGHT])     { dst.fontWeight=src.fontWeight;           dst.set[P_FONT_WEIGHT]=true; }
    if (src.set[P_TEXT_ALIGN])      { dst.textAlign=src.textAlign;             dst.set[P_TEXT_ALIGN]=true; }
    if (src.set[P_LINE_HEIGHT])     { dst.lineHeight=src.lineHeight;           dst.set[P_LINE_HEIGHT]=true; }
    if (src.set[P_LETTER_SPACING])  { dst.letterSpacing=src.letterSpacing;     dst.set[P_LETTER_SPACING]=true; }
    if (src.set[P_OPACITY])         { dst.opacity=src.opacity;                 dst.set[P_OPACITY]=true; }
    if (src.set[P_CURSOR])          { dst.cursor=src.cursor;                   dst.set[P_CURSOR]=true; }
    if (src.set[P_OVERFLOW])        { dst.overflow=src.overflow;               dst.set[P_OVERFLOW]=true; }
    if (src.set[P_TRANSITION])      { dst.transitionDuration=src.transitionDuration; dst.set[P_TRANSITION]=true; }
    if (src.set[P_POSITION])        { dst.position=src.position;               dst.set[P_POSITION]=true; }
    if (src.set[P_TOP])             { dst.top=src.top;                         dst.set[P_TOP]=true; }
    if (src.set[P_LEFT])            { dst.left=src.left;                       dst.set[P_LEFT]=true; }
    if (src.set[P_RIGHT])           { dst.right=src.right;                     dst.set[P_RIGHT]=true; }
    if (src.set[P_BOTTOM])          { dst.bottom=src.bottom;                   dst.set[P_BOTTOM]=true; }
}

// ── Style resolver ────────────────────────────────────────────────────────────

static bool selectorMatches(const std::string& sel,
                             const std::string& tag,
                             const std::string& id,
                             const std::vector<std::string>& classes) {
    if (sel.empty() || sel=="*") return true;
    if (sel[0]=='#') return sel.substr(1)==id;
    if (sel[0]=='.') {
        std::string cls = sel.substr(1);
        return std::find(classes.begin(),classes.end(),cls)!=classes.end();
    }
    size_t dot = sel.find('.');
    if (dot!=std::string::npos) {
        std::string t   = sel.substr(0,dot);
        std::string cls = sel.substr(dot+1);
        bool tagOk  = (t.empty()||t==tag);
        bool clsOk  = std::find(classes.begin(),classes.end(),cls)!=classes.end();
        return tagOk && clsOk;
    }
    return sel==tag;
}

CSSStyle CSSParser::resolve(const std::string& tag,
                             const std::string& id,
                             const std::vector<std::string>& classes,
                             const std::string& pseudoClass) const {
    std::vector<const CSSRule*> matched;
    for (auto& rule : m_rules) {
        if (rule.pseudoClass != pseudoClass) continue;
        if (selectorMatches(rule.selector, tag, id, classes))
            matched.push_back(&rule);
    }
    std::stable_sort(matched.begin(), matched.end(),
        [](const CSSRule* a, const CSSRule* b){ return a->specificity < b->specificity; });

    CSSStyle result;
    for (auto* rule : matched)
        mergeStyle(result, rule->style);
    return result;
}

CSSStyle CSSParser::resolveWithPseudo(const std::string& tag,
                                       const std::string& id,
                                       const std::vector<std::string>& classes,
                                       const std::string& pseudoClass) const {
    // Start with base style, then overlay pseudo-class properties
    CSSStyle base = resolve(tag, id, classes, "");
    CSSStyle over = resolve(tag, id, classes, pseudoClass);
    mergeStyle(base, over);
    return base;
}

bool CSSParser::hasPseudo(const std::string& tag,
                           const std::string& id,
                           const std::vector<std::string>& classes,
                           const std::string& pseudoClass) const {
    for (auto& rule : m_rules) {
        if (rule.pseudoClass != pseudoClass) continue;
        if (selectorMatches(rule.selector, tag, id, classes))
            return true;
    }
    return false;
}

CSSStyle CSSParser::merge(const CSSStyle& dst, const CSSStyle& src) {
    CSSStyle result = dst;
    mergeStyle(result, src);
    return result;
}

CSSStyle CSSParser::mergeInline(const CSSStyle& base, const CSSStyle& inl) {
    CSSStyle result = base;
    mergeStyle(result, inl);
    return result;
}