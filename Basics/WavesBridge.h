// WavesBridge.h - C-compatible bridge interface for Waves Language
// Copy this file to your library directory and include it in your C++ bridge code.
#ifndef WAVES_BRIDGE_H
#define WAVES_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// ── Value Type Enum ───────────────────────────────────────────────────────────
typedef enum {
    WAVES_NULL      = 0,
    WAVES_INT       = 1,
    WAVES_LONG      = 2,
    WAVES_DOUBLE    = 3,
    WAVES_BOOL      = 4,
    WAVES_STRING    = 5,
    WAVES_ARRAY     = 6,
    WAVES_OBJECT    = 7,
    WAVES_ERROR     = 8,
    WAVES_CHAR      = 9,
    WAVES_FLOAT     = 10,
} WavesValueType;

// ── Opaque handles for complex types ─────────────────────────────────────────
typedef struct WavesArrayHandle  WavesArrayHandle;
typedef struct WavesObjectHandle WavesObjectHandle;

// ── Value Structure ───────────────────────────────────────────────────────────
typedef struct {
    WavesValueType type;
    union {
        int32_t           intVal;
        int64_t           longVal;
        double            doubleVal;
        float             floatVal;
        int32_t           boolVal;
        char              charVal;
        const char*       stringVal;    // Points to VM-managed memory
        WavesArrayHandle* arrayVal;     // Opaque array handle
        WavesObjectHandle* objectVal;   // Opaque object handle
        const char*       errorMsg;     // Error message
    } data;
} WavesValue;

// ── Shared variable registry ──────────────────────────────────────────────────
// Allows C++ and Waves to share variables by name
typedef struct {
    void (*set)(const char* name, WavesValue value);
    WavesValue (*get)(const char* name);
    int (*has)(const char* name);
} WavesSharedVars;

extern WavesSharedVars* waves_shared;

// ── Callback registry ─────────────────────────────────────────────────────────
// Call Waves functions from C++
typedef WavesValue (*WavesCallback)(int argc, WavesValue* argv);

typedef struct {
    void (*register_cb)(const char* name, WavesCallback cb);
    WavesValue (*invoke)(const char* name, int argc, WavesValue* argv);
    int (*has)(const char* name);
} WavesCallbacks;

extern WavesCallbacks* waves_callbacks;

// ── Make functions ────────────────────────────────────────────────────────────

static inline WavesValue waves_makeNull() {
    WavesValue v = {};
    v.type = WAVES_NULL;
    return v;
}

static inline WavesValue waves_makeInt(int32_t val) {
    WavesValue v = {};
    v.type = WAVES_INT;
    v.data.intVal = val;
    return v;
}

static inline WavesValue waves_makeLong(int64_t val) {
    WavesValue v = {};
    v.type = WAVES_LONG;
    v.data.longVal = val;
    return v;
}

static inline WavesValue waves_makeDouble(double val) {
    WavesValue v = {};
    v.type = WAVES_DOUBLE;
    v.data.doubleVal = val;
    return v;
}

static inline WavesValue waves_makeFloat(float val) {
    WavesValue v = {};
    v.type = WAVES_FLOAT;
    v.data.floatVal = val;
    return v;
}

static inline WavesValue waves_makeBool(int val) {
    WavesValue v = {};
    v.type = WAVES_BOOL;
    v.data.boolVal = val ? 1 : 0;
    return v;
}

static inline WavesValue waves_makeChar(char val) {
    WavesValue v = {};
    v.type = WAVES_CHAR;
    v.data.charVal = val;
    return v;
}

// waves_makeString does NOT copy `val` — it just wraps the pointer you give it.
// That pointer must stay valid until the VM has processed the return value (it
// interns its own copy at that point, so nothing needs to stay valid after the
// call into Waves returns). Safe to use directly for:
//   - string literals ("like this")
//   - a buffer you own and are intentionally leaking/transferring to the VM
//     (e.g. malloc'd and never freed on your side)
// NOT safe for a local std::string's .c_str() or any other buffer that gets
// freed/destroyed as part of your own function returning — a std::string's
// destructor runs as part of YOUR function's return sequence, before the VM
// ever sees the value, so by the time it's read the pointer is already
// dangling. Use waves_makeStringOwned() for those cases instead.
static inline WavesValue waves_makeString(const char* val) {
    WavesValue v = {};
    v.type = WAVES_STRING;
    v.data.stringVal = val;
    return v;
}

// Like waves_makeString, but takes its own heap copy (via strdup) first, so it's
// always safe regardless of what happens to `val` after this call returns — the
// one you need for `return waves_makeStringOwned(myString.c_str());` or any other
// locally-built string. The copy is intentionally never freed on your side: the
// VM interns its own copy when it reads the value, same trade-off Waves' own
// string handling already makes everywhere else (see WaveCore's wc_intern) —
// one small one-time allocation per call, not worth tracking down and freeing.
static inline WavesValue waves_makeStringOwned(const char* val) {
    WavesValue v = {};
    v.type = WAVES_STRING;
    if (val) {
        // Plain malloc+memcpy rather than strdup: strdup is POSIX, not ISO C, so
        // it isn't guaranteed to be declared under a strict -std=c11 build without
        // the consuming file defining its own feature-test macros first — this
        // avoids forcing that requirement on every file that includes this header.
        size_t len = strlen(val);
        char* copy = (char*)malloc(len + 1);
        if (copy) memcpy(copy, val, len + 1);
        v.data.stringVal = copy;
    } else {
        v.data.stringVal = NULL;
    }
    return v;
}

static inline WavesValue waves_makeError(const char* message) {
    WavesValue v = {};
    v.type = WAVES_ERROR;
    v.data.errorMsg = message;
    return v;
}

// ── Array functions ───────────────────────────────────────────────────────────

// These are implemented by the VM and set at startup
extern WavesValue (*waves_array_create)(int capacity);
extern void       (*waves_array_push)(WavesValue arr, WavesValue element);
extern WavesValue (*waves_array_get)(WavesValue arr, int index);
extern void       (*waves_array_set)(WavesValue arr, int index, WavesValue value);
extern int        (*waves_array_length)(WavesValue arr);

// ── Object functions ──────────────────────────────────────────────────────────

// These are implemented by the VM and set at startup
extern WavesValue (*waves_object_create)(const char* className);
extern void       (*waves_object_set_field)(WavesValue obj, const char* field, WavesValue value);
extern WavesValue (*waves_object_get_field)(WavesValue obj, const char* field);
extern int        (*waves_object_has_field)(WavesValue obj, const char* field);
extern const char*(*waves_object_class_name)(WavesValue obj);

// ── Get functions (with type checking) ───────────────────────────────────────

static inline int32_t waves_getInt(WavesValue v) {
    if (v.type == WAVES_INT)    return v.data.intVal;
    if (v.type == WAVES_DOUBLE) return (int32_t)v.data.doubleVal;
    if (v.type == WAVES_FLOAT)  return (int32_t)v.data.floatVal;
    if (v.type == WAVES_LONG)   return (int32_t)v.data.longVal;
    return 0;
}

static inline int64_t waves_getLong(WavesValue v) {
    if (v.type == WAVES_LONG)   return v.data.longVal;
    if (v.type == WAVES_INT)    return (int64_t)v.data.intVal;
    if (v.type == WAVES_DOUBLE) return (int64_t)v.data.doubleVal;
    return 0;
}

static inline double waves_getDouble(WavesValue v) {
    if (v.type == WAVES_DOUBLE) return v.data.doubleVal;
    if (v.type == WAVES_FLOAT)  return (double)v.data.floatVal;
    if (v.type == WAVES_INT)    return (double)v.data.intVal;
    if (v.type == WAVES_LONG)   return (double)v.data.longVal;
    return 0.0;
}

static inline float waves_getFloat(WavesValue v) {
    if (v.type == WAVES_FLOAT)  return v.data.floatVal;
    if (v.type == WAVES_DOUBLE) return (float)v.data.doubleVal;
    if (v.type == WAVES_INT)    return (float)v.data.intVal;
    return 0.0f;
}

static inline int waves_getBool(WavesValue v) {
    if (v.type == WAVES_BOOL) return v.data.boolVal;
    if (v.type == WAVES_INT)  return v.data.intVal != 0;
    if (v.type == WAVES_NULL) return 0;
    return 1;
}

static inline char waves_getChar(WavesValue v) {
    if (v.type == WAVES_CHAR)   return v.data.charVal;
    if (v.type == WAVES_STRING && v.data.stringVal) return v.data.stringVal[0];
    return '\0';
}

static inline const char* waves_getString(WavesValue v) {
    if (v.type == WAVES_STRING) return v.data.stringVal;
    if (v.type == WAVES_ERROR)  return v.data.errorMsg;
    return "";
}

// ── Equality ──────────────────────────────────────────────────────────────────
// Value equality by type+content — the comparison any Dictionary/Set/cache-style
// library needs for its keys. No two libraries should have to reimplement this;
// promoted here after Generics' Helper.cpp had its own copy. Only meaningful for
// scalar types (numbers, bool, char, string, null) — WAVES_ARRAY/WAVES_OBJECT
// compare by handle identity (same array/object instance), not deep equality.
static inline int waves_valuesEqual(WavesValue a, WavesValue b) {
    if (a.type != b.type) return 0;
    switch (a.type) {
        case WAVES_NULL:   return 1;
        case WAVES_INT:    return a.data.intVal    == b.data.intVal;
        case WAVES_LONG:   return a.data.longVal   == b.data.longVal;
        case WAVES_BOOL:   return a.data.boolVal   == b.data.boolVal;
        case WAVES_DOUBLE: return a.data.doubleVal == b.data.doubleVal;
        case WAVES_FLOAT:  return a.data.floatVal  == b.data.floatVal;
        case WAVES_CHAR:   return a.data.charVal   == b.data.charVal;
        case WAVES_STRING:
            return a.data.stringVal && b.data.stringVal &&
                   strcmp(a.data.stringVal, b.data.stringVal) == 0;
        case WAVES_ARRAY:  return a.data.arrayVal  == b.data.arrayVal;
        case WAVES_OBJECT: return a.data.objectVal == b.data.objectVal;
        default: return 0;
    }
}

// ── Type checking ─────────────────────────────────────────────────────────────

static inline int waves_isNull(WavesValue v)   { return v.type == WAVES_NULL; }
static inline int waves_isInt(WavesValue v)    { return v.type == WAVES_INT; }
static inline int waves_isLong(WavesValue v)   { return v.type == WAVES_LONG; }
static inline int waves_isDouble(WavesValue v) { return v.type == WAVES_DOUBLE; }
static inline int waves_isFloat(WavesValue v)  { return v.type == WAVES_FLOAT; }
static inline int waves_isBool(WavesValue v)   { return v.type == WAVES_BOOL; }
static inline int waves_isChar(WavesValue v)   { return v.type == WAVES_CHAR; }
static inline int waves_isString(WavesValue v) { return v.type == WAVES_STRING; }
static inline int waves_isArray(WavesValue v)  { return v.type == WAVES_ARRAY; }
static inline int waves_isObject(WavesValue v) { return v.type == WAVES_OBJECT; }
static inline int waves_isError(WavesValue v)  { return v.type == WAVES_ERROR; }
static inline int waves_isNumeric(WavesValue v) {
    return v.type == WAVES_INT || v.type == WAVES_LONG ||
           v.type == WAVES_DOUBLE || v.type == WAVES_FLOAT;
}

// ── Convenience macros ────────────────────────────────────────────────────────

// Check arg count and return null if not enough
#define WAVES_REQUIRE_ARGS(argc, n) \
    if ((argc) < (n)) return waves_makeError("Not enough arguments: expected " #n)

// Check arg type
#define WAVES_REQUIRE_TYPE(argv, i, t) \
    if ((argv)[i].type != (t)) return waves_makeError("Wrong argument type at index " #i)

// Safe array access
#define WAVES_ARG_INT(argv, i)    waves_getInt((argv)[i])
#define WAVES_ARG_DOUBLE(argv, i) waves_getDouble((argv)[i])
#define WAVES_ARG_FLOAT(argv, i)  waves_getFloat((argv)[i])
#define WAVES_ARG_BOOL(argv, i)   waves_getBool((argv)[i])
#define WAVES_ARG_STRING(argv, i) waves_getString((argv)[i])
#define WAVES_ARG_CHAR(argv, i)   waves_getChar((argv)[i])

#ifdef __cplusplus
}
#endif

#endif // WAVES_BRIDGE_H