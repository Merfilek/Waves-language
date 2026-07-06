#include "WavesBridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

// ── Internal helpers ──────────────────────────────────────────────────────────

// Read entire file into a heap-allocated buffer (caller must free)
static char* read_file_raw(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* buf = (char*)malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    if (out_size) *out_size = size;
    return buf;
}

// Count occurrences of a char in a string
static int count_char(const char* s, char c) {
    int n = 0;
    while (*s) if (*s++ == c) n++;
    return n;
}

// Duplicate a string into malloc'd memory
static char* dup_str(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)malloc(len + 1);
    memcpy(r, s, len + 1);
    return r;
}

extern "C" {

// ── File.readText(path) -> string ─────────────────────────────────────────────
WavesValue waves_FileRead(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    const char* path = WAVES_ARG_STRING(argv, 0);
    char* buf = read_file_raw(path, NULL);
    if (!buf) return waves_makeError("File not found");
    WavesValue v = waves_makeString(buf);
    // buf ownership transferred — Waves VM will copy/intern the string
    return v;
}

// ── File.writeText(path, text) -> bool ───────────────────────────────────────
WavesValue waves_FileWrite(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    const char* path = WAVES_ARG_STRING(argv, 0);
    const char* text = WAVES_ARG_STRING(argv, 1);
    FILE* f = fopen(path, "wb");
    if (!f) return waves_makeBool(0);
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    return waves_makeBool(1);
}

// ── File.appendText(path, text) -> bool ──────────────────────────────────────
WavesValue waves_FileAppend(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    const char* path = WAVES_ARG_STRING(argv, 0);
    const char* text = WAVES_ARG_STRING(argv, 1);
    FILE* f = fopen(path, "ab");
    if (!f) return waves_makeBool(0);
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    return waves_makeBool(1);
}

// ── File.exists(path) -> bool ─────────────────────────────────────────────────
WavesValue waves_FileExists(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    const char* path = WAVES_ARG_STRING(argv, 0);
    struct stat st;
    return waves_makeBool(stat(path, &st) == 0);
}

// ── File.delete(path) -> bool ─────────────────────────────────────────────────
WavesValue waves_FileDelete(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    return waves_makeBool(remove(WAVES_ARG_STRING(argv, 0)) == 0);
}

// ── File.size(path) -> int ────────────────────────────────────────────────────
WavesValue waves_FileSize(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    struct stat st;
    if (stat(WAVES_ARG_STRING(argv, 0), &st) != 0) return waves_makeInt(-1);
    return waves_makeInt((int32_t)st.st_size);
}

// ── File.readLines(path) -> string[] ─────────────────────────────────────────
// Returns a Waves array where each element is one line (newline stripped).
WavesValue waves_FileReadLines(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    const char* path = WAVES_ARG_STRING(argv, 0);

    char* buf = read_file_raw(path, NULL);
    if (!buf) return waves_makeError("File not found");

    // Count lines first for capacity hint
    int line_count = count_char(buf, '\n') + 1;
    WavesValue arr = waves_array_create(line_count);

    char* cursor = buf;
    while (*cursor) {
        char* newline = strchr(cursor, '\n');
        size_t len;
        if (newline) {
            len = (size_t)(newline - cursor);
            // Strip trailing \r for Windows line endings
            if (len > 0 && cursor[len - 1] == '\r') len--;
        } else {
            len = strlen(cursor);
        }

        char* line = (char*)malloc(len + 1);
        memcpy(line, cursor, len);
        line[len] = '\0';

        waves_array_push(arr, waves_makeString(line));
        // line ownership transferred to VM via makeString

        if (!newline) break;
        cursor = newline + 1;
    }

    free(buf);
    return arr;
}

// ── Path.dirname(path) -> string ─────────────────────────────────────────────
// Returns everything before the last '/', or "." if none.
WavesValue waves_PathDirname(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    char* copy = dup_str(WAVES_ARG_STRING(argv, 0));
    // dirname() may modify its argument and return a pointer into it
    char* dir = dirname(copy);
    WavesValue v = waves_makeString(dup_str(dir));
    free(copy);
    return v;
}

// ── Path.basename(path) -> string ────────────────────────────────────────────
// Returns the filename component: "src/main.waves" -> "main.waves"
WavesValue waves_PathBasename(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    char* copy = dup_str(WAVES_ARG_STRING(argv, 0));
    char* base = basename(copy);
    WavesValue v = waves_makeString(dup_str(base));
    free(copy);
    return v;
}

// ── Path.extension(path) -> string ───────────────────────────────────────────
// Returns the extension including the dot: "main.waves" -> ".waves"
// Returns "" if no extension.
WavesValue waves_PathExtension(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    const char* path = WAVES_ARG_STRING(argv, 0);
    const char* dot = strrchr(path, '.');
    // Make sure the dot is in the filename, not a directory like ".config/file"
    const char* slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) return waves_makeString("");
    return waves_makeString(dup_str(dot));
}

// ── Path.noExtension(path) -> string ─────────────────────────────────────────
// "src/main.waves" -> "src/main"
WavesValue waves_PathNoExtension(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 1);
    const char* path = WAVES_ARG_STRING(argv, 0);
    const char* dot  = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) return waves_makeString(dup_str(path));
    size_t len = (size_t)(dot - path);
    char* result = (char*)malloc(len + 1);
    memcpy(result, path, len);
    result[len] = '\0';
    return waves_makeString(result);
}

// ── Path.changeExtension(path, ext) -> string ────────────────────────────────
// File.changeExtension("src/main.waves", ".wbc") -> "src/main.wbc"
// ext should include the dot. If it doesn't, one is added automatically.
WavesValue waves_PathChangeExt(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    const char* path = WAVES_ARG_STRING(argv, 0);
    const char* ext  = WAVES_ARG_STRING(argv, 1);

    const char* dot   = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    size_t base_len   = (dot && (!slash || dot > slash))
                          ? (size_t)(dot - path)
                          : strlen(path);

    // Add leading dot if caller forgot it
    int need_dot = (ext[0] != '.' && ext[0] != '\0') ? 1 : 0;
    size_t ext_len = strlen(ext);
    char* result = (char*)malloc(base_len + need_dot + ext_len + 1);
    memcpy(result, path, base_len);
    if (need_dot) result[base_len] = '.';
    memcpy(result + base_len + need_dot, ext, ext_len);
    result[base_len + need_dot + ext_len] = '\0';
    return waves_makeString(result);
}

// ── Path.join(a, b) -> string ─────────────────────────────────────────────────
// Joins two path segments with a '/' between them, but won't double-up slashes.
// File.join("src", "utils.waves")   -> "src/utils.waves"
// File.join("src/", "utils.waves")  -> "src/utils.waves"
// File.join("", "utils.waves")      -> "utils.waves"
WavesValue waves_PathJoin(int argc, WavesValue* argv) {
    WAVES_REQUIRE_ARGS(argc, 2);
    const char* a = WAVES_ARG_STRING(argv, 0);
    const char* b = WAVES_ARG_STRING(argv, 1);
    size_t la = strlen(a);
    size_t lb = strlen(b);

    if (la == 0) return waves_makeString(dup_str(b));
    if (lb == 0) return waves_makeString(dup_str(a));

    int needs_sep = (a[la - 1] != '/');
    char* result  = (char*)malloc(la + needs_sep + lb + 1);
    memcpy(result, a, la);
    if (needs_sep) result[la] = '/';
    memcpy(result + la + needs_sep, b, lb);
    result[la + needs_sep + lb] = '\0';
    return waves_makeString(result);
}

} // extern "C"