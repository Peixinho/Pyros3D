//============================================================================
// Name        : FPS.cpp
// Author      : Duarte Peixinho
// Version     : 0.2
// Copyright   : ;)
// Description : FPS counter
//============================================================================

#include <Pyros3D/Utils/FPS/FPS.h>

namespace p3d {

    FPS::FPS() {

        countFPS = fps = 0;
        seconds = 0;

    }

    void FPS::setFPS(const f64 time) {        
        
        if (time - seconds >=1000 || seconds == 0) {
            seconds=time;
            countFPS=fps;
            fps=0;
        }
        fps++;

        // save milliseconds
        MS=time-lastFrameMS;
        lastFrameMS=time;
    } 

    uint32 FPS::getFPS()
    {
        return countFPS;
    }
    
    FPS::~FPS() {}

}