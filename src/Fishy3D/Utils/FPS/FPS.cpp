//============================================================================
// Name        : FPS.cpp
// Author      : Duarte Peixinho
// Version     : 0.2
// Copyright   : ;)
// Description : FPS counter
//============================================================================

#include "FPS.h"

namespace Fishy3D {

    FPS::FPS() {

        countFPS = fps = 0;

    }

    void FPS::setFPS(const float& time) {        
        
        if (time - seconds >=1 || seconds == 0) {
            seconds=time;
            countFPS=fps;
            fps=0;
        }
        fps++;

        // save milliseconds
        MS=time-lastFrameMS;
        lastFrameMS=time;
    } 

    unsigned FPS::getFPS()
    {
        return countFPS;
    }
    
    FPS::~FPS() {}

}