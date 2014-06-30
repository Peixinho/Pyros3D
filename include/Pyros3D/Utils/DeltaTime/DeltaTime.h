//============================================================================
// Name        : DeltaTime.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Delta Time
//============================================================================

#ifndef DELTATIME_H
#define	DELTATIME_H

#include "../../Core/Math/Math.h"
#include "../../Other/Export.h"

namespace p3d {

    class PYROS3D_API DeltaTime {
        public:
            
            DeltaTime();
            virtual ~DeltaTime();
            
            void Pause();
            void Resume();
            bool IsPaused();
            
            void StartBulletTime(const f32 &factor);
            void StopBulletTime();
            bool IsBulletTime();
            
            f96 GetTime();
            f96 GetTimeInterval();
            
            void Update(const f96 &time);
            
        private:
            
            // save internal time
            f96 internalTime;
            // save external time
            f96 externalTime;
            // save external interval
            f32 externalTimeInterval;
            
            // pause vars
            bool isPaused;
            f96 pauseTime;
            f96 pauseTimeStart;
            
            bool isBulletTime;
            f96 bulletTime;
            f96 bulletTimeStart;
            f32 bulletFactor;
      
            // Time Interval
            f96 timeInterval;
            
    };

};

#endif	/* DELTATIME_H */