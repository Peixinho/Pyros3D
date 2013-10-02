//============================================================================
// Name        : InputManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Input Manager
//============================================================================

#include "InputManager.h"

namespace Fishy3D {
    
    unsigned InputManager::mouseX = 0;
    unsigned InputManager::mouseY = 0;
    
    std::map<unsigned, Gallant::Signal1<Event::Input::Info> > InputManager::EventsMapPressed;
    std::map<unsigned, Gallant::Signal1<Event::Input::Info> > InputManager::EventsMapReleased;
    Gallant::Signal1<Event::Input::Info> InputManager::MouseWheelEvents;
    Gallant::Signal1<Event::Input::Info> InputManager::MouseMoveEvents;
    Gallant::Signal1<Event::Input::Info> InputManager::CharacterEnteredPress;
    Gallant::Signal1<Event::Input::Info> InputManager::CharacterEnteredRelease;
    unsigned InputManager::CharCode;
    
    // Joypad
    std::map <unsigned, std::map<unsigned, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadPressed;
    std::map <unsigned, std::map<unsigned, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadReleased;
    std::map <unsigned, std::map<unsigned, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadMove;
    
    void InputManager::JoypadButtonPressed(const unsigned& joypadID, const unsigned& button)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnPress;
        m.Input = button; 
        JoyPadPressed[joypadID][button](m);        
    }
    void InputManager::JoypadButtonReleased(const unsigned& joypadID, const unsigned& button)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnRelease;
        m.Input = button;
        JoyPadReleased[joypadID][button](m);
    }
    void InputManager::JoypadMove(const unsigned& joypadID, const unsigned& axis, const float &value)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnMove;
        m.Input = axis;
        m.Value = value;        
        JoyPadMove[joypadID][axis](m);        
    }
    
    void InputManager::SetMousePosition(const unsigned& mousex, const unsigned& mousey)
    {
        mouseX = mousex;
        mouseY = mousey;
        
        Event::Input::Info m;
        m.Type = Event::Type::OnMove;
        m.Input = Event::Input::Mouse::Move;

        MouseMoveEvents(m);
    }
    void InputManager::SetMouseWheel(const float& delta)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnMove;
        m.Input = Event::Input::Mouse::Wheel;
        m.Value = delta;
        MouseWheelEvents(m);
    }
    vec2 InputManager::GetMousePosition()
    {
        return vec2(mouseX, mouseY);
    }
    void InputManager::MousePressed(const unsigned& e)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnPress;
        m.Input = e;
        EventsMapPressed[e](m);
    }
    void InputManager::MouseReleased(const unsigned& e)
    {
        Event::Input::Info m;
        m.Type = Event::Type::OnRelease;
        m.Input = e;
        EventsMapReleased[e](m);
    }
    
    void InputManager::KeyPressed(const unsigned& e)
    {
       
        Event::Input::Info k;
        k.Type = Event::Type::OnPress;
        k.Input = e;
        
        EventsMapPressed[e](k);
        
    }
    void InputManager::KeyReleased(const unsigned& e)
    {
        
        Event::Input::Info k;
        k.Type = Event::Type::OnRelease;
        k.Input = e;

        EventsMapReleased[e](k);        
    }
    
    void InputManager::CharEntered(const unsigned& e)
    {
        // Could be on Pressed or Released, there is nothing different from them        
        CharCode = e;
        Event::Input::Info kPress;
        kPress.Type = Event::Type::OnPress;
        kPress.Input = e;        
        EventsMapPressed[Event::Input::Keyboard::OtherKeyboardEvents::CharacterEnter](kPress);

        Event::Input::Info kRelease;
        kRelease.Type = Event::Type::OnRelease;
        kRelease.Input = e;
        EventsMapReleased[Event::Input::Keyboard::OtherKeyboardEvents::CharacterEnter](kRelease);
    }
};