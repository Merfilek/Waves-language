#include "WavesBridge.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <algorithm>

// ── Key helpers ───────────────────────────────────────────────────────────────
// Dictionary and Set need to hash WavesValues. We stringify the value for the
// hash key, prefixed with a type tag so int 1 and string "1" don't collide.

static std::string to_key(WavesValue v) {
    switch (v.type) {
        case WAVES_INT:    return "i:" + std::to_string(v.data.intVal);
        case WAVES_LONG:   return "l:" + std::to_string(v.data.longVal);
        case WAVES_BOOL:   return std::string("b:") + (v.data.boolVal ? "1" : "0");
        case WAVES_DOUBLE: return "d:" + std::to_string(v.data.doubleVal);
        case WAVES_FLOAT:  return "f:" + std::to_string(v.data.floatVal);
        case WAVES_CHAR:   return std::string("c:") + v.data.charVal;
        case WAVES_STRING: return v.data.stringVal ? ("s:" + std::string(v.data.stringVal)) : "s:";
        case WAVES_NULL:   return "null";
        default:           return "p:" + std::to_string((intptr_t)v.data.objectVal);
    }
}

static bool values_equal(WavesValue a, WavesValue b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case WAVES_INT:    return a.data.intVal    == b.data.intVal;
        case WAVES_LONG:   return a.data.longVal   == b.data.longVal;
        case WAVES_BOOL:   return a.data.boolVal   == b.data.boolVal;
        case WAVES_DOUBLE: return a.data.doubleVal == b.data.doubleVal;
        case WAVES_FLOAT:  return a.data.floatVal  == b.data.floatVal;
        case WAVES_CHAR:   return a.data.charVal   == b.data.charVal;
        case WAVES_NULL:   return true;
        case WAVES_STRING:
            return a.data.stringVal && b.data.stringVal &&
                   strcmp(a.data.stringVal, b.data.stringVal) == 0;
        default: return false;
    }
}

// ── Macro: extract C++ pointer from argv[idx] ─────────────────────────────────
// The handle is stored as WAVES_LONG whose longVal is reinterpret_cast<intptr_t>(ptr).
#define HANDLE(T, argv, idx) reinterpret_cast<T*>((intptr_t)waves_getLong((argv)[(idx)]))

// ── Container structs ─────────────────────────────────────────────────────────

struct WavesList {
    std::vector<WavesValue> items;
};

struct WavesDict {
    // map: stringified key -> value
    std::unordered_map<std::string, WavesValue> map;
    // original key values in insertion order (for keys()/values())
    std::vector<std::pair<std::string, WavesValue>> ordered;
};

struct WavesStack {
    std::vector<WavesValue> items;
};

struct WavesSet {
    std::unordered_set<std::string> keys;
    // keep original WavesValues so toArray() can return them
    std::unordered_map<std::string, WavesValue> originals;
};

extern "C" {

// ════════════════════════════════════════════════════════════════════════
// LIST
// ════════════════════════════════════════════════════════════════════════

WavesValue waves_ListCreate(int argc, WavesValue* argv) {
    return waves_makeLong((int64_t)(intptr_t)(new WavesList()));
}

WavesValue waves_ListAdd(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    HANDLE(WavesList, argv, 0)->items.push_back(argv[1]);
    return waves_makeNull();
}

WavesValue waves_ListInsert(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    auto* list = HANDLE(WavesList, argv, 0);
    int idx = waves_getInt(argv[1]);
    if (idx < 0 || idx > (int)list->items.size())
        return waves_makeError("List.insert: index out of range");
    list->items.insert(list->items.begin() + idx, argv[2]);
    return waves_makeNull();
}

WavesValue waves_ListGet(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto* list = HANDLE(WavesList, argv, 0);
    int idx = waves_getInt(argv[1]);
    if (idx < 0 || idx >= (int)list->items.size())
        return waves_makeError("List.get: index out of range");
    return list->items[idx];
}

WavesValue waves_ListSet(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    auto* list = HANDLE(WavesList, argv, 0);
    int idx = waves_getInt(argv[1]);
    if (idx < 0 || idx >= (int)list->items.size())
        return waves_makeError("List.set: index out of range");
    list->items[idx] = argv[2];
    return waves_makeNull();
}

WavesValue waves_ListRemove(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto* list = HANDLE(WavesList, argv, 0);
    int idx = waves_getInt(argv[1]);
    if (idx < 0 || idx >= (int)list->items.size())
        return waves_makeError("List.remove: index out of range");
    list->items.erase(list->items.begin() + idx);
    return waves_makeNull();
}

WavesValue waves_ListCount(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeInt((int32_t)HANDLE(WavesList, argv, 0)->items.size());
}

WavesValue waves_ListContains(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    for (auto& item : HANDLE(WavesList, argv, 0)->items)
        if (values_equal(item, argv[1])) return waves_makeBool(1);
    return waves_makeBool(0);
}

WavesValue waves_ListIndexOf(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto& items = HANDLE(WavesList, argv, 0)->items;
    for (int i = 0; i < (int)items.size(); i++)
        if (values_equal(items[i], argv[1])) return waves_makeInt(i);
    return waves_makeInt(-1);
}

WavesValue waves_ListClear(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    HANDLE(WavesList, argv, 0)->items.clear();
    return waves_makeNull();
}

WavesValue waves_ListDestroy(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    delete HANDLE(WavesList, argv, 0);
    return waves_makeNull();
}

// ════════════════════════════════════════════════════════════════════════
// DICTIONARY
// ════════════════════════════════════════════════════════════════════════

WavesValue waves_DictCreate(int argc, WavesValue* argv) {
    return waves_makeLong((int64_t)(intptr_t)(new WavesDict()));
}

WavesValue waves_DictSet(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 3);
    auto* d = HANDLE(WavesDict, argv, 0);
    std::string k = to_key(argv[1]);
    bool is_new = (d->map.find(k) == d->map.end());
    d->map[k] = argv[2];
    if (is_new)
        d->ordered.push_back({k, argv[1]});
    else {
        // update key's original value in ordered list
        for (auto& p : d->ordered)
            if (p.first == k) { p.second = argv[1]; break; }
    }
    return waves_makeNull();
}

WavesValue waves_DictGet(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto* d = HANDLE(WavesDict, argv, 0);
    auto it = d->map.find(to_key(argv[1]));
    return (it != d->map.end()) ? it->second : waves_makeNull();
}

WavesValue waves_DictHas(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto* d = HANDLE(WavesDict, argv, 0);
    return waves_makeBool(d->map.count(to_key(argv[1])) > 0);
}

WavesValue waves_DictRemove(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto* d = HANDLE(WavesDict, argv, 0);
    std::string k = to_key(argv[1]);
    d->map.erase(k);
    d->ordered.erase(
        std::remove_if(d->ordered.begin(), d->ordered.end(),
            [&](const std::pair<std::string,WavesValue>& p){ return p.first == k; }),
        d->ordered.end());
    return waves_makeNull();
}

WavesValue waves_DictCount(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeInt((int32_t)HANDLE(WavesDict, argv, 0)->map.size());
}

WavesValue waves_DictClear(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto* d = HANDLE(WavesDict, argv, 0);
    d->map.clear();
    d->ordered.clear();
    return waves_makeNull();
}

// Returns a Waves array of all keys in insertion order
WavesValue waves_DictKeys(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto* d = HANDLE(WavesDict, argv, 0);
    WavesValue arr = waves_array_create((int)d->ordered.size());
    for (auto& p : d->ordered)
        waves_array_push(arr, p.second);
    return arr;
}

// Returns a Waves array of all values in insertion order
WavesValue waves_DictValues(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto* d = HANDLE(WavesDict, argv, 0);
    WavesValue arr = waves_array_create((int)d->ordered.size());
    for (auto& p : d->ordered)
        waves_array_push(arr, d->map[p.first]);
    return arr;
}

WavesValue waves_DictDestroy(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    delete HANDLE(WavesDict, argv, 0);
    return waves_makeNull();
}

// ════════════════════════════════════════════════════════════════════════
// STACK
// ════════════════════════════════════════════════════════════════════════

WavesValue waves_StackCreate(int argc, WavesValue* argv) {
    return waves_makeLong((int64_t)(intptr_t)(new WavesStack()));
}

WavesValue waves_StackPush(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    HANDLE(WavesStack, argv, 0)->items.push_back(argv[1]);
    return waves_makeNull();
}

WavesValue waves_StackPop(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto* s = HANDLE(WavesStack, argv, 0);
    if (s->items.empty()) return waves_makeError("Stack.pop: empty");
    WavesValue top = s->items.back();
    s->items.pop_back();
    return top;
}

WavesValue waves_StackPeek(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto* s = HANDLE(WavesStack, argv, 0);
    if (s->items.empty()) return waves_makeError("Stack.peek: empty");
    return s->items.back();
}

WavesValue waves_StackIsEmpty(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeBool(HANDLE(WavesStack, argv, 0)->items.empty());
}

WavesValue waves_StackCount(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeInt((int32_t)HANDLE(WavesStack, argv, 0)->items.size());
}

WavesValue waves_StackClear(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    HANDLE(WavesStack, argv, 0)->items.clear();
    return waves_makeNull();
}

WavesValue waves_StackDestroy(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    delete HANDLE(WavesStack, argv, 0);
    return waves_makeNull();
}

// ════════════════════════════════════════════════════════════════════════
// SET
// ════════════════════════════════════════════════════════════════════════

WavesValue waves_SetCreate(int argc, WavesValue* argv) {
    return waves_makeLong((int64_t)(intptr_t)(new WavesSet()));
}

WavesValue waves_SetAdd(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto* s = HANDLE(WavesSet, argv, 0);
    std::string k = to_key(argv[1]);
    s->keys.insert(k);
    s->originals[k] = argv[1];
    return waves_makeNull();
}

WavesValue waves_SetContains(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    return waves_makeBool(HANDLE(WavesSet, argv, 0)->keys.count(to_key(argv[1])) > 0);
}

WavesValue waves_SetRemove(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    auto* s = HANDLE(WavesSet, argv, 0);
    std::string k = to_key(argv[1]);
    s->keys.erase(k);
    s->originals.erase(k);
    return waves_makeNull();
}

WavesValue waves_SetCount(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeInt((int32_t)HANDLE(WavesSet, argv, 0)->keys.size());
}

WavesValue waves_SetClear(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto* s = HANDLE(WavesSet, argv, 0);
    s->keys.clear();
    s->originals.clear();
    return waves_makeNull();
}

// Returns all set elements as a Waves array (unordered)
WavesValue waves_SetToArray(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    auto* s = HANDLE(WavesSet, argv, 0);
    WavesValue arr = waves_array_create((int)s->originals.size());
    for (auto& p : s->originals)
        waves_array_push(arr, p.second);
    return arr;
}

WavesValue waves_SetDestroy(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    delete HANDLE(WavesSet, argv, 0);
    return waves_makeNull();
}

} // extern "C"