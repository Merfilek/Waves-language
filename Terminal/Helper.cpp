#include "WavesBridge.h"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
// Windows' console only renders \033[...m ANSI/VT escape codes (what every
// function below writes) once ENABLE_VIRTUAL_TERMINAL_PROCESSING is turned
// on for the output handle -- off by default pre-Windows 10 1511, and even
// on modern Windows a fresh console starts without it. Linux/macOS terminals
// don't need an equivalent: VT100 escapes are just how they've always worked.
// Runs once, at DLL load, rather than before every single color call.
namespace {
    struct EnableVtMode {
        EnableVtMode() {
            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            if (GetConsoleMode(h, &mode))
                SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    } g_enableVtMode;
}
#endif

extern "C" {
    WavesValue waves_Color(int argc, WavesValue* argv)
    {
        int r = waves_getInt(argv[0]);
        int g = waves_getInt(argv[1]);
        int b = waves_getInt(argv[2]);
        std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m";
        return waves_makeNull();
    }

    WavesValue waves_ResetColor(int argc, WavesValue* argv)
    {
        std::cout << "\033[0m";
        return waves_makeNull();
    }

    WavesValue waves_BackgroundColor(int argc, WavesValue* argv)
    {
        int r = waves_getInt(argv[0]);
        int g = waves_getInt(argv[1]);
        int b = waves_getInt(argv[2]);
        std::cout << "\033[48;2;" << r << ";" << g << ";" << b << "m";
        return waves_makeNull();
    }
}