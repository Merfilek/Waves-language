#include "WavesBridge.h"
#include <iostream>

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