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

    // Seconds since an arbitrary, fixed-for-this-process epoch (steady_clock's own) --
    // never jumps backward or gets adjusted by the system clock, which is all
    // Stopwatch.Start/Stop/Elapsed (main.waves) needs: every call subtracts two
    // readings from the same run to get a duration, never compares against a wall-
    // clock timestamp from anywhere else.
    WavesValue waves_Monotonic(int argc, WavesValue* argv)
    {
        using namespace std::chrono;
        double seconds = duration<double>(steady_clock::now().time_since_epoch()).count();
        return waves_makeDouble(seconds);
    }
}