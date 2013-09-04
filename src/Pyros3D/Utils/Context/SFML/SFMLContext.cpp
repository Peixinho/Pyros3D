//============================================================================
// Name        : SFMLContext.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Template
//============================================================================

#include "GL/glew.h"
#include "SFMLContext.h"

namespace p3d {
    
    #define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }
    SFMLContext::SFMLContext(const int &width, const int &height, const std::string &title, const unsigned int &windowType) : Context(width,height) 
    {        
        
        // Display Info
        displayInfo = false;
        
        unsigned type = 0;
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
        settings.stencilBits = 0;
        settings.antialiasingLevel = 0;
        settings.majorVersion = 0;
        settings.minorVersion = 0;

        if (fullScreen == true)
            rview.create(modes[0], title, type, settings);
        else
            rview.create(sf::VideoMode(width,height), title, type, settings);
        
        // Initialize GLew
        glewInit();
        
    }
    SFMLContext::~SFMLContext() 
    {
        rview.close();
    }
    
    void SFMLContext::OnResize(const uint32 &width, const uint32 &height)
    {
        Width = width;
        Height = height;
        
        // resize application
        sf::View TheView(sf::FloatRect(0, 0, (f32)Width, (f32)Height));
        rview.setView(TheView);
    }
    bool SFMLContext::IsRunning() const
    {
        // return if application is running
        return rview.isOpen() && Initialized == true;
    }
    void SFMLContext::GetEvents()
    {
        // Get Events
        sf::Event event;

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

        SetTime(clock.getElapsedTime().asSeconds());
        fps.setFPS(clock.getElapsedTime().asSeconds());
    }

    void SFMLContext::Draw() 
    {
        // Push States
        rview.pushGLStates();                
        
        // Display Text and Logo
        if (displayInfo == true)
        {
            rview.draw(Text);
        }
        
        // use sfml display
        rview.display();

        // Pop States
        rview.popGLStates();
    }
    
    void SFMLContext::HideMouse()
    {
        rview.setMouseCursorVisible(false);
    }
    void SFMLContext::ShowMouse()
    {
        rview.setMouseCursorVisible(true);
    }
    
    // private
    void SFMLContext::DisplayInfo(const std::string& text, const Vec2& scale, const Vec4& color)
    {

        Font = sf::Font();
        Font.loadFromFile("font/verdana.ttf");

        Text = sf::Text(text,Font);
        Text.setFont(Font);

        // Draw Text
        Text.setColor(sf::Color(color.x*255,color.y*255,color.z*255));
        Text.scale(scale.x, scale.y);
        
        // Show Display Info
        displayInfo = true;
        
    }
    
    // Buttons and Mouse
    void SFMLContext::KeyPressed(const unsigned& key)
    {
        // Key Pressed
        InputManager::KeyPressed(key);        
    }
    void SFMLContext::KeyReleased(const unsigned& key)
    {
        // Key Released
        InputManager::KeyReleased(key);
    }
    void SFMLContext::TextEntered(const unsigned& Code)
    {
        InputManager::CharEntered(Code);
    }
    void SFMLContext::MouseButtonPressed(const unsigned& button)
    {
        // Mouse Button Pressed
        switch(button)
        {
            case sf::Mouse::Left:
                InputManager::MousePressed(Event::Input::Mouse::Left);
                break;
            case sf::Mouse::Middle:
                InputManager::MousePressed(Event::Input::Mouse::Middle);
                break;
            case sf::Mouse::Right:
                InputManager::MousePressed(Event::Input::Mouse::Right);
                break;
        }
    }
    void SFMLContext::MouseButtonReleased(const unsigned& button)
    {
        // Mouse Button Released
        switch(button)
        {
            case sf::Mouse::Left:
                InputManager::MouseReleased(Event::Input::Mouse::Left);
                break;
            case sf::Mouse::Middle:
                InputManager::MouseReleased(Event::Input::Mouse::Middle);
                break;
            case sf::Mouse::Right:
                InputManager::MouseReleased(Event::Input::Mouse::Right);
                break;
        }
    }
    void SFMLContext::MouseMove(const f32 &mousex, const f32 &mousey)
    {
        // Mouse Moving
        InputManager::SetMousePosition(mousex,mousey);
    }
    void SFMLContext::MouseWheel(const f32 &delta)
    {
        // Mouse Wheel
        InputManager::SetMouseWheel(delta);
    }
    void SFMLContext::SetMousePosition(const unsigned &mouseX, const unsigned &mouseY)
    {
        sf::Mouse::setPosition(sf::Vector2i(mouseX, mouseY), rview);
    }
    const Vec2 SFMLContext::GetMousePosition() const
    {
        return Vec2(sf::Mouse::getPosition(rview).x, sf::Mouse::getPosition(rview).y);
    }
    void SFMLContext::JoypadButtonPressed(const unsigned& JoypadID, const unsigned& Button)
    {
        InputManager::JoypadButtonPressed(JoypadID, Button);
    }
    void SFMLContext::JoypadButtonReleased(const unsigned& JoypadID, const unsigned& Button)
    {
        InputManager::JoypadButtonReleased(JoypadID, Button);
    }
    void SFMLContext::JoypadMove(const unsigned& JoypadID, const unsigned& Axis, const f32 &Value)
    {
        InputManager::JoypadMove(JoypadID, Axis, Value);
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
