//============================================================================
// Name        : FPS.h
// Author      : Duarte Peixinho
// Version     : 0.2
// Copyright   : ;)
// Description : FPS counter
//============================================================================

#ifndef FPS_H
#define	FPS_H

#include <sstream>
#include <iostream>
#include <math.h>

namespace Fishy3D {

    class FPS {
        public:
            FPS();
            void setFPS(const float &time);
            unsigned getFPS();
            virtual ~FPS();
        private:
            float seconds, lastFrameMS,MS;
            int fps, countFPS;
    };

}

#endif	/* FPS_H */

