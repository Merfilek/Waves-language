#include "WavesBridge.h"
#include <iostream>

extern "C" {
    WavesValue waves_Print(int argc, WavesValue* argv)
    {
        if(argc != 1){
            return waves_makeError("You need to put atleast 1 argument!");
        }
        printf(waves_getString(argv[0]));
        return waves_makeNull();
    }
}