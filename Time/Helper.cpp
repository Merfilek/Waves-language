#include "WavesBridge.h"
#include <iostream>
#include <thread>
#include <chrono>

extern "C" {
    WavesValue waves_Wait(int argc, WavesValue* argv)
    {
        WAVES_REQUIRE_ARGS(argc, 1);
        std::this_thread::sleep_for(std::chrono::duration<double>(waves_getDouble(argv[0])));
        return waves_makeNull();
    }
}