//============================================================================
// Name        : DeltaTime.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Delta Time
//============================================================================

#ifndef DELTATIME_H
#define	DELTATIME_H

namespace Fishy3D {

    class DeltaTime {
        public:
            
            DeltaTime();
            virtual ~DeltaTime();
            
            void Pause();
            void Resume();
            bool IsPaused();
            
            void StartBulletTime(const float &factor);
            void StopBulletTime();
            bool IsBulletTime();
            
            long double GetTime();
            long double GetTimeInterval();
            
            void Update(const long double &time);
            
        private:
            
            // save internal time
            long double internalTime;
            // save external time
            long double externalTime;
            // save external interval
            float externalTimeInterval;
            
            // pause vars
            bool isPaused;
            long double pauseTime;
            long double pauseTimeStart;
            
            bool isBulletTime;
            long double bulletTime;
            long double bulletTimeStart;
            float bulletFactor;
      
            // Time Interval
            long double timeInterval;
            
    };

};

#endif	/* DELTATIME_H */