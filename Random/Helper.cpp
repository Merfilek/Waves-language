#include "WavesBridge.h"
#include <iostream>
#include <random>

extern "C" {
    WavesValue waves_RandomInt(int argc, WavesValue* argv)
    {
        WAVES_REQUIRE_ARGS(argc, 2);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> distrib(waves_getInt(argv[0]), waves_getInt(argv[1]));
        return waves_makeInt(distrib(gen));
    }
}