//============================================================================
// Name        : DeltaTime.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Delta Time
//============================================================================

#include "DeltaTime.h"
#include <iostream>
namespace Fishy3D {

        DeltaTime::DeltaTime() 
        {
            externalTime = 0;
            externalTimeInterval = 0;
            internalTime = 0;
            
            pauseTime = 0;
            pauseTimeStart = 0;
            isPaused = false;
            
            bulletTime = 0;
            bulletTimeStart = 0;
            isBulletTime = false;
            
            timeInterval = 0;
        }
        DeltaTime::~DeltaTime() {}
        
        void DeltaTime::Update(const long double& time)
        {
            // Save External Time
            externalTime = time;
            // Calculate Internal Time
            internalTime = externalTime - (pauseTime + bulletTime);
        }
        long double DeltaTime::GetTime()
        {
            if (isPaused==true)
            {
                return pauseTimeStart;
            } else
                return internalTime;
        }
       long double DeltaTime::GetTimeInterval()
        {
           if (isPaused)
           {
               return 0.0f;
           }
           else if (isBulletTime==true)
           {
               float _timeInterval = (internalTime - timeInterval) / bulletFactor;
               timeInterval = internalTime;
               return _timeInterval;
           }
           else {
               float _timeInterval = internalTime - timeInterval;
               timeInterval = internalTime;               
               return _timeInterval;
           }
        }
        void DeltaTime::Pause()
        {
            if (isPaused!=true)
            {
                isPaused = true;
                pauseTimeStart = internalTime;
            }
        }
        void DeltaTime::Resume()
        {
            if (isPaused==true)
            {
                isPaused = false;
                pauseTime += internalTime - pauseTimeStart;
                pauseTimeStart = 0;
            }
        }        
        void DeltaTime::StartBulletTime(const float &factor)
        {
            if (isBulletTime!=true)
            {
                isBulletTime = true;
                bulletTimeStart = internalTime;
                bulletFactor = factor;
            }
        }
        void DeltaTime::StopBulletTime()
        {
            if (isBulletTime==true)
            {
                isBulletTime = false;
                float _bulletTime = (internalTime - bulletTimeStart) / bulletFactor;
                timeInterval -= _bulletTime;
                bulletTime += _bulletTime;
                bulletTimeStart = 0;
                bulletFactor = 0;                
            }
        }
        bool DeltaTime::IsBulletTime()
        {
            return isBulletTime;
        }
}