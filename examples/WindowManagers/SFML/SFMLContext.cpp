//============================================================================
// Name        : SFMLContext.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Template
//============================================================================

#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Utils/Thread/Thread.h>
#include "SFMLContext.h"

namespace p3d {
    
    SFMLContext::SFMLContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : Context(width,height) 
    {
        
        uint32 type = 0;
        bool fullScreen = false;
        
        if (windowType & WindowType::Fullscreen) { type = (type | sf::Style::Fullscreen); fullScreen = true; }
        if (windowType & WindowType::Close) type = (type | sf::Style::Close);
        if (windowType & WindowType::None) type = (type | sf::Style::None);
        if (windowType & WindowType::Resize) type = (type | sf::Style::Resize);
        if (windowType & WindowType::Titlebar) type = (type | sf::Style::Titlebar);
        
        // Get Fullscreen Modes
        modes = sf::VideoMode::getFullscreenModes();
        
        sf::ContextSettings settings = sf::ContextSettings(32);
        settings.depthBits = 24;
        settings.stencilBits = 8;
        settings.antialiasingLevel = 0;
        settings.majorVersion = 0;
        settings.minorVersion = 0;

        if (fullScreen == true)
            rview.create(modes[0], title.c_str(), type, settings);
        else
            rview.create(sf::VideoMode(width,height), title.c_str(), type, settings);
        
        Width = rview.getSize().x;
        Height = rview.getSize().y;

#if !defined(GLES2)
        // Initialize GLew
        glewInit();
#endif

        // Set OpenGL Major and Minor Versions
        sf::ContextSettings oglsettings = rview.getSettings();
        glMajor = oglsettings.majorVersion;
        glMinor = oglsettings.minorVersion;
    }
    SFMLContext::~SFMLContext() 
    {
        rview.close();
    }
    
    void SFMLContext::OnResize(const uint32 width, const uint32 height)
    {
        Width = width;
        Height = height;
    }
    bool SFMLContext::IsRunning() const
    {
        // return if application is running
        return rview.isOpen() && Initialized == true;
    }
    void SFMLContext::GetEvents()
    {
        
        // Get Events
        while (rview.pollEvent(event)) {

            if (event.type == sf::Event::Closed)
            {
                Close();
            }

            if (event.type == sf::Event::KeyPressed)
                KeyPressed(event.key.code);

            if (event.type == sf::Event::KeyReleased)
                KeyReleased(event.key.code);

            if (event.type == sf::Event::TextEntered)
            {
                TextEntered(event.text.unicode);
            }
            
            if (event.type == sf::Event::MouseButtonPressed)
                MouseButtonPressed(event.mouseButton.button);

            if (event.type == sf::Event::MouseButtonReleased)
                MouseButtonReleased(event.mouseButton.button);

            if (event.type == sf::Event::MouseMoved)
                MouseMove(sf::Mouse::getPosition(rview).x,sf::Mouse::getPosition(rview).y);

            if (event.type == sf::Event::MouseWheelMoved)
                MouseWheel(event.mouseWheel.delta);

            // Joypad
            if (event.type == sf::Event::JoystickButtonPressed)
                JoypadButtonPressed(event.joystickButton.joystickId,event.joystickButton.button);
            
            if (event.type == sf::Event::JoystickButtonReleased)
                JoypadButtonReleased(event.joystickButton.joystickId,event.joystickButton.button);
            
            if (event.type == sf::Event::JoystickMoved)
            {
                JoypadMove(event.joystickButton.joystickId, event.joystickMove.axis, event.joystickMove.position);
            }
            
            // Adjust the viewport when the window is resized
            if (event.type == sf::Event::Resized)
                OnResize(event.size.width, event.size.height);
        }   

        SetTime(clock.getElapsedTime().asMilliseconds());
        fps.setFPS(clock.getElapsedTime().asMilliseconds());
        Thread::CheckThreads();
    }

    void SFMLContext::Draw() 
    {
        // use sfml display
        rview.display();
    }
    
    void SFMLContext::HideMouse()
    {
        rview.setMouseCursorVisible(false);
    }
    void SFMLContext::ShowMouse()
    {
        rview.setMouseCursorVisible(true);
    }
    
    // Buttons and Mouse
    void SFMLContext::KeyPressed(const uint32 key)
    {
        // Key Pressed
        SetKeyPressed(key);
    }
    void SFMLContext::KeyReleased(const uint32 key)
    {
        // Key Released
        SetKeyReleased(key);
    }
    void SFMLContext::TextEntered(const uint32 Code)
    {
        SetCharEntered(Code);
    }
    void SFMLContext::MouseButtonPressed(const uint32 button)
    {
        // Mouse Button Pressed
        switch(button)
        {
            case sf::Mouse::Left:
                SetMouseButtonPressed(Event::Input::Mouse::Left);
                break;
            case sf::Mouse::Middle:
                SetMouseButtonPressed(Event::Input::Mouse::Middle);
                break;
            case sf::Mouse::Right:
                SetMouseButtonPressed(Event::Input::Mouse::Right);
                break;
        }
    }
    void SFMLContext::MouseButtonReleased(const uint32 button)
    {
        // Mouse Button Released
        switch(button)
        {
            case sf::Mouse::Left:
                SetMouseButtonReleased(Event::Input::Mouse::Left);
                break;
            case sf::Mouse::Middle:
                SetMouseButtonReleased(Event::Input::Mouse::Middle);
                break;
            case sf::Mouse::Right:
                SetMouseButtonReleased(Event::Input::Mouse::Right);
                break;
        }
    }
    void SFMLContext::MouseMove(const f32 mousex, const f32 mousey)
    {
        SetMouseMove(mousex,mousey);
    }
    void SFMLContext::MouseWheel(const f32 delta)
    {
        SetMouseWheel(delta);
    }
    void SFMLContext::SetMousePosition(const uint32 mouseX, const uint32 mouseY)
    {
        sf::Mouse::setPosition(sf::Vector2i(mouseX, mouseY), rview);
    }
    const Vec2 SFMLContext::GetMousePosition() const
    {
        return Vec2(sf::Mouse::getPosition(rview).x, sf::Mouse::getPosition(rview).y);
    }
    void SFMLContext::JoypadButtonPressed(const uint32 JoypadID, const uint32 Button)
    {
        SetJoypadButtonPressed(JoypadID, Button);
    }
    void SFMLContext::JoypadButtonReleased(const uint32 JoypadID, const uint32 Button)
    {
        SetJoypadButtonReleased(JoypadID, Button);
    }
    void SFMLContext::JoypadMove(const uint32 JoypadID, const uint32 Axis, const f32 Value)
    {
        SetJoypadMove(JoypadID, Axis, Value);
    }
    // virtuals methods
    void SFMLContext::Init() {}
    void SFMLContext::Update() {}
    void SFMLContext::Shutdown() {}
    void SFMLContext::Close()
    {
        Initialized = false;
    }
   
}
