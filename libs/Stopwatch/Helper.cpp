#include "WavesBridge.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <string>

extern "C" {

    static std::chrono::high_resolution_clock::time_point g_start;
    static std::chrono::high_resolution_clock::time_point g_end;

    WavesValue waves_Start(int argc, WavesValue* argv)
    {
        g_start = std::chrono::high_resolution_clock::now();
        return waves_makeNull();
    }

    WavesValue waves_Stop(int argc, WavesValue* argv)
    {
        g_end = std::chrono::high_resolution_clock::now();
        return waves_makeNull();
    }

    // Return duration in milliseconds, zero-padded as string
    WavesValue waves_DurationMs(int argc, WavesValue* argv)
    {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(g_end - g_start).count();
        std::ostringstream oss;
        oss << std::setw(3) << std::setfill('0') << duration;
        return waves_makeString(oss.str().c_str());
    }

    // Return duration in seconds, zero-padded as string
    WavesValue waves_DurationSc(int argc, WavesValue* argv)
    {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(g_end - g_start).count();
        std::ostringstream oss;
        oss << std::setw(3) << std::setfill('0') << duration;
        return waves_makeString(oss.str().c_str());
    }

    // Return duration in microseconds, zero-padded as string
    WavesValue waves_DurationUs(int argc, WavesValue* argv)
    {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(g_end - g_start).count();
        std::ostringstream oss;
        oss << std::setw(3) << std::setfill('0') << duration;
        return waves_makeString(oss.str().c_str());
    }

    WavesValue waves_Reset(int argc, WavesValue* argv)
    {
        g_start = std::chrono::high_resolution_clock::time_point();
        g_end   = std::chrono::high_resolution_clock::time_point();
        return waves_makeNull();
    }
}