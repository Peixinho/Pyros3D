//============================================================================
// Name        : Context.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a Context
//============================================================================

#include <Pyros3D/Core/Context/Context.h>

namespace p3d {

    
    Context::Context(const uint32 width, const uint32 height) : Width(width), Height(height), Initialized(true) {}
    
    Context::~Context() {}
   
    f64 Context::GetTime()
    {
        return deltaTime.GetTime();
    }
    
    f64 Context::GetTimeInterval()
    {
        return deltaTime.GetTimeInterval();
    }
    void Context::ActivateBulletTime(const f32 factor)
    {
        deltaTime.StartBulletTime(factor);
    }
    void Context::DeactivateBulletTime()
    {
        deltaTime.StopBulletTime();
    }
    void Context::SetTime(const f32 Timer)
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
    void Context::SetKeyPressed(const uint32 key)
    {
        // Key Pressed
        InputManager::KeyPressed(key);
    }
    void Context::SetKeyReleased(const uint32 key)
    {
        // Key Released
        InputManager::KeyReleased(key);
    }
    void Context::SetCharEntered(const uint32 key)
    {
        // Key Released
        InputManager::CharEntered(key);
    }    
    void Context::SetMouseButtonPressed(const uint32 button)
    { 
        InputManager::MousePressed(button);
    }
    void Context::SetMouseButtonReleased(const uint32 button)
    {
        InputManager::MouseReleased(button);
    }
    void Context::SetMouseMove(const f32 mousex, const f32 mousey)
    {
        InputManager::SetMousePosition(mousex,mousey);
    }
    void Context::SetMouseWheel(const f32 delta)
    {
        InputManager::SetMouseWheel(delta);
    }
    void Context::SetJoypadButtonPressed(const uint32 JoypadID, const uint32 Button)
    {
        InputManager::JoypadButtonPressed(JoypadID, Button);
    }
    void Context::SetJoypadButtonReleased(const uint32 JoypadID, const uint32 Button)
    {
        InputManager::JoypadButtonReleased(JoypadID, Button);
    }
    void Context::SetJoypadMove(const uint32 JoypadID, const uint32 Button, const f32 Value)
    {
        InputManager::JoypadMove(JoypadID, Button, Value);
    }
}