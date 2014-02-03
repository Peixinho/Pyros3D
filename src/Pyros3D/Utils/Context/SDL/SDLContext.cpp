//============================================================================
// Name        : SDLContext.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL Context
//============================================================================

#include "GL/glew.h"
#include "SDLContext.h"

namespace p3d {
    
    #define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }
    SDLContext::SDLContext(const uint32 &width, const uint32 &height, const std::string &title, const unsigned int &windowType) : Context(width,height) 
    {
        
        // Initialize SDL2
		SDL_Init(SDL_INIT_VIDEO);

        unsigned type = 0;

		if (windowType & WindowType::Fullscreen) { type = (type | SDL_WINDOW_FULLSCREEN); }
        if (windowType & WindowType::None) type = (type | SDL_WINDOW_BORDERLESS);
        if (windowType & WindowType::Resize) type = (type | SDL_WINDOW_RESIZABLE);
        type = type | SDL_WINDOW_OPENGL;

    	rview = SDL_CreateWindow(
    		title.c_str(),
			SDL_WINDOWPOS_UNDEFINED,
    		SDL_WINDOWPOS_UNDEFINED,
    		width,
    		height,
    		type
		);
        
		mainGLContext = SDL_GL_CreateContext(rview);

        // Initialize GLew
        glewInit();
		
    }
    SDLContext::~SDLContext() 
    {
    	SDL_GL_DeleteContext(mainGLContext);
        SDL_DestroyWindow(rview);
        SDL_Quit();
    }
    
    void SDLContext::OnResize(const uint32 &width, const uint32 &height)
    {
        Width = width;
        Height = height;
        
        // resize application
        SDL_SetWindowSize(rview,width,height);
    }
    bool SDLContext::IsRunning() const
    {
    	return Initialized;
    }
    void SDLContext::GetEvents()
    {
        SDL_Event sdl_event;
		while(SDL_PollEvent(&sdl_event) > 0) /* While there are more than 0 events in the queue */
		{
            if (sdl_event.type == SDL_QUIT)
            {
		        Close();
            }

            if (sdl_event.type == SDL_KEYDOWN)
                KeyPressed(sdl_event.key.keysym.sym);

            if (sdl_event.type == SDL_KEYUP)
                KeyReleased(sdl_event.key.keysym.sym);

            if (sdl_event.type == SDL_TEXTINPUT)
            {
                TextEntered(*(int32*)sdl_event.text.text);
            }
            
            if (sdl_event.type == SDL_MOUSEBUTTONDOWN)
                MouseButtonPressed(sdl_event.button.button);

            if (sdl_event.type == SDL_MOUSEBUTTONUP)
                MouseButtonReleased(sdl_event.button.button);

            if (sdl_event.type == SDL_MOUSEMOTION)
                MouseMove(sdl_event.motion.x,sdl_event.motion.y);

            if (sdl_event.type == SDL_MOUSEWHEEL)
                MouseWheel(sdl_event.wheel.y);

            // // Joypad
            // if (event.type == sf::Event::JoystickButtonPressed)
            //     JoypadButtonPressed(event.joystickButton.joystickId,event.joystickButton.button);
            
            // if (event.type == sf::Event::JoystickButtonReleased)
            //     JoypadButtonReleased(event.joystickButton.joystickId,event.joystickButton.button);
            
            // if (event.type == sf::Event::JoystickMoved)
            // {
            //     JoypadMove(event.joystickButton.joystickId, event.joystickMove.axis, event.joystickMove.position);
            // }

            // Adjust the viewport when the window is resized
            if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                OnResize(sdl_event.window.data1, sdl_event.window.data2);
            }
		}
        
        SetTime(SDL_GetTicks()/1000.f);
        fps.setFPS(SDL_GetTicks()/1000.f);
    }

    void SDLContext::Draw() 
    {
        SDL_GL_SwapWindow(rview);
    }
    
    void SDLContext::HideMouse()
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
    void SDLContext::ShowMouse()
    {
        SDL_ShowCursor(SDL_ENABLE);
    }
    
    // Buttons and Mouse
    void SDLContext::KeyPressed(const unsigned& key)
    {
       SetKeyPressed(key);
    }
    void SDLContext::KeyReleased(const unsigned& key)
    {
        SetKeyReleased(key);
    }
    void SDLContext::TextEntered(const unsigned& Code)
    {
        SetCharEntered(Code);
    }
    void SDLContext::MouseButtonPressed(const unsigned& button)
    {
        // Mouse Button Pressed
        switch(button)
        {
            case SDL_BUTTON_LEFT:
                SetMouseButtonPressed(Event::Input::Mouse::Left);
                break;
            case SDL_BUTTON_MIDDLE:
                SetMouseButtonPressed(Event::Input::Mouse::Middle);
                break;
            case SDL_BUTTON_RIGHT:
                SetMouseButtonPressed(Event::Input::Mouse::Right);
                break;
        }
    }
    void SDLContext::MouseButtonReleased(const unsigned& button)
    {
        // Mouse Button Released
        switch(button)
        {
            case SDL_BUTTON_LEFT:
                SetMouseButtonReleased(Event::Input::Mouse::Left);
                break;
            case SDL_BUTTON_MIDDLE:
                SetMouseButtonReleased(Event::Input::Mouse::Middle);
                break;
            case SDL_BUTTON_RIGHT:
                SetMouseButtonReleased(Event::Input::Mouse::Right);
                break;
        }
    }
    void SDLContext::MouseMove(const f32 &mousex, const f32 &mousey)
    {
        // Mouse Moving
        SetMouseMove(mousex,mousey);
    }
    void SDLContext::MouseWheel(const f32 &delta)
    {
        SetMouseWheel(delta);
    }
    void SDLContext::SetMousePosition(const unsigned &mouseX, const unsigned &mouseY)
    {
        SDL_WarpMouseInWindow(rview, mouseX, mouseY);
    }
    const Vec2 SDLContext::GetMousePosition() const
    {
        int mousex, mousey;
        SDL_GetMouseState(&mousex,&mousey);
        return Vec2(mousex,mousey);
    }
    void SDLContext::JoypadButtonPressed(const unsigned& JoypadID, const unsigned& Button)
    {

    }
    void SDLContext::JoypadButtonReleased(const unsigned& JoypadID, const unsigned& Button)
    {

    }
    void SDLContext::JoypadMove(const unsigned& JoypadID, const unsigned& Axis, const f32 &Value)
    {

    }
    // virtuals methods
    void SDLContext::Init() {}
    void SDLContext::Update() {}
    void SDLContext::Shutdown() {}
    void SDLContext::Close()
    {
        Initialized = false;
    }
   
}
