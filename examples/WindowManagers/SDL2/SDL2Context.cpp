//============================================================================
// Name        : SDL2Context.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL Context
//============================================================================

#include <Pyros3D/Other/PyrosGL.h>
#include "SDL2Context.h"

namespace p3d {

    std::map<uint32, uint32> SDL2Context::MapSDLKeyboard;

    void SDL2Context::CreateKeyboardMap()
    {
        SDL2Context::MapSDLKeyboard[SDLK_a] = Event::Input::Keyboard::A;
        SDL2Context::MapSDLKeyboard[SDLK_b] = Event::Input::Keyboard::B;
        SDL2Context::MapSDLKeyboard[SDLK_c] = Event::Input::Keyboard::C;
        SDL2Context::MapSDLKeyboard[SDLK_d] = Event::Input::Keyboard::D;
        SDL2Context::MapSDLKeyboard[SDLK_e] = Event::Input::Keyboard::E;
        SDL2Context::MapSDLKeyboard[SDLK_f] = Event::Input::Keyboard::F;
        SDL2Context::MapSDLKeyboard[SDLK_g] = Event::Input::Keyboard::G;
        SDL2Context::MapSDLKeyboard[SDLK_h] = Event::Input::Keyboard::H;
        SDL2Context::MapSDLKeyboard[SDLK_i] = Event::Input::Keyboard::I;
        SDL2Context::MapSDLKeyboard[SDLK_j] = Event::Input::Keyboard::J;
        SDL2Context::MapSDLKeyboard[SDLK_k] = Event::Input::Keyboard::K;
        SDL2Context::MapSDLKeyboard[SDLK_l] = Event::Input::Keyboard::L;
        SDL2Context::MapSDLKeyboard[SDLK_m] = Event::Input::Keyboard::M;
        SDL2Context::MapSDLKeyboard[SDLK_n] = Event::Input::Keyboard::N;
        SDL2Context::MapSDLKeyboard[SDLK_o] = Event::Input::Keyboard::O;
        SDL2Context::MapSDLKeyboard[SDLK_p] = Event::Input::Keyboard::P;
        SDL2Context::MapSDLKeyboard[SDLK_q] = Event::Input::Keyboard::Q;
        SDL2Context::MapSDLKeyboard[SDLK_r] = Event::Input::Keyboard::R;
        SDL2Context::MapSDLKeyboard[SDLK_s] = Event::Input::Keyboard::S;
        SDL2Context::MapSDLKeyboard[SDLK_t] = Event::Input::Keyboard::T;
        SDL2Context::MapSDLKeyboard[SDLK_u] = Event::Input::Keyboard::U;
        SDL2Context::MapSDLKeyboard[SDLK_v] = Event::Input::Keyboard::V;
        SDL2Context::MapSDLKeyboard[SDLK_w] = Event::Input::Keyboard::W;
        SDL2Context::MapSDLKeyboard[SDLK_x] = Event::Input::Keyboard::X;
        SDL2Context::MapSDLKeyboard[SDLK_y] = Event::Input::Keyboard::Y;
        SDL2Context::MapSDLKeyboard[SDLK_z] = Event::Input::Keyboard::Z;
        SDL2Context::MapSDLKeyboard[SDLK_p] = Event::Input::Keyboard::P;
        SDL2Context::MapSDLKeyboard[SDLK_KP_0] = Event::Input::Keyboard::Num0;
        SDL2Context::MapSDLKeyboard[SDLK_KP_1] = Event::Input::Keyboard::Num1;
        SDL2Context::MapSDLKeyboard[SDLK_KP_2] = Event::Input::Keyboard::Num2;
        SDL2Context::MapSDLKeyboard[SDLK_KP_3] = Event::Input::Keyboard::Num3;
        SDL2Context::MapSDLKeyboard[SDLK_KP_4] = Event::Input::Keyboard::Num4;
        SDL2Context::MapSDLKeyboard[SDLK_KP_5] = Event::Input::Keyboard::Num5;
        SDL2Context::MapSDLKeyboard[SDLK_KP_6] = Event::Input::Keyboard::Num6;
        SDL2Context::MapSDLKeyboard[SDLK_KP_7] = Event::Input::Keyboard::Num7;
        SDL2Context::MapSDLKeyboard[SDLK_KP_8] = Event::Input::Keyboard::Num8;
        SDL2Context::MapSDLKeyboard[SDLK_KP_9] = Event::Input::Keyboard::Num9;
        SDL2Context::MapSDLKeyboard[SDLK_ESCAPE] = Event::Input::Keyboard::Escape;
        SDL2Context::MapSDLKeyboard[SDLK_LCTRL] = Event::Input::Keyboard::LControl;
        SDL2Context::MapSDLKeyboard[SDLK_LSHIFT] = Event::Input::Keyboard::LShift;
        SDL2Context::MapSDLKeyboard[SDLK_LALT] = Event::Input::Keyboard::LAlt;    
        SDL2Context::MapSDLKeyboard[SDLK_LGUI] = Event::Input::Keyboard::LSystem;       
        SDL2Context::MapSDLKeyboard[SDLK_RSHIFT] = Event::Input::Keyboard::RShift;
        SDL2Context::MapSDLKeyboard[SDLK_RALT] = Event::Input::Keyboard::RAlt;
        SDL2Context::MapSDLKeyboard[SDLK_RGUI] = Event::Input::Keyboard::RSystem;
        SDL2Context::MapSDLKeyboard[SDLK_MENU] = Event::Input::Keyboard::Menu;
        SDL2Context::MapSDLKeyboard[SDLK_LEFTBRACKET] = Event::Input::Keyboard::LBracket;
        SDL2Context::MapSDLKeyboard[SDLK_RIGHTBRACKET] = Event::Input::Keyboard::RBracket;
        SDL2Context::MapSDLKeyboard[SDLK_KP_COLON] = Event::Input::Keyboard::SemiColon;
        SDL2Context::MapSDLKeyboard[SDLK_KP_COMMA] = Event::Input::Keyboard::Comma;
        SDL2Context::MapSDLKeyboard[SDLK_KP_PERIOD] = Event::Input::Keyboard::Period;
        SDL2Context::MapSDLKeyboard[SDLK_QUOTE] = Event::Input::Keyboard::Quote;
        SDL2Context::MapSDLKeyboard[SDLK_SLASH] = Event::Input::Keyboard::Slash;
        SDL2Context::MapSDLKeyboard[SDLK_BACKSLASH] = Event::Input::Keyboard::BackSlash;
        //< The ~ key
        SDL2Context::MapSDLKeyboard[SDLK_EQUALS] = Event::Input::Keyboard::Equal;
        SDL2Context::MapSDLKeyboard[SDLK_KP_MINUS] = Event::Input::Keyboard::Dash;
        SDL2Context::MapSDLKeyboard[SDLK_SPACE] = Event::Input::Keyboard::Space;
        SDL2Context::MapSDLKeyboard[SDLK_RETURN] =Event::Input::Keyboard::Return;;
        SDL2Context::MapSDLKeyboard[SDLK_AC_BACK] = Event::Input::Keyboard::Back;
        SDL2Context::MapSDLKeyboard[SDLK_TAB] = Event::Input::Keyboard::Tab;
        SDL2Context::MapSDLKeyboard[SDLK_PAGEUP] = Event::Input::Keyboard::PageUp;
        SDL2Context::MapSDLKeyboard[SDLK_PAGEDOWN] = Event::Input::Keyboard::PageDown;
        SDL2Context::MapSDLKeyboard[SDLK_END] = Event::Input::Keyboard::End;
        SDL2Context::MapSDLKeyboard[SDLK_AC_HOME] = Event::Input::Keyboard::Home;
        SDL2Context::MapSDLKeyboard[SDLK_INSERT] = Event::Input::Keyboard::Insert;
        SDL2Context::MapSDLKeyboard[SDLK_DELETE] = Event::Input::Keyboard::Delete;
        SDL2Context::MapSDLKeyboard[SDLK_KP_MEMADD] = Event::Input::Keyboard::Add;
        SDL2Context::MapSDLKeyboard[SDLK_KP_MEMSUBTRACT] = Event::Input::Keyboard::Subtract;
        SDL2Context::MapSDLKeyboard[SDLK_KP_MULTIPLY] = Event::Input::Keyboard::Multiply;
        SDL2Context::MapSDLKeyboard[SDLK_KP_DIVIDE] = Event::Input::Keyboard::Divide;
        SDL2Context::MapSDLKeyboard[SDLK_LEFT] = Event::Input::Keyboard::Left;
        SDL2Context::MapSDLKeyboard[SDLK_RIGHT] = Event::Input::Keyboard::Right;
        SDL2Context::MapSDLKeyboard[SDLK_UP] = Event::Input::Keyboard::Up;
        SDL2Context::MapSDLKeyboard[SDLK_DOWN] = Event::Input::Keyboard::Down;
        SDL2Context::MapSDLKeyboard[SDLK_0] = Event::Input::Keyboard::Numpad0;
        SDL2Context::MapSDLKeyboard[SDLK_1] = Event::Input::Keyboard::Numpad1;
        SDL2Context::MapSDLKeyboard[SDLK_2] = Event::Input::Keyboard::Numpad2;
        SDL2Context::MapSDLKeyboard[SDLK_3] = Event::Input::Keyboard::Numpad3;
        SDL2Context::MapSDLKeyboard[SDLK_4] = Event::Input::Keyboard::Numpad4;
        SDL2Context::MapSDLKeyboard[SDLK_5] = Event::Input::Keyboard::Numpad5;
        SDL2Context::MapSDLKeyboard[SDLK_6] = Event::Input::Keyboard::Numpad6;
        SDL2Context::MapSDLKeyboard[SDLK_7] = Event::Input::Keyboard::Numpad7;
        SDL2Context::MapSDLKeyboard[SDLK_8] = Event::Input::Keyboard::Numpad8;
        SDL2Context::MapSDLKeyboard[SDLK_9] = Event::Input::Keyboard::Numpad9;
        SDL2Context::MapSDLKeyboard[SDLK_F1] = Event::Input::Keyboard::F1;
        SDL2Context::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F2;
        SDL2Context::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F3;
        SDL2Context::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F4;
        SDL2Context::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F5;
        SDL2Context::MapSDLKeyboard[SDLK_F16] = Event::Input::Keyboard::F6;
        SDL2Context::MapSDLKeyboard[SDLK_F17] = Event::Input::Keyboard::F7;
        SDL2Context::MapSDLKeyboard[SDLK_F18] = Event::Input::Keyboard::F8;
        SDL2Context::MapSDLKeyboard[SDLK_F19] = Event::Input::Keyboard::F9;
        SDL2Context::MapSDLKeyboard[SDLK_F10] = Event::Input::Keyboard::F10;
        SDL2Context::MapSDLKeyboard[SDLK_F11] = Event::Input::Keyboard::F11;
        SDL2Context::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F12;
        SDL2Context::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F13;
        SDL2Context::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F14;
        SDL2Context::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F15;
        SDL2Context::MapSDLKeyboard[SDLK_PAUSE] = Event::Input::Keyboard::Pause;
    }

    SDL2Context::SDL2Context(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : Context(width,height) 
    {
        // Map Keys
        CreateKeyboardMap();

        // Initialize SDL2
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

#if defined GLES2_DESKTOP
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined GLES3_DESKTOP
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

        int audio_rate = 22050;
        Uint16 audio_format = AUDIO_S16; /* 16-bit stereo */
        int audio_channels = 2;
        int audio_buffers = 4096;

        if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers))
            echo("ERROR: Failed to open audio device");
        
        Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);

        uint32 type = 0;

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
#if !defined(GLES2) && !defined(GLES3)
        glewInit();
#else
        gladLoadGLES2Loader(SDL_GL_GetProcAddress);
#endif

		// Set OpenGL Major and Minor Versions
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,(int*)&glMajor);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,(int*)&glMinor);

    }
    SDL2Context::~SDL2Context() 
    {
    	SDL_GL_DeleteContext(mainGLContext);
        SDL_DestroyWindow(rview);
        SDL_Quit();
    }
    
    void SDL2Context::OnResize(const uint32 width, const uint32 height)
    {
        Width = width;
        Height = height;
        
        // resize application
        SDL_SetWindowSize(rview,width,height);
    }
    bool SDL2Context::IsRunning() const
    {
    	return Initialized;
    }
    void SDL2Context::GetEvents()
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
        
        SetTime(SDL_GetTicks());
        fps.setFPS(SDL_GetTicks());
    }

    void SDL2Context::Draw() 
    {
        SDL_GL_SwapWindow(rview);
    }
    
    void SDL2Context::HideMouse()
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
    void SDL2Context::ShowMouse()
    {
        SDL_ShowCursor(SDL_ENABLE);
    }
    
    // Buttons and Mouse
    void SDL2Context::KeyPressed(const uint32 key)
    {
       SetKeyPressed(MapSDLKeyboard[key]);
    }
    void SDL2Context::KeyReleased(const uint32 key)
    {
        SetKeyReleased(MapSDLKeyboard[key]);
    }
    void SDL2Context::TextEntered(const uint32 Code)
    {
        SetCharEntered(Code);
    }
    void SDL2Context::MouseButtonPressed(const uint32 button)
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
    void SDL2Context::MouseButtonReleased(const uint32 button)
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
    void SDL2Context::MouseMove(const f32 mousex, const f32 mousey)
    {
        // Mouse Moving
        SetMouseMove(mousex,mousey);
    }
    void SDL2Context::MouseWheel(const f32 delta)
    {
        SetMouseWheel(delta);
    }
    void SDL2Context::SetMousePosition(const uint32 mouseX, const uint32 mouseY)
    {
        SDL_WarpMouseInWindow(rview, mouseX, mouseY);
    }
    const Vec2 SDL2Context::GetMousePosition() const
    {
        int mousex, mousey;
        SDL_GetMouseState(&mousex,&mousey);
        return Vec2(mousex,mousey);
    }
    void SDL2Context::JoypadButtonPressed(const uint32 JoypadID, const uint32 Button)
    {

    }
    void SDL2Context::JoypadButtonReleased(const uint32 JoypadID, const uint32 Button)
    {

    }
    void SDL2Context::JoypadMove(const uint32 JoypadID, const uint32 Axis, const f32 Value)
    {

    }
    // virtuals methods
    void SDL2Context::Init() {}
    void SDL2Context::Update() {}
    void SDL2Context::Shutdown() {}
    void SDL2Context::Close()
    {
        Initialized = false;
    }
   
}