//============================================================================
// Name        : SFMLInterface.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Template
//============================================================================

#include "GL/glew.h"
#include "SFMLInterface.h"

namespace Fishy3D {
	#define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }
    SFMLInterface::SFMLInterface(const int &width, const int &height, const std::string &title, const unsigned int &windowType) : Context(width,height) 
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
        
        if (fullScreen == true)
            rview.create(modes[0], title, type, sf::ContextSettings(32));
        else 
            rview.create(sf::VideoMode(width,height), title, type, sf::ContextSettings(32));
        
        // Set Logo
        Logo.loadFromFile("textures/LogoDemo_black.png");
        LogoSprite.setTexture(Logo);
        
        // Initialize GLew
        glewInit();
        
    }
    SFMLInterface::~SFMLInterface() 
    {
        rview.close();
    }
    void SFMLInterface::Resize(const int& width, const int& height)
    {
        Width = width;
        Height = height;
        
        // resize application
        glViewport(0,0,Width,Height);
        sf::View TheView(sf::FloatRect(0, 0, (float)Width, (float)Height));
        rview.setView(TheView);                

    }
    void SFMLInterface::OnResize(const int &width, const int &height)
    {
        Width = width;
        Height = height;
        
        Resize(Width, Height);
    }
    bool SFMLInterface::IsRunning() const
    {
        // return if application is running
        return rview.isOpen() && Initialized == true;
    }
    void SFMLInterface::GetEvents()
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

    void SFMLInterface::Draw() 
    {
        // Push States
        rview.pushGLStates();                
        
        // Display Text and Logo
        if (displayInfo == true)
        {            
            rview.draw(LogoSprite);
            rview.draw(Text);
        }
        
        // use sfml display
        rview.display();

        // Pop States
        rview.popGLStates();
    }
    
    void SFMLInterface::HideMouse()
    {
        rview.setMouseCursorVisible(false);
    }
    void SFMLInterface::ShowMouse()
    {
        rview.setMouseCursorVisible(true);
    }
    
    // private
    void SFMLInterface::DisplayInfo(const std::string& text, const vec2& scale, const vec4& color)
    {
        // Draw Text
        Text = sf::Text(text);        
        Text.setColor(sf::Color(color.x*255,color.y*255,color.z*255));
        Text.scale(scale.x, scale.y);
        
        // Draw Logo
        LogoSprite.setPosition(Width-100,0);
        
        // Show Display Info
        displayInfo = true;
        
    }
    
    // Buttons and Mouse
    void SFMLInterface::KeyPressed(const unsigned& key)
    {
        // Key Pressed
        InputManager::KeyPressed(key);        
    }
    void SFMLInterface::KeyReleased(const unsigned& key)
    {
        // Key Released
        InputManager::KeyReleased(key);
    }
    void SFMLInterface::TextEntered(const unsigned& Code)
    {
        InputManager::CharEntered(Code);
    }
    void SFMLInterface::MouseButtonPressed(const unsigned& button)
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
    void SFMLInterface::MouseButtonReleased(const unsigned& button)
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
    void SFMLInterface::MouseMove(const float &mousex, const float &mousey)
    {
        // Mouse Moving
        InputManager::SetMousePosition(mousex,mousey);
    }
    void SFMLInterface::MouseWheel(const float &delta)
    {
        // Mouse Wheel
        InputManager::SetMouseWheel(delta);
    }
    void SFMLInterface::SetMousePosition(const unsigned &mouseX, const unsigned &mouseY)
    {
        sf::Mouse::setPosition(sf::Vector2i(mouseX, mouseY), rview);
    }
    const vec2 &SFMLInterface::GetMousePosition() const
    {
        return vec2(sf::Mouse::getPosition(rview).x, sf::Mouse::getPosition(rview).y);
    }
    void SFMLInterface::JoypadButtonPressed(const unsigned& JoypadID, const unsigned& Button)
    {
        InputManager::JoypadButtonPressed(JoypadID, Button);
    }
    void SFMLInterface::JoypadButtonReleased(const unsigned& JoypadID, const unsigned& Button)
    {
        InputManager::JoypadButtonReleased(JoypadID, Button);
    }
    void SFMLInterface::JoypadMove(const unsigned& JoypadID, const unsigned& Axis, const float &Value)
    {
        InputManager::JoypadMove(JoypadID, Axis, Value);
    }
    // virtuals methods
    void SFMLInterface::Init() {}
    void SFMLInterface::Update() {}
    void SFMLInterface::Shutdown() {}
    void SFMLInterface::Close()
    {
        Initialized = false;
    }
   
}
