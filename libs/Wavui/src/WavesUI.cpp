#include "../WavesBridge.h"
#include "WindowManager.h"
#include "Widget.h"
#include "CSSParser.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <algorithm>

// ── Globals ───────────────────────────────────────────────────────────────────

static WindowManager* g_window = nullptr;

// Widget handle registry (id string → widget ptr)
static std::unordered_map<std::string, Widget::Ptr> g_widgetRegistry;

static std::string wval_to_str(WavesValue v) {
    if (waves_isString(v)) {
        const char* s = waves_getString(v);
        if (s == nullptr) return "";
        return std::string(s);
    }
    if (waves_isInt(v))    return std::to_string(waves_getInt(v));
    if (waves_isFloat(v))  return std::to_string(waves_getFloat(v));
    if (waves_isDouble(v)) return std::to_string(waves_getDouble(v));
    if (waves_isBool(v))   return waves_getBool(v) ? "true" : "false";
    return "";
}

static Widget::Ptr get_widget(WavesValue v) {
    std::string wid = wval_to_str(v);
    auto it = g_widgetRegistry.find(wid);
    if (it != g_widgetRegistry.end()) return it->second;
    // Try tree lookup
    if (g_window) {
        auto w = g_window->findById(wid);
        if (w) { g_widgetRegistry[wid] = w; return w; }
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// ── WINDOW FUNCTIONS ─────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// UI_CreateWindow(title, width, height) → bool
extern "C" WavesValue waves_UI_CreateWindow(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    if (g_window) {
        delete g_window;
        g_window = nullptr;
    }
    g_window = new WindowManager();
    WindowConfig cfg;
    cfg.title  = WAVES_ARG_STRING(argv, 0);
    cfg.width  = WAVES_ARG_INT(argv, 1);
    cfg.height = WAVES_ARG_INT(argv, 2);
    bool ok = g_window->create(cfg);
    if (!ok) { delete g_window; g_window = nullptr; }
    return waves_makeBool(ok);
}

// UI_CreateWindowEx(title, width, height, transparent, decorated, vsync) → bool
extern "C" WavesValue waves_UI_CreateWindowEx(int argc, WavesValue* argv) {
    fprintf(stderr, "[UI] argc=%d\n", argc);
    if (argc >= 1) fprintf(stderr, "[UI] title=%s\n", waves_getString(argv[0]));
    if (argc >= 2) fprintf(stderr, "[UI] w=%d h=%d\n", waves_getInt(argv[1]), waves_getInt(argv[2]));
    fflush(stderr);

    WAVES_REQUIRE_ARGS(argc, 6);
    if (g_window) { delete g_window; g_window = nullptr; }
    g_window = new WindowManager();
    WindowConfig cfg;
    cfg.title       = WAVES_ARG_STRING(argv, 0);
    cfg.width       = WAVES_ARG_INT   (argv, 1);
    cfg.height      = WAVES_ARG_INT   (argv, 2);
    cfg.transparent = WAVES_ARG_BOOL  (argv, 3);
    cfg.decorated   = WAVES_ARG_BOOL  (argv, 4);
    cfg.vsync       = WAVES_ARG_BOOL  (argv, 5);
    bool ok = g_window->create(cfg);
    if (!ok) { delete g_window; g_window = nullptr; }
    return waves_makeBool(ok);
}

// UI_SetWindowTitle(title)
extern "C" WavesValue waves_UI_SetWindowTitle(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    if (g_window) g_window->setTitle(WAVES_ARG_STRING(argv, 0));
    return waves_makeNull();
}

// UI_CloseWindow()
extern "C" WavesValue waves_UI_CloseWindow(int argc, WavesValue* argv) {
    if (g_window) g_window->close();
    return waves_makeNull();
}

// UI_Tick() → bool  (returns false when window should close)
extern "C" WavesValue waves_UI_Tick(int argc, WavesValue* argv) {
    if (!g_window) return waves_makeBool(false);
    bool open = g_window->tick();
    if (!open) { delete g_window; g_window = nullptr; }
    return waves_makeBool(open);
}

// UI_Run()  (blocks until window closes)
extern "C" WavesValue waves_UI_Run(int argc, WavesValue* argv) {
    if (g_window) {
        g_window->run();
        delete g_window;
        g_window = nullptr;
    }
    return waves_makeNull();
}

// UI_SetFrameCallback(callbackName)
extern "C" WavesValue waves_UI_SetFrameCallback(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    if (g_window) g_window->onFrameCallback = WAVES_ARG_STRING(argv, 0);
    return waves_makeNull();
}

// UI_LoadCSS(path) → bool
extern "C" WavesValue waves_UI_LoadCSS(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    if (!g_window) return waves_makeBool(false);
    return waves_makeBool(g_window->loadCSS(WAVES_ARG_STRING(argv, 0)));
}

// UI_LoadFont(name, path) → int (font id)
extern "C" WavesValue waves_UI_LoadFont(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    if (!g_window) return waves_makeInt(-1);
    return waves_makeInt(g_window->loadFont(WAVES_ARG_STRING(argv,0), WAVES_ARG_STRING(argv,1)));
}

// ─────────────────────────────────────────────────────────────────────────────
// ── WIDGET CREATION ───────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// UI_CreateWidget(tag, id) → widgetId (String)
extern "C" WavesValue waves_UI_CreateWidget(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    if (!g_window) return waves_makeError("No window");
    std::string tag = WAVES_ARG_STRING(argv, 0);
    std::string id  = WAVES_ARG_STRING(argv, 1);
    auto w = createWidget(tag);
    w->id  = id;
    g_widgetRegistry[id] = w;
    return waves_makeString(id.c_str());
}

// UI_AddToRoot(widgetId)
extern "C" WavesValue waves_UI_AddToRoot(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    if (!g_window) return waves_makeError("No window");
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    g_window->root()->addChild(w);
    return waves_makeNull();
}

// UI_AddChild(parentId, childId)
extern "C" WavesValue waves_UI_AddChild(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto parent = get_widget(argv[0]);
    auto child  = get_widget(argv[1]);
    if (!parent) return waves_makeError("Parent not found");
    if (!child)  return waves_makeError("Child not found");
    parent->addChild(child);
    return waves_makeNull();
}

// UI_RemoveChild(parentId, childId)
extern "C" WavesValue waves_UI_RemoveChild(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto parent = get_widget(argv[0]);
    if (!parent) return waves_makeError("Parent not found");
    parent->removeChild(wval_to_str(argv[1]));
    return waves_makeNull();
}

// ─────────────────────────────────────────────────────────────────────────────
// ── WIDGET PROPERTIES ─────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// UI_SetText(widgetId, text)
extern "C" WavesValue waves_UI_SetText(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->text = wval_to_str(argv[1]);
    return waves_makeNull();
}

// UI_GetText(widgetId) → String
extern "C" WavesValue waves_UI_GetText(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    return waves_makeString(w->text.c_str());
}

// UI_SetValue(widgetId, value)  — for inputs
extern "C" WavesValue waves_UI_SetValue(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->value = wval_to_str(argv[1]);
    if (w->tag=="input") static_cast<InputWidget*>(w.get())->cursorPos=(int)w->value.size();
    return waves_makeNull();
}

// UI_GetValue(widgetId) → String
extern "C" WavesValue waves_UI_GetValue(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    return waves_makeString(w->value.c_str());
}

// UI_SetPlaceholder(widgetId, placeholder)
extern "C" WavesValue waves_UI_SetPlaceholder(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w || w->tag!="input") return waves_makeError("Not an input widget");
    static_cast<InputWidget*>(w.get())->placeholder = wval_to_str(argv[1]);
    return waves_makeNull();
}

// UI_SetInputType(widgetId, type)  — "text" | "password" | "number"
extern "C" WavesValue waves_UI_SetInputType(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w || w->tag!="input") return waves_makeError("Not an input widget");
    static_cast<InputWidget*>(w.get())->inputType = wval_to_str(argv[1]);
    return waves_makeNull();
}

// UI_SetVisible(widgetId, bool)
extern "C" WavesValue waves_UI_SetVisible(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->visible = WAVES_ARG_BOOL(argv, 1);
    return waves_makeNull();
}

// UI_SetDisabled(widgetId, bool)
extern "C" WavesValue waves_UI_SetDisabled(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->disabled = WAVES_ARG_BOOL(argv, 1);
    return waves_makeNull();
}

// ─────────────────────────────────────────────────────────────────────────────
// ── STYLING ───────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// UI_SetClass(widgetId, className)  — replaces all classes
extern "C" WavesValue waves_UI_SetClass(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->classes = { wval_to_str(argv[1]) };
    return waves_makeNull();
}

// UI_AddClass(widgetId, className)
extern "C" WavesValue waves_UI_AddClass(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    std::string cls = wval_to_str(argv[1]);
    if (std::find(w->classes.begin(), w->classes.end(), cls) == w->classes.end())
        w->classes.push_back(cls);
    return waves_makeNull();
}

// UI_RemoveClass(widgetId, className)
extern "C" WavesValue waves_UI_RemoveClass(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    std::string cls = wval_to_str(argv[1]);
    w->classes.erase(std::remove(w->classes.begin(), w->classes.end(), cls), w->classes.end());
    return waves_makeNull();
}

// UI_SetStyle(widgetId, inlineCSS)  — e.g. "background: red; padding: 8px"
extern "C" WavesValue waves_UI_SetStyle(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->inlineStyle = wval_to_str(argv[1]);
    return waves_makeNull();
}

// UI_SetImageSrc(widgetId, path)
extern "C" WavesValue waves_UI_SetImageSrc(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    if (!g_window) return waves_makeError("No window");
    auto w = get_widget(argv[0]);
    if (!w || w->tag!="img") return waves_makeError("Not an image widget");
    std::string path = wval_to_str(argv[1]);
    auto* img = static_cast<ImageWidget*>(w.get());
    img->src = path;
    img->nvgImageHandle = g_window->renderer().loadImage(path);
    return waves_makeNull();
}

// ─────────────────────────────────────────────────────────────────────────────
// ── EVENTS ────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// UI_On(widgetId, eventName, callbackName)
// e.g. UI_On("myBtn", "click", "handleClick")
extern "C" WavesValue waves_UI_On(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->eventHandlers[wval_to_str(argv[1])] = wval_to_str(argv[2]);
    return waves_makeNull();
}

// UI_Off(widgetId, eventName)
extern "C" WavesValue waves_UI_Off(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeError("Widget not found");
    w->eventHandlers.erase(wval_to_str(argv[1]));
    return waves_makeNull();
}

// ─────────────────────────────────────────────────────────────────────────────
// ── LAYOUT QUERIES ────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// UI_GetBounds(widgetId) → Object {x, y, width, height}
extern "C" WavesValue waves_UI_GetBounds(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto w = get_widget(argv[0]);
    if (!w || !waves_object_create) return waves_makeError("Widget not found");
    WavesValue obj = waves_object_create("Bounds");
    waves_object_set_field(obj, "x",      waves_makeFloat(w->box.x));
    waves_object_set_field(obj, "y",      waves_makeFloat(w->box.y));
    waves_object_set_field(obj, "width",  waves_makeFloat(w->box.w));
    waves_object_set_field(obj, "height", waves_makeFloat(w->box.h));
    return obj;
}

// UI_IsHovered(widgetId) → Bool
extern "C" WavesValue waves_UI_IsHovered(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeBool(false);
    return waves_makeBool(w->hovered);
}

// UI_IsFocused(widgetId) → Bool
extern "C" WavesValue waves_UI_IsFocused(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto w = get_widget(argv[0]);
    if (!w) return waves_makeBool(false);
    return waves_makeBool(w->focused);
}

// ─────────────────────────────────────────────────────────────────────────────
// ── CSS INJECTION AT RUNTIME ──────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// UI_ParseCSS(cssString)  — inject CSS string at runtime
extern "C" WavesValue waves_UI_ParseCSS(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    if (!g_window) return waves_makeError("No window");
    g_window->cssParser().parse(wval_to_str(argv[0]));
    return waves_makeNull();
}