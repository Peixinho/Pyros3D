//============================================================================
// Name        : Context.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a Context
//============================================================================

#include "Context.h"

namespace p3d {

    
    Context::Context(const unsigned int &width, const unsigned int &height) : Width(width), Height(height), Initialized(true) {}
    
    Context::~Context() {}
   
    f64 Context::GetTime()
    {
        return deltaTime.GetTime();
    }
    
    f64 Context::GetTimeInterval()
    {
        return deltaTime.GetTimeInterval();
    }
    void Context::ActivateBulletTime(const f32& factor)
    {
        deltaTime.StartBulletTime(factor);
    }
    void Context::DeactivateBulletTime()
    {
        deltaTime.StopBulletTime();
    }
    void Context::SetTime(const f32 &Timer)
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

    // Setters for InputManager
    void Context::SetKeyPressed(const unsigned &key)
    {
        // Key Pressed
        InputManager::KeyPressed(key);
    }
    void Context::SetKeyReleased(const unsigned &key)
    {
        // Key Released
        InputManager::KeyReleased(key);
    }
    void Context::SetCharEntered(const unsigned &key)
    {
        // Key Released
        InputManager::CharEntered(key);
    }    
    void Context::SetMouseButtonPressed(const unsigned &button)
    { 
        InputManager::MousePressed(button);
    }
    void Context::SetMouseButtonReleased(const unsigned &button)
    {
        InputManager::MouseReleased(button);
    }
    void Context::SetMouseMove(const f32 &mousex, const f32 &mousey)
    {
        InputManager::SetMousePosition(mousex,mousey);
    }
    void Context::SetMouseWheel(const f32 &delta)
    {
        InputManager::SetMouseWheel(delta);
    }
    void Context::SetJoypadButtonPressed(const unsigned &JoypadID, const unsigned &Button)
    {
        InputManager::JoypadButtonPressed(JoypadID, Button);
    }
    void Context::SetJoypadButtonReleased(const unsigned &JoypadID, const unsigned &Button)
    {
        InputManager::JoypadButtonReleased(JoypadID, Button);
    }
    void Context::SetJoypadMove(const unsigned &JoypadID, const unsigned &Button, const f32 &Value)
    {
        InputManager::JoypadMove(JoypadID, Button, Value);
    }
}