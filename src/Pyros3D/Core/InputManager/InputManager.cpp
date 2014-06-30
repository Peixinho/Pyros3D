//============================================================================
// Name        : InputManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Input Manager
//============================================================================

#include <Pyros3D/Core/InputManager/InputManager.h>

namespace p3d {
    
    uint32 InputManager::mouseX = 0;
    uint32 InputManager::mouseY = 0;
    
    std::map<uint32, Gallant::Signal1<Event::Input::Info> > InputManager::EventsMapPressed;
    std::map<uint32, Gallant::Signal1<Event::Input::Info> > InputManager::EventsMapReleased;
    Gallant::Signal1<Event::Input::Info> InputManager::MouseWheelEvents;
    Gallant::Signal1<Event::Input::Info> InputManager::MouseMoveEvents;
    Gallant::Signal1<Event::Input::Info> InputManager::CharacterEnteredPress;
    Gallant::Signal1<Event::Input::Info> InputManager::CharacterEnteredRelease;
    uchar InputManager::Code;
    
    // Joypad
    std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadPressed;
    std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadReleased;
    std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadMove;
    
    void InputManager::JoypadButtonPressed(const uint32& joypadID, const uint32& button)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnPress;
        m.Input = button; 
        JoyPadPressed[joypadID][button](m);        
    }
    void InputManager::JoypadButtonReleased(const uint32& joypadID, const uint32& button)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnRelease;
        m.Input = button;
        JoyPadReleased[joypadID][button](m);
    }
    void InputManager::JoypadMove(const uint32& joypadID, const uint32& axis, const f32 &value)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnMove;
        m.Input = axis;
        m.Value = value;        
        JoyPadMove[joypadID][axis](m);        
    }
    
    void InputManager::SetMousePosition(const uint32& mousex, const uint32& mousey)
    {
        mouseX = mousex;
        mouseY = mousey;
        
        Event::Input::Info m;
        m.Type = Event::Type::OnMove;
        m.Input = Event::Input::Mouse::Move;

        MouseMoveEvents(m);
    }
    void InputManager::SetMouseWheel(const f32& delta)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnMove;
        m.Input = Event::Input::Mouse::Wheel;
        m.Value = delta;
        MouseWheelEvents(m);
    }
    Vec2 InputManager::GetMousePosition()
    {
        return Vec2(mouseX, mouseY);
    }
    void InputManager::MousePressed(const uint32& e)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnPress;
        m.Input = e;
        EventsMapPressed[e](m);
    }
    void InputManager::MouseReleased(const uint32& e)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnRelease;
        m.Input = e;
        EventsMapReleased[e](m);
    }
    
    void InputManager::KeyPressed(const uint32& e)
    {
       
        Event::Input::Info k;
        k.Type = Event::Type::OnPress;
        k.Input = e;
        
        EventsMapPressed[e](k);
        
    }
    void InputManager::KeyReleased(const uint32& e)
    {
        
        Event::Input::Info k;
        k.Type = Event::Type::OnRelease;
        k.Input = e;

        EventsMapReleased[e](k);        
    }
    
    void InputManager::CharEntered(const uint32& e)
    {
#if !defined(ANDROID) && !defined(EMSCRIPTEN)
        // Could be on Pressed or Released, there is nothing different from them
        Code = e;
        Event::Input::Info kPress;
        kPress.Type = Event::Type::OnPress;
        kPress.Input = e;        
        EventsMapPressed[Event::Input::Keyboard::OtherKeyboardEvents::CharacterEnter](kPress);
        
        Event::Input::Info kRelease;
        kRelease.Type = Event::Type::OnRelease;
        kRelease.Input = e;
        EventsMapReleased[Event::Input::Keyboard::OtherKeyboardEvents::CharacterEnter](kRelease);
#endif
    }
};
