//============================================================================
// Name        : FPS.h
// Author      : Duarte Peixinho
// Version     : 0.2
// Copyright   : ;)
// Description : FPS counter
//============================================================================

#ifndef FPS_H
#define	FPS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <sstream>
#include <iostream>
#include <math.h>

namespace p3d {

    class PYROS3D_API FPS {
        public:
            FPS();
            void setFPS(const f32 &time);
            uint32 getFPS();
            virtual ~FPS();
        private:
            f32 seconds, lastFrameMS,MS;
            int32 fps, countFPS;
    };

}

#endif	/* FPS_H */

