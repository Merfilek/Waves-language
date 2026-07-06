// VouncyBridge.cpp — Bridges the WaveGUI native core (SDL2 + Blend2D) to
// Waves via the extern("Name", ...) convention. Every function here is
// callable from waves-src/Vouncy.waves as extern("GUI_XYZ", ...).
//
// Handle marshaling convention used throughout:
//   Native pointers (WaveGUI_Window, WaveGUI_Font) cross the Waves/C++
//   boundary as WAVES_LONG (int64), since WavesValue has no raw-pointer
//   type. The Waves-side wrapper classes (Vouncy.waves) store these as a
//   private `long _handle` field and pass it back in on every call.
//
// Build: see build.sh in this directory. Produces libVouncy.so, which
// must be discoverable by WaveVM/WaveCore at runtime (see build.sh notes
// on this — the exact loader convention needs to be confirmed against
// your toolchain).

#include "../WavesBridge.h"
#include "wavegui.h"
#include <cstdint>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------
// Handle <-> WavesValue helpers
// ---------------------------------------------------------------------

static inline WavesValue handleToValue(void* ptr) {
    return waves_makeLong(reinterpret_cast<int64_t>(ptr));
}

template <typename T>
static inline T handleFromValue(WavesValue v) {
    return reinterpret_cast<T>(static_cast<intptr_t>(waves_getLong(v)));
}

extern "C" {

// -----------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------

WavesValue waves_GUI_Init(int argc, WavesValue* argv) {
    (void)argc; (void)argv;
    return waves_makeBool(WaveGUI_Init());
}

WavesValue waves_GUI_Shutdown(int argc, WavesValue* argv) {
    (void)argc; (void)argv;
    WaveGUI_Shutdown();
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Window
// -----------------------------------------------------------------

// argv: [0]=title string, [1]=width int, [2]=height int, [3]=resizable bool
WavesValue waves_GUI_CreateWindow(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);

    WaveGUI_WindowDesc desc;
    desc.title        = WAVES_ARG_STRING(argv, 0);
    desc.width        = WAVES_ARG_INT(argv, 1);
    desc.height       = WAVES_ARG_INT(argv, 2);
    desc.resizable    = (argc > 3) ? WAVES_ARG_BOOL(argv, 3) : true;
    desc.highDpiAware = true;

    WaveGUI_Window win = WaveGUI_CreateWindow(&desc);
    if (!win) return waves_makeError("WaveGUI_CreateWindow failed");
    return handleToValue(win);
}

WavesValue waves_GUI_DestroyWindow(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_DestroyWindow(handleFromValue<WaveGUI_Window>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_GetWindowWidth(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    int32_t w, h;
    WaveGUI_GetWindowSize(handleFromValue<WaveGUI_Window>(argv[0]), &w, &h);
    return waves_makeInt(w);
}

WavesValue waves_GUI_GetWindowHeight(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    int32_t w, h;
    WaveGUI_GetWindowSize(handleFromValue<WaveGUI_Window>(argv[0]), &w, &h);
    return waves_makeInt(h);
}

WavesValue waves_GUI_GetWindowDpiScale(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeFloat(WaveGUI_GetWindowDpiScale(handleFromValue<WaveGUI_Window>(argv[0])));
}

WavesValue waves_GUI_SetWindowTitle(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_SetWindowTitle(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_STRING(argv, 1));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Timing
// -----------------------------------------------------------------

WavesValue waves_GUI_GetTicksMs(int argc, WavesValue* argv) {
    (void)argc; (void)argv;
    return waves_makeLong((int64_t)WaveGUI_GetTicksMs());
}

// argv: [0]=targetFps int, [1]=frameStartMs long
WavesValue waves_GUI_FrameDelay(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_FrameDelay((uint32_t)WAVES_ARG_INT(argv, 0), (uint64_t)waves_getLong(argv[1]));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Events
// -----------------------------------------------------------------

// Returns a "GUIEvent" Waves object, or WAVES_NULL if the queue is empty.
// See waves-src/Vouncy.waves for the matching class definition and the
// EventType constant table these int values map to.
WavesValue waves_GUI_PollEvent(int argc, WavesValue* argv) {
    (void)argc; (void)argv;

    WaveGUI_Event ev;
    if (!WaveGUI_PollEvent(&ev)) {
        return waves_makeNull();
    }

    WavesValue obj = waves_object_create("GUIEvent");
    waves_object_set_field(obj, "type",         waves_makeInt((int32_t)ev.type));
    waves_object_set_field(obj, "window",       handleToValue(ev.window));
    waves_object_set_field(obj, "mouseX",       waves_makeFloat(ev.mouseX));
    waves_object_set_field(obj, "mouseY",       waves_makeFloat(ev.mouseY));
    waves_object_set_field(obj, "mouseButton",  waves_makeInt(ev.mouseButton));
    waves_object_set_field(obj, "wheelDeltaY",  waves_makeFloat(ev.wheelDeltaY));
    waves_object_set_field(obj, "keyCode",      waves_makeInt(ev.keyCode));
    waves_object_set_field(obj, "modifiers",    waves_makeInt((int32_t)ev.modifiers));
    waves_object_set_field(obj, "text",         waves_makeString(ev.text));
    waves_object_set_field(obj, "width",        waves_makeInt(ev.width));
    waves_object_set_field(obj, "height",       waves_makeInt(ev.height));
    return obj;
}

// -----------------------------------------------------------------
// Frame begin/end
// -----------------------------------------------------------------

WavesValue waves_GUI_BeginFrame(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_BeginFrame(handleFromValue<WaveGUI_Window>(argv[0]), (uint32_t)waves_getLong(argv[1]));
    return waves_makeNull();
}

WavesValue waves_GUI_EndFrame(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_EndFrame(handleFromValue<WaveGUI_Window>(argv[0]));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Drawing primitives
// argv[0] is always the window handle. Colors are passed as WAVES_LONG
// (0xAARRGGBB) since that comfortably holds 32 bits unsigned.
// -----------------------------------------------------------------

WavesValue waves_GUI_FillRect(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 6);
    WaveGUI_FillRect(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2),
        WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        (uint32_t)waves_getLong(argv[5]));
    return waves_makeNull();
}

WavesValue waves_GUI_FillRoundRect(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 7);
    WaveGUI_FillRoundRect(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2),
        WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5), (uint32_t)waves_getLong(argv[6]));
    return waves_makeNull();
}

WavesValue waves_GUI_StrokeRoundRect(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 8);
    WaveGUI_StrokeRoundRect(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2),
        WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5), WAVES_ARG_FLOAT(argv, 6),
        (uint32_t)waves_getLong(argv[7]));
    return waves_makeNull();
}

WavesValue waves_GUI_DrawBoxShadow(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 9);
    WaveGUI_DrawBoxShadow(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2),
        WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5), WAVES_ARG_FLOAT(argv, 6), WAVES_ARG_FLOAT(argv, 7),
        (uint32_t)waves_getLong(argv[8]));
    return waves_makeNull();
}

WavesValue waves_GUI_FillRoundRectGradientV(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 8);
    WaveGUI_FillRoundRectGradientV(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2),
        WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5),
        (uint32_t)waves_getLong(argv[6]), (uint32_t)waves_getLong(argv[7]));
    return waves_makeNull();
}

WavesValue waves_GUI_FillCircle(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 5);
    WaveGUI_FillCircle(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3),
        (uint32_t)waves_getLong(argv[4]));
    return waves_makeNull();
}

WavesValue waves_GUI_PushClipRect(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 5);
    WaveGUI_PushClipRect(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2),
        WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4));
    return waves_makeNull();
}

WavesValue waves_GUI_PopClip(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_PopClip(handleFromValue<WaveGUI_Window>(argv[0]));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Text
// -----------------------------------------------------------------

WavesValue waves_GUI_LoadFont(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_Font font = WaveGUI_LoadFont(WAVES_ARG_STRING(argv, 0), WAVES_ARG_FLOAT(argv, 1));
    if (!font) return waves_makeError("WaveGUI_LoadFont failed");
    return handleToValue(font);
}

WavesValue waves_GUI_DestroyFont(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_DestroyFont(handleFromValue<WaveGUI_Font>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_DrawText(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 6);
    WaveGUI_DrawText(handleFromValue<WaveGUI_Window>(argv[0]),
        handleFromValue<WaveGUI_Font>(argv[1]),
        WAVES_ARG_STRING(argv, 2),
        WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        (uint32_t)waves_getLong(argv[5]));
    return waves_makeNull();
}

WavesValue waves_GUI_MeasureTextWidth(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    float w, h;
    WaveGUI_MeasureText(handleFromValue<WaveGUI_Font>(argv[0]), WAVES_ARG_STRING(argv, 1), &w, &h);
    return waves_makeFloat(w);
}

WavesValue waves_GUI_MeasureTextHeight(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    float w, h;
    WaveGUI_MeasureText(handleFromValue<WaveGUI_Font>(argv[0]), WAVES_ARG_STRING(argv, 1), &w, &h);
    return waves_makeFloat(h);
}

// -----------------------------------------------------------------
// Debug / tooling
// -----------------------------------------------------------------

WavesValue waves_GUI_SaveSnapshotPNG(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    bool ok = WaveGUI_SaveSnapshotPNG(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_STRING(argv, 1));
    return waves_makeBool(ok);
}

// -----------------------------------------------------------------
// Animation — Tween
// -----------------------------------------------------------------

// argv: [0]=from float, [1]=to float, [2]=durationMs float, [3]=easing int
WavesValue waves_GUI_CreateTween(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 4);
    WaveGUI_Tween t = WaveGUI_CreateTween(
        WAVES_ARG_FLOAT(argv, 0), WAVES_ARG_FLOAT(argv, 1),
        WAVES_ARG_FLOAT(argv, 2), (WaveGUI_Easing)WAVES_ARG_INT(argv, 3));
    return handleToValue(t);
}

WavesValue waves_GUI_DestroyTween(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_DestroyTween(handleFromValue<WaveGUI_Tween>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_TweenUpdate(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_TweenUpdate(handleFromValue<WaveGUI_Tween>(argv[0]), WAVES_ARG_FLOAT(argv, 1));
    return waves_makeNull();
}

// argv: [0]=tween, [1]=newTarget float, [2]=newDurationMs float (<=0 keeps existing)
WavesValue waves_GUI_TweenSetTarget(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_TweenSetTarget(handleFromValue<WaveGUI_Tween>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2));
    return waves_makeNull();
}

WavesValue waves_GUI_TweenGetValue(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeFloat(WaveGUI_TweenGetValue(handleFromValue<WaveGUI_Tween>(argv[0])));
}

WavesValue waves_GUI_TweenIsDone(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeBool(WaveGUI_TweenIsDone(handleFromValue<WaveGUI_Tween>(argv[0])));
}

// argv: [0]=colorA long, [1]=colorB long, [2]=t float
WavesValue waves_GUI_LerpColorARGB(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    uint32_t result = WaveGUI_LerpColorARGB(
        (uint32_t)waves_getLong(argv[0]), (uint32_t)waves_getLong(argv[1]), WAVES_ARG_FLOAT(argv, 2));
    return waves_makeLong((int64_t)result);
}

// -----------------------------------------------------------------
// Animation — Spring
// -----------------------------------------------------------------

// argv: [0]=initialValue float, [1]=stiffness float, [2]=damping float, [3]=mass float
WavesValue waves_GUI_CreateSpring(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 4);
    WaveGUI_Spring s = WaveGUI_CreateSpring(
        WAVES_ARG_FLOAT(argv, 0), WAVES_ARG_FLOAT(argv, 1),
        WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3));
    return handleToValue(s);
}

WavesValue waves_GUI_DestroySpring(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_DestroySpring(handleFromValue<WaveGUI_Spring>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_SpringSetTarget(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_SpringSetTarget(handleFromValue<WaveGUI_Spring>(argv[0]), WAVES_ARG_FLOAT(argv, 1));
    return waves_makeNull();
}

WavesValue waves_GUI_SpringSetValue(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_SpringSetValue(handleFromValue<WaveGUI_Spring>(argv[0]), WAVES_ARG_FLOAT(argv, 1));
    return waves_makeNull();
}

WavesValue waves_GUI_SpringUpdate(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_SpringUpdate(handleFromValue<WaveGUI_Spring>(argv[0]), WAVES_ARG_FLOAT(argv, 1));
    return waves_makeNull();
}

WavesValue waves_GUI_SpringGetValue(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeFloat(WaveGUI_SpringGetValue(handleFromValue<WaveGUI_Spring>(argv[0])));
}

WavesValue waves_GUI_SpringIsSettled(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeBool(WaveGUI_SpringIsSettled(handleFromValue<WaveGUI_Spring>(argv[0])));
}

// -----------------------------------------------------------------
// Window — size + resize
// -----------------------------------------------------------------

WavesValue waves_GUI_SetWindowSize(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_SetWindowSize(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_INT(argv, 1), WAVES_ARG_INT(argv, 2));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Cursor
// -----------------------------------------------------------------

WavesValue waves_GUI_SetCursor(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_SetCursor((WaveGUI_CursorShape)WAVES_ARG_INT(argv, 0));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Polled input state
// -----------------------------------------------------------------

WavesValue waves_GUI_GetMouseX(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    float x, y;
    WaveGUI_GetMousePos(handleFromValue<WaveGUI_Window>(argv[0]), &x, &y);
    return waves_makeFloat(x);
}

WavesValue waves_GUI_GetMouseY(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    float x, y;
    WaveGUI_GetMousePos(handleFromValue<WaveGUI_Window>(argv[0]), &x, &y);
    return waves_makeFloat(y);
}

WavesValue waves_GUI_IsMouseButtonDown(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeBool(WaveGUI_IsMouseButtonDown(WAVES_ARG_INT(argv, 0)));
}

WavesValue waves_GUI_IsKeyDown(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeBool(WaveGUI_IsKeyDown(WAVES_ARG_INT(argv, 0)));
}

WavesValue waves_GUI_PointInRect(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 6);
    bool inside = WaveGUI_PointInRect(
        WAVES_ARG_FLOAT(argv, 0), WAVES_ARG_FLOAT(argv, 1),
        WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3),
        WAVES_ARG_FLOAT(argv, 4), WAVES_ARG_FLOAT(argv, 5));
    return waves_makeBool(inside);
}

// -----------------------------------------------------------------
// Transform (translate for scroll composition)
// -----------------------------------------------------------------

WavesValue waves_GUI_PushTranslate(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_PushTranslate(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2));
    return waves_makeNull();
}

WavesValue waves_GUI_PopTransform(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_PopTransform(handleFromValue<WaveGUI_Window>(argv[0]));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Images
// -----------------------------------------------------------------

WavesValue waves_GUI_LoadImage(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_Image img = WaveGUI_LoadImage(WAVES_ARG_STRING(argv, 0));
    if (!img) return waves_makeError("WaveGUI_LoadImage failed");
    return handleToValue(img);
}

WavesValue waves_GUI_DestroyImage(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_DestroyImage(handleFromValue<WaveGUI_Image>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_GetImageWidth(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    int32_t w, h;
    WaveGUI_GetImageSize(handleFromValue<WaveGUI_Image>(argv[0]), &w, &h);
    return waves_makeInt(w);
}

WavesValue waves_GUI_GetImageHeight(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    int32_t w, h;
    WaveGUI_GetImageSize(handleFromValue<WaveGUI_Image>(argv[0]), &w, &h);
    return waves_makeInt(h);
}

WavesValue waves_GUI_DrawImage(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 6);
    WaveGUI_DrawImage(handleFromValue<WaveGUI_Window>(argv[0]), handleFromValue<WaveGUI_Image>(argv[1]),
        WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4), WAVES_ARG_FLOAT(argv, 5));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Text — wrapping + click-to-index
// -----------------------------------------------------------------

WavesValue waves_GUI_DrawTextWrapped(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 8);
    WaveGUI_DrawTextWrapped(handleFromValue<WaveGUI_Window>(argv[0]), handleFromValue<WaveGUI_Font>(argv[1]),
        WAVES_ARG_STRING(argv, 2), WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5), WAVES_ARG_FLOAT(argv, 6), (uint32_t)waves_getLong(argv[7]));
    return waves_makeNull();
}

WavesValue waves_GUI_MeasureTextWrappedWidth(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    float w, h;
    WaveGUI_MeasureTextWrapped(handleFromValue<WaveGUI_Font>(argv[0]), WAVES_ARG_STRING(argv, 1),
        WAVES_ARG_FLOAT(argv, 2), -1.0f, &w, &h);
    return waves_makeFloat(w);
}

WavesValue waves_GUI_MeasureTextWrappedHeight(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    float w, h;
    WaveGUI_MeasureTextWrapped(handleFromValue<WaveGUI_Font>(argv[0]), WAVES_ARG_STRING(argv, 1),
        WAVES_ARG_FLOAT(argv, 2), -1.0f, &w, &h);
    return waves_makeFloat(h);
}

WavesValue waves_GUI_TextIndexAtX(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    int32_t idx = WaveGUI_TextIndexAtX(handleFromValue<WaveGUI_Font>(argv[0]), WAVES_ARG_STRING(argv, 1), WAVES_ARG_FLOAT(argv, 2));
    return waves_makeInt(idx);
}

// -----------------------------------------------------------------
// Layout
// -----------------------------------------------------------------

WavesValue waves_GUI_CreateLayoutNode(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_LayoutNode n = WaveGUI_CreateLayoutNode((WaveGUI_LayoutDirection)WAVES_ARG_INT(argv, 0));
    return handleToValue(n);
}

WavesValue waves_GUI_DestroyLayoutNode(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_DestroyLayoutNode(handleFromValue<WaveGUI_LayoutNode>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutAddChild(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_LayoutAddChild(handleFromValue<WaveGUI_LayoutNode>(argv[0]), handleFromValue<WaveGUI_LayoutNode>(argv[1]));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutSetFixedSize(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_LayoutSetFixedSize(handleFromValue<WaveGUI_LayoutNode>(argv[0]), WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutSetFlexGrow(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_LayoutSetFlexGrow(handleFromValue<WaveGUI_LayoutNode>(argv[0]), WAVES_ARG_FLOAT(argv, 1));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutSetPadding(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 5);
    WaveGUI_LayoutSetPadding(handleFromValue<WaveGUI_LayoutNode>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutSetGap(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_LayoutSetGap(handleFromValue<WaveGUI_LayoutNode>(argv[0]), WAVES_ARG_FLOAT(argv, 1));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutSetAlign(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_LayoutSetAlign(handleFromValue<WaveGUI_LayoutNode>(argv[0]),
        (WaveGUI_MainAlign)WAVES_ARG_INT(argv, 1), (WaveGUI_CrossAlign)WAVES_ARG_INT(argv, 2));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutCompute(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_LayoutCompute(handleFromValue<WaveGUI_LayoutNode>(argv[0]), WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2));
    return waves_makeNull();
}

WavesValue waves_GUI_LayoutGetX(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    float x, y, w, h;
    WaveGUI_LayoutGetRect(handleFromValue<WaveGUI_LayoutNode>(argv[0]), &x, &y, &w, &h);
    return waves_makeFloat(x);
}

WavesValue waves_GUI_LayoutGetY(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    float x, y, w, h;
    WaveGUI_LayoutGetRect(handleFromValue<WaveGUI_LayoutNode>(argv[0]), &x, &y, &w, &h);
    return waves_makeFloat(y);
}

WavesValue waves_GUI_LayoutGetWidth(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    float x, y, w, h;
    WaveGUI_LayoutGetRect(handleFromValue<WaveGUI_LayoutNode>(argv[0]), &x, &y, &w, &h);
    return waves_makeFloat(w);
}

WavesValue waves_GUI_LayoutGetHeight(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    float x, y, w, h;
    WaveGUI_LayoutGetRect(handleFromValue<WaveGUI_LayoutNode>(argv[0]), &x, &y, &w, &h);
    return waves_makeFloat(h);
}

// -----------------------------------------------------------------
// Shapes
// -----------------------------------------------------------------

WavesValue waves_GUI_DrawLine(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 7);
    WaveGUI_DrawLine(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5), (uint32_t)waves_getLong(argv[6]));
    return waves_makeNull();
}

WavesValue waves_GUI_StrokeRect(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 7);
    WaveGUI_StrokeRect(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5), (uint32_t)waves_getLong(argv[6]));
    return waves_makeNull();
}

WavesValue waves_GUI_FillTriangle(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 7);
    WaveGUI_FillTriangle(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3),
        WAVES_ARG_FLOAT(argv, 4), WAVES_ARG_FLOAT(argv, 5), WAVES_ARG_FLOAT(argv, 6),
        (uint32_t)waves_getLong(argv[7]));
    return waves_makeNull();
}

// argv: [0]=window, [1]=xs array, [2]=ys array, [3]=color
WavesValue waves_GUI_FillPolygon(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 4);
    int n = waves_array_length(argv[1]);
    int nY = waves_array_length(argv[2]);
    if (n != nY) return waves_makeError("FillPolygon: xs and ys arrays must be the same length");

    std::vector<float> xs((size_t)n), ys((size_t)n);
    for (int i = 0; i < n; i++) {
        xs[(size_t)i] = waves_getFloat(waves_array_get(argv[1], i));
        ys[(size_t)i] = waves_getFloat(waves_array_get(argv[2], i));
    }
    WaveGUI_FillPolygon(handleFromValue<WaveGUI_Window>(argv[0]), xs.data(), ys.data(), n, (uint32_t)waves_getLong(argv[3]));
    return waves_makeNull();
}

WavesValue waves_GUI_FillRoundRectGradientH(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 8);
    WaveGUI_FillRoundRectGradientH(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3), WAVES_ARG_FLOAT(argv, 4),
        WAVES_ARG_FLOAT(argv, 5), (uint32_t)waves_getLong(argv[6]), (uint32_t)waves_getLong(argv[7]));
    return waves_makeNull();
}

WavesValue waves_GUI_FillRadialGradient(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 6);
    WaveGUI_FillRadialGradient(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), WAVES_ARG_FLOAT(argv, 3),
        (uint32_t)waves_getLong(argv[4]), (uint32_t)waves_getLong(argv[5]));
    return waves_makeNull();
}

// -----------------------------------------------------------------
// Message boxes
// -----------------------------------------------------------------

WavesValue waves_GUI_ShowMessageBox(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 4);
    WaveGUI_ShowMessageBox(handleFromValue<WaveGUI_Window>(argv[0]), (WaveGUI_MessageBoxType)WAVES_ARG_INT(argv, 1),
        WAVES_ARG_STRING(argv, 2), WAVES_ARG_STRING(argv, 3));
    return waves_makeNull();
}

WavesValue waves_GUI_ShowConfirmDialog(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 5);
    int32_t result = WaveGUI_ShowConfirmDialog(handleFromValue<WaveGUI_Window>(argv[0]),
        WAVES_ARG_STRING(argv, 1), WAVES_ARG_STRING(argv, 2), WAVES_ARG_STRING(argv, 3), WAVES_ARG_STRING(argv, 4));
    return waves_makeInt(result);
}

// -----------------------------------------------------------------
// File / folder dialogs
// -----------------------------------------------------------------

WavesValue waves_GUI_ShowOpenFileDialog(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeString(WaveGUI_ShowOpenFileDialog(WAVES_ARG_STRING(argv, 0)));
}

WavesValue waves_GUI_ShowSaveFileDialog(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeString(WaveGUI_ShowSaveFileDialog(WAVES_ARG_STRING(argv, 0)));
}

WavesValue waves_GUI_ShowOpenFolderDialog(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeString(WaveGUI_ShowOpenFolderDialog(WAVES_ARG_STRING(argv, 0)));
}

// -----------------------------------------------------------------
// Clipboard
// -----------------------------------------------------------------

WavesValue waves_GUI_SetClipboardText(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_SetClipboardText(WAVES_ARG_STRING(argv, 0));
    return waves_makeNull();
}

WavesValue waves_GUI_GetClipboardText(int argc, WavesValue* argv) {
    (void)argc; (void)argv;
    return waves_makeString(WaveGUI_GetClipboardText());
}

// -----------------------------------------------------------------
// Window polish
// -----------------------------------------------------------------

WavesValue waves_GUI_SetWindowMinSize(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_SetWindowMinSize(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_INT(argv, 1), WAVES_ARG_INT(argv, 2));
    return waves_makeNull();
}

WavesValue waves_GUI_SetWindowMaxSize(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    WaveGUI_SetWindowMaxSize(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_INT(argv, 1), WAVES_ARG_INT(argv, 2));
    return waves_makeNull();
}

WavesValue waves_GUI_SetWindowIcon(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_SetWindowIcon(handleFromValue<WaveGUI_Window>(argv[0]), handleFromValue<WaveGUI_Image>(argv[1]));
    return waves_makeNull();
}

WavesValue waves_GUI_SetWindowFullscreen(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_SetWindowFullscreen(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_BOOL(argv, 1));
    return waves_makeNull();
}

WavesValue waves_GUI_SetWindowAlwaysOnTop(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    WaveGUI_SetWindowAlwaysOnTop(handleFromValue<WaveGUI_Window>(argv[0]), WAVES_ARG_BOOL(argv, 1));
    return waves_makeNull();
}

WavesValue waves_GUI_CenterWindowOnScreen(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_CenterWindowOnScreen(handleFromValue<WaveGUI_Window>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_GetDisplayWidth(int argc, WavesValue* argv) {
    (void)argc; (void)argv;
    int32_t w, h;
    WaveGUI_GetDisplaySize(&w, &h);
    return waves_makeInt(w);
}

WavesValue waves_GUI_GetDisplayHeight(int argc, WavesValue* argv) {
    (void)argc; (void)argv;
    int32_t w, h;
    WaveGUI_GetDisplaySize(&w, &h);
    return waves_makeInt(h);
}

// -----------------------------------------------------------------
// Animation — Curve
// -----------------------------------------------------------------

WavesValue waves_GUI_CreateCurve(int argc, WavesValue* argv) {
    (void)argc; (void)argv;
    return handleToValue(WaveGUI_CreateCurve());
}

WavesValue waves_GUI_DestroyCurve(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_DestroyCurve(handleFromValue<WaveGUI_Curve>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_CurveAddKeyframe(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 4);
    WaveGUI_CurveAddKeyframe(handleFromValue<WaveGUI_Curve>(argv[0]),
        WAVES_ARG_FLOAT(argv, 1), WAVES_ARG_FLOAT(argv, 2), (WaveGUI_Easing)WAVES_ARG_INT(argv, 3));
    return waves_makeNull();
}

WavesValue waves_GUI_CurveClear(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    WaveGUI_CurveClear(handleFromValue<WaveGUI_Curve>(argv[0]));
    return waves_makeNull();
}

WavesValue waves_GUI_CurveEvaluate(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    return waves_makeFloat(WaveGUI_CurveEvaluate(handleFromValue<WaveGUI_Curve>(argv[0]), WAVES_ARG_FLOAT(argv, 1)));
}

WavesValue waves_GUI_CurveKeyframeCount(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeInt(WaveGUI_CurveKeyframeCount(handleFromValue<WaveGUI_Curve>(argv[0])));
}

WavesValue waves_GUI_CurveGetKeyframeTime(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    return waves_makeFloat(WaveGUI_CurveGetKeyframeTime(handleFromValue<WaveGUI_Curve>(argv[0]), WAVES_ARG_INT(argv, 1)));
}

WavesValue waves_GUI_CurveGetKeyframeValue(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    return waves_makeFloat(WaveGUI_CurveGetKeyframeValue(handleFromValue<WaveGUI_Curve>(argv[0]), WAVES_ARG_INT(argv, 1)));
}

WavesValue waves_GUI_CurveGetKeyframeEasing(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    return waves_makeInt(WaveGUI_CurveGetKeyframeEasing(handleFromValue<WaveGUI_Curve>(argv[0]), WAVES_ARG_INT(argv, 1)));
}

} // extern "C"