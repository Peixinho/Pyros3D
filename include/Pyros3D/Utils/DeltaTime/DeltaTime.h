//============================================================================
// Name        : DeltaTime.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Delta Time
//============================================================================

#ifndef DELTATIME_H
#define	DELTATIME_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

    class PYROS3D_API DeltaTime {
        public:
            
            DeltaTime();
            virtual ~DeltaTime();
            
            void Pause();
            void Resume();
            bool IsPaused();
            
            void StartBulletTime(const f32 factor);
            void StopBulletTime();
            bool IsBulletTime();
            
            f64 GetTime();
            f64 GetTimeInterval();
            
            void Update(const f64 &time);
            
        private:
            
            // save internal time
            f64 internalTime;
            // save external time
            f64 externalTime;
            // save external interval
            f32 externalTimeInterval;
            
            // pause vars
            bool isPaused;
            f64 pauseTime;
            f64 pauseTimeStart;
            
            bool isBulletTime;
            f64 bulletTime;
            f64 bulletTimeStart;
            f32 bulletFactor;
      
            // Time Interval
            f64 timeInterval;
            
    };

};

#endif	/* DELTATIME_H */