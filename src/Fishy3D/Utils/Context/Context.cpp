//============================================================================
// Name        : Context.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a Context
//============================================================================

#include <SFML/Graphics/RenderWindow.hpp>

#include "Context.h"

namespace Fishy3D {

    
        Context::Context(const unsigned int &width, const unsigned int &height) : Width(width), Height(height), Initialized(true) {}
        
        Context::~Context() {}
       
        long double Context::GetTime()
        {
            return deltaTime.GetTime();
        }
        
        long double Context::GetTimeInterval()
        {
            return deltaTime.GetTimeInterval();
        }
        void Context::ActivateBulletTime(const float& factor)
        {
            deltaTime.StartBulletTime(factor);
        }
        void Context::DeactivateBulletTime()
        {
            deltaTime.StopBulletTime();
        }
        void Context::SetTime(const float &Timer)
        {
            // Update Clock
            deltaTime.Update(Timer);
            
        }
        void Context::Pause()
        {
            deltaTime.Pause();
        }
        void Context::Resume()
        {
            deltaTime.Resume();
        }
    
        void Context::Close()
        {
            Initialized = false;
        }
}