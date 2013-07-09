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

namespace p3d {

    class DeltaTime {
        public:
            
            DeltaTime();
            virtual ~DeltaTime();
            
            void Pause();
            void Resume();
            bool IsPaused();
            
            void StartBulletTime(const f32 &factor);
            void StopBulletTime();
            bool IsBulletTime();
            
            ld64 GetTime();
            ld64 GetTimeInterval();
            
            void Update(const ld64 &time);
            
        private:
            
            // save internal time
            ld64 internalTime;
            // save external time
            ld64 externalTime;
            // save external interval
            f32 externalTimeInterval;
            
            // pause vars
            bool isPaused;
            ld64 pauseTime;
            ld64 pauseTimeStart;
            
            bool isBulletTime;
            ld64 bulletTime;
            ld64 bulletTimeStart;
            f32 bulletFactor;
      
            // Time Interval
            ld64 timeInterval;
            
    };

};

#endif	/* DELTATIME_H */