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

    std::map<uint32, uint32> SDLContext::MapSDLKeyboard;

    void SDLContext::CreateKeyboardMap()
    {
        SDLContext::MapSDLKeyboard[SDLK_a] = Event::Input::Keyboard::A;
        SDLContext::MapSDLKeyboard[SDLK_b] = Event::Input::Keyboard::B;
        SDLContext::MapSDLKeyboard[SDLK_c] = Event::Input::Keyboard::C;
        SDLContext::MapSDLKeyboard[SDLK_d] = Event::Input::Keyboard::D;
        SDLContext::MapSDLKeyboard[SDLK_e] = Event::Input::Keyboard::E;
        SDLContext::MapSDLKeyboard[SDLK_f] = Event::Input::Keyboard::F;
        SDLContext::MapSDLKeyboard[SDLK_g] = Event::Input::Keyboard::G;
        SDLContext::MapSDLKeyboard[SDLK_h] = Event::Input::Keyboard::H;
        SDLContext::MapSDLKeyboard[SDLK_i] = Event::Input::Keyboard::I;
        SDLContext::MapSDLKeyboard[SDLK_j] = Event::Input::Keyboard::J;
        SDLContext::MapSDLKeyboard[SDLK_k] = Event::Input::Keyboard::K;
        SDLContext::MapSDLKeyboard[SDLK_l] = Event::Input::Keyboard::L;
        SDLContext::MapSDLKeyboard[SDLK_m] = Event::Input::Keyboard::M;
        SDLContext::MapSDLKeyboard[SDLK_n] = Event::Input::Keyboard::N;
        SDLContext::MapSDLKeyboard[SDLK_o] = Event::Input::Keyboard::O;
        SDLContext::MapSDLKeyboard[SDLK_p] = Event::Input::Keyboard::P;
        SDLContext::MapSDLKeyboard[SDLK_q] = Event::Input::Keyboard::Q;
        SDLContext::MapSDLKeyboard[SDLK_r] = Event::Input::Keyboard::R;
        SDLContext::MapSDLKeyboard[SDLK_s] = Event::Input::Keyboard::S;
        SDLContext::MapSDLKeyboard[SDLK_t] = Event::Input::Keyboard::T;
        SDLContext::MapSDLKeyboard[SDLK_u] = Event::Input::Keyboard::U;
        SDLContext::MapSDLKeyboard[SDLK_v] = Event::Input::Keyboard::V;
        SDLContext::MapSDLKeyboard[SDLK_w] = Event::Input::Keyboard::W;
        SDLContext::MapSDLKeyboard[SDLK_x] = Event::Input::Keyboard::X;
        SDLContext::MapSDLKeyboard[SDLK_y] = Event::Input::Keyboard::Y;
        SDLContext::MapSDLKeyboard[SDLK_z] = Event::Input::Keyboard::Z;
        SDLContext::MapSDLKeyboard[SDLK_p] = Event::Input::Keyboard::P;
        SDLContext::MapSDLKeyboard[SDLK_KP_0] = Event::Input::Keyboard::Num0;
        SDLContext::MapSDLKeyboard[SDLK_KP_1] = Event::Input::Keyboard::Num1;
        SDLContext::MapSDLKeyboard[SDLK_KP_2] = Event::Input::Keyboard::Num2;
        SDLContext::MapSDLKeyboard[SDLK_KP_3] = Event::Input::Keyboard::Num3;
        SDLContext::MapSDLKeyboard[SDLK_KP_4] = Event::Input::Keyboard::Num4;
        SDLContext::MapSDLKeyboard[SDLK_KP_5] = Event::Input::Keyboard::Num5;
        SDLContext::MapSDLKeyboard[SDLK_KP_6] = Event::Input::Keyboard::Num6;
        SDLContext::MapSDLKeyboard[SDLK_KP_7] = Event::Input::Keyboard::Num7;
        SDLContext::MapSDLKeyboard[SDLK_KP_8] = Event::Input::Keyboard::Num8;
        SDLContext::MapSDLKeyboard[SDLK_KP_9] = Event::Input::Keyboard::Num9;
        SDLContext::MapSDLKeyboard[SDLK_ESCAPE] = Event::Input::Keyboard::Escape;
        SDLContext::MapSDLKeyboard[SDLK_LCTRL] = Event::Input::Keyboard::LControl;
        SDLContext::MapSDLKeyboard[SDLK_LSHIFT] = Event::Input::Keyboard::LShift;
        SDLContext::MapSDLKeyboard[SDLK_LALT] = Event::Input::Keyboard::LAlt;    
        SDLContext::MapSDLKeyboard[SDLK_LGUI] = Event::Input::Keyboard::LSystem;       
        SDLContext::MapSDLKeyboard[SDLK_RSHIFT] = Event::Input::Keyboard::RShift;
        SDLContext::MapSDLKeyboard[SDLK_RALT] = Event::Input::Keyboard::RAlt;
        SDLContext::MapSDLKeyboard[SDLK_RGUI] = Event::Input::Keyboard::RSystem;
        SDLContext::MapSDLKeyboard[SDLK_MENU] = Event::Input::Keyboard::Menu;
        SDLContext::MapSDLKeyboard[SDLK_LEFTBRACKET] = Event::Input::Keyboard::LBracket;
        SDLContext::MapSDLKeyboard[SDLK_RIGHTBRACKET] = Event::Input::Keyboard::RBracket;
        SDLContext::MapSDLKeyboard[SDLK_KP_COLON] = Event::Input::Keyboard::SemiColon;
        SDLContext::MapSDLKeyboard[SDLK_KP_COMMA] = Event::Input::Keyboard::Comma;
        SDLContext::MapSDLKeyboard[SDLK_KP_PERIOD] = Event::Input::Keyboard::Period;
        SDLContext::MapSDLKeyboard[SDLK_QUOTE] = Event::Input::Keyboard::Quote;
        SDLContext::MapSDLKeyboard[SDLK_SLASH] = Event::Input::Keyboard::Slash;
        SDLContext::MapSDLKeyboard[SDLK_BACKSLASH] = Event::Input::Keyboard::BackSlash;
        //< The ~ key
        SDLContext::MapSDLKeyboard[SDLK_EQUALS] = Event::Input::Keyboard::Equal;
        SDLContext::MapSDLKeyboard[SDLK_KP_MINUS] = Event::Input::Keyboard::Dash;
        SDLContext::MapSDLKeyboard[SDLK_SPACE] = Event::Input::Keyboard::Space;
        SDLContext::MapSDLKeyboard[SDLK_RETURN] =Event::Input::Keyboard::Return;;
        SDLContext::MapSDLKeyboard[SDLK_AC_BACK] = Event::Input::Keyboard::Back;
        SDLContext::MapSDLKeyboard[SDLK_TAB] = Event::Input::Keyboard::Tab;
        SDLContext::MapSDLKeyboard[SDLK_PAGEUP] = Event::Input::Keyboard::PageUp;
        SDLContext::MapSDLKeyboard[SDLK_PAGEDOWN] = Event::Input::Keyboard::PageDown;
        SDLContext::MapSDLKeyboard[SDLK_END] = Event::Input::Keyboard::End;
        SDLContext::MapSDLKeyboard[SDLK_AC_HOME] = Event::Input::Keyboard::Home;
        SDLContext::MapSDLKeyboard[SDLK_INSERT] = Event::Input::Keyboard::Insert;
        SDLContext::MapSDLKeyboard[SDLK_DELETE] = Event::Input::Keyboard::Delete;
        SDLContext::MapSDLKeyboard[SDLK_KP_MEMADD] = Event::Input::Keyboard::Add;
        SDLContext::MapSDLKeyboard[SDLK_KP_MEMSUBTRACT] = Event::Input::Keyboard::Subtract;
        SDLContext::MapSDLKeyboard[SDLK_KP_MULTIPLY] = Event::Input::Keyboard::Multiply;
        SDLContext::MapSDLKeyboard[SDLK_KP_DIVIDE] = Event::Input::Keyboard::Divide;
        SDLContext::MapSDLKeyboard[SDLK_LEFT] = Event::Input::Keyboard::Left;
        SDLContext::MapSDLKeyboard[SDLK_RIGHT] = Event::Input::Keyboard::Right;
        SDLContext::MapSDLKeyboard[SDLK_UP] = Event::Input::Keyboard::Up;
        SDLContext::MapSDLKeyboard[SDLK_DOWN] = Event::Input::Keyboard::Down;
        SDLContext::MapSDLKeyboard[SDLK_0] = Event::Input::Keyboard::Numpad0;
        SDLContext::MapSDLKeyboard[SDLK_1] = Event::Input::Keyboard::Numpad1;
        SDLContext::MapSDLKeyboard[SDLK_2] = Event::Input::Keyboard::Numpad2;
        SDLContext::MapSDLKeyboard[SDLK_3] = Event::Input::Keyboard::Numpad3;
        SDLContext::MapSDLKeyboard[SDLK_4] = Event::Input::Keyboard::Numpad4;
        SDLContext::MapSDLKeyboard[SDLK_5] = Event::Input::Keyboard::Numpad5;
        SDLContext::MapSDLKeyboard[SDLK_6] = Event::Input::Keyboard::Numpad6;
        SDLContext::MapSDLKeyboard[SDLK_7] = Event::Input::Keyboard::Numpad7;
        SDLContext::MapSDLKeyboard[SDLK_8] = Event::Input::Keyboard::Numpad8;
        SDLContext::MapSDLKeyboard[SDLK_9] = Event::Input::Keyboard::Numpad9;
        SDLContext::MapSDLKeyboard[SDLK_F1] = Event::Input::Keyboard::F1;
        SDLContext::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F2;
        SDLContext::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F3;
        SDLContext::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F4;
        SDLContext::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F5;
        SDLContext::MapSDLKeyboard[SDLK_F16] = Event::Input::Keyboard::F6;
        SDLContext::MapSDLKeyboard[SDLK_F17] = Event::Input::Keyboard::F7;
        SDLContext::MapSDLKeyboard[SDLK_F18] = Event::Input::Keyboard::F8;
        SDLContext::MapSDLKeyboard[SDLK_F19] = Event::Input::Keyboard::F9;
        SDLContext::MapSDLKeyboard[SDLK_F10] = Event::Input::Keyboard::F10;
        SDLContext::MapSDLKeyboard[SDLK_F11] = Event::Input::Keyboard::F11;
        SDLContext::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F12;
        SDLContext::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F13;
        SDLContext::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F14;
        SDLContext::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F15;
        SDLContext::MapSDLKeyboard[SDLK_PAUSE] = Event::Input::Keyboard::Pause;
    }

    SDLContext::SDLContext(const uint32 &width, const uint32 &height, const std::string &title, const unsigned int &windowType) : Context(width,height) 
    {
        // Map Keys
        CreateKeyboardMap();

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
       SetKeyPressed(MapSDLKeyboard[key]);
    }
    void SDLContext::KeyReleased(const unsigned& key)
    {
        SetKeyReleased(MapSDLKeyboard[key]);
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
