#include "WavesBridge.h"
#include <iostream>

extern "C" {
    WavesValue waves_Print(int argc, WavesValue* argv)
    {
        std::cout << "ITS WORKING!" << std::endl;
        return waves_makeNull();
    }
}