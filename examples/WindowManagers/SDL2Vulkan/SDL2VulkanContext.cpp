//============================================================================
// Name        : SDL2VulkanContext.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL2 Context for the Vulkan backend - see the header
//               comment for scope (Vulkan roadmap Phase 5 Step C).
//============================================================================

#include "SDL2VulkanContext.h"

#ifdef VULKAN_BACKEND

#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#include <cstdio>

namespace p3d {

    std::map<uint32, uint32> SDL2VulkanContext::MapSDLKeyboard;

    // Identical to SDL2Context::CreateKeyboardMap() - GL/Vulkan-agnostic,
    // duplicated rather than shared per this file's header comment (keeps
    // the GL path untouched).
    void SDL2VulkanContext::CreateKeyboardMap()
    {
        SDL2VulkanContext::MapSDLKeyboard[SDLK_a] = Event::Input::Keyboard::A;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_b] = Event::Input::Keyboard::B;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_c] = Event::Input::Keyboard::C;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_d] = Event::Input::Keyboard::D;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_e] = Event::Input::Keyboard::E;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_f] = Event::Input::Keyboard::F;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_g] = Event::Input::Keyboard::G;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_h] = Event::Input::Keyboard::H;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_i] = Event::Input::Keyboard::I;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_j] = Event::Input::Keyboard::J;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_k] = Event::Input::Keyboard::K;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_l] = Event::Input::Keyboard::L;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_m] = Event::Input::Keyboard::M;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_n] = Event::Input::Keyboard::N;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_o] = Event::Input::Keyboard::O;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_p] = Event::Input::Keyboard::P;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_q] = Event::Input::Keyboard::Q;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_r] = Event::Input::Keyboard::R;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_s] = Event::Input::Keyboard::S;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_t] = Event::Input::Keyboard::T;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_u] = Event::Input::Keyboard::U;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_v] = Event::Input::Keyboard::V;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_w] = Event::Input::Keyboard::W;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_x] = Event::Input::Keyboard::X;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_y] = Event::Input::Keyboard::Y;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_z] = Event::Input::Keyboard::Z;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_p] = Event::Input::Keyboard::P;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_0] = Event::Input::Keyboard::Num0;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_1] = Event::Input::Keyboard::Num1;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_2] = Event::Input::Keyboard::Num2;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_3] = Event::Input::Keyboard::Num3;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_4] = Event::Input::Keyboard::Num4;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_5] = Event::Input::Keyboard::Num5;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_6] = Event::Input::Keyboard::Num6;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_7] = Event::Input::Keyboard::Num7;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_8] = Event::Input::Keyboard::Num8;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_9] = Event::Input::Keyboard::Num9;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_ESCAPE] = Event::Input::Keyboard::Escape;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_LCTRL] = Event::Input::Keyboard::LControl;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_LSHIFT] = Event::Input::Keyboard::LShift;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_LALT] = Event::Input::Keyboard::LAlt;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_LGUI] = Event::Input::Keyboard::LSystem;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_RSHIFT] = Event::Input::Keyboard::RShift;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_RALT] = Event::Input::Keyboard::RAlt;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_RGUI] = Event::Input::Keyboard::RSystem;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_MENU] = Event::Input::Keyboard::Menu;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_LEFTBRACKET] = Event::Input::Keyboard::LBracket;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_RIGHTBRACKET] = Event::Input::Keyboard::RBracket;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_COLON] = Event::Input::Keyboard::SemiColon;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_COMMA] = Event::Input::Keyboard::Comma;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_PERIOD] = Event::Input::Keyboard::Period;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_QUOTE] = Event::Input::Keyboard::Quote;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_SLASH] = Event::Input::Keyboard::Slash;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_BACKSLASH] = Event::Input::Keyboard::BackSlash;
        //< The ~ key
        SDL2VulkanContext::MapSDLKeyboard[SDLK_EQUALS] = Event::Input::Keyboard::Equal;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_MINUS] = Event::Input::Keyboard::Dash;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_SPACE] = Event::Input::Keyboard::Space;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_RETURN] = Event::Input::Keyboard::Return;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_AC_BACK] = Event::Input::Keyboard::Back;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_TAB] = Event::Input::Keyboard::Tab;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_PAGEUP] = Event::Input::Keyboard::PageUp;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_PAGEDOWN] = Event::Input::Keyboard::PageDown;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_END] = Event::Input::Keyboard::End;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_AC_HOME] = Event::Input::Keyboard::Home;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_INSERT] = Event::Input::Keyboard::Insert;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_DELETE] = Event::Input::Keyboard::Delete;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_MEMADD] = Event::Input::Keyboard::Add;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_MEMSUBTRACT] = Event::Input::Keyboard::Subtract;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_MULTIPLY] = Event::Input::Keyboard::Multiply;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_KP_DIVIDE] = Event::Input::Keyboard::Divide;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_LEFT] = Event::Input::Keyboard::Left;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_RIGHT] = Event::Input::Keyboard::Right;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_UP] = Event::Input::Keyboard::Up;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_DOWN] = Event::Input::Keyboard::Down;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_0] = Event::Input::Keyboard::Numpad0;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_1] = Event::Input::Keyboard::Numpad1;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_2] = Event::Input::Keyboard::Numpad2;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_3] = Event::Input::Keyboard::Numpad3;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_4] = Event::Input::Keyboard::Numpad4;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_5] = Event::Input::Keyboard::Numpad5;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_6] = Event::Input::Keyboard::Numpad6;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_7] = Event::Input::Keyboard::Numpad7;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_8] = Event::Input::Keyboard::Numpad8;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_9] = Event::Input::Keyboard::Numpad9;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F1] = Event::Input::Keyboard::F1;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F2;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F3;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F4;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F5;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F16] = Event::Input::Keyboard::F6;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F17] = Event::Input::Keyboard::F7;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F18] = Event::Input::Keyboard::F8;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F19] = Event::Input::Keyboard::F9;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F10] = Event::Input::Keyboard::F10;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F11] = Event::Input::Keyboard::F11;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F12;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F13;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F14;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F15;
        SDL2VulkanContext::MapSDLKeyboard[SDLK_PAUSE] = Event::Input::Keyboard::Pause;
    }

    SDL2VulkanContext::SDL2VulkanContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : Context(width,height), vulkanDevice(NULL)
    {
        // Map Keys
        CreateKeyboardMap();

        // Initialize SDL2
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

        uint32 type = 0;

        if (windowType & WindowType::Fullscreen) { type = (type | SDL_WINDOW_FULLSCREEN); }
        if (windowType & WindowType::None) type = (type | SDL_WINDOW_BORDERLESS);
        if (windowType & WindowType::Resize) type = (type | SDL_WINDOW_RESIZABLE);
        // No SDL_GL_SetAttribute calls, no SDL_GL_CreateContext, no
        // gladLoadGL - this window never gets an OpenGL context. Instance/
        // device/swapchain creation happens elsewhere (VulkanRenderDevice,
        // Step D) using GetRequiredInstanceExtensions()/CreateSurface()
        // below.
        type = type | SDL_WINDOW_VULKAN;

        rview = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            width,
            height,
            type
        );

        // Construct the real render device + swapchain here, now that a
        // window exists to create a VkSurfaceKHR against - see
        // GetRequiredInstanceExtensions()/CreateSurface() below, and the
        // header comment on `vulkanDevice`. Ownership transfers to
        // whichever IRenderer gets constructed next (every example's
        // `new ForwardRenderer(Width, Height)`) via
        // RegisterRenderDeviceForOwnership() - this class never deletes
        // `vulkanDevice` itself.
        vulkanDevice = new VulkanRenderDevice(GetRequiredInstanceExtensions());
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (vulkanDevice->GetInstance() == VK_NULL_HANDLE ||
            !CreateSurface(vulkanDevice->GetInstance(), &surface) ||
            !vulkanDevice->InitializeSwapchain(surface, width, height))
        {
            // Matches this constructor's existing no-error-handling style
            // (SDL_CreateWindow() above isn't checked either) - a NULL
            // rview/unusable vulkanDevice will surface loudly the first
            // time anything tries to use it, same as today.
            fprintf(stderr, "SDL2VulkanContext: failed to initialize VulkanRenderDevice/swapchain\n");
        }
        RegisterRenderDeviceForOwnership(vulkanDevice);
    }
    SDL2VulkanContext::~SDL2VulkanContext()
    {
        SDL_DestroyWindow(rview);
        SDL_Quit();
    }

    std::vector<const char*> SDL2VulkanContext::GetRequiredInstanceExtensions() const
    {
        uint32_t count = 0;
        SDL_Vulkan_GetInstanceExtensions(rview, &count, NULL);
        std::vector<const char*> extensions(count);
        SDL_Vulkan_GetInstanceExtensions(rview, &count, extensions.data());
        return extensions;
    }

    bool SDL2VulkanContext::CreateSurface(VkInstance instance, VkSurfaceKHR *outSurface) const
    {
        return SDL_Vulkan_CreateSurface(rview, instance, outSurface) == SDL_TRUE;
    }

    void SDL2VulkanContext::OnResize(const uint32 width, const uint32 height)
    {
        Width = width;
        Height = height;

        // resize application
        SDL_SetWindowSize(rview,width,height);
    }
    bool SDL2VulkanContext::IsRunning() const
    {
        return Initialized;
    }
    void SDL2VulkanContext::GetEvents()
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

            // Adjust the viewport when the window is resized
            if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                OnResize(sdl_event.window.data1, sdl_event.window.data2);
            }
        }

        SetTime(SDL_GetTicks());
        fps.setFPS(SDL_GetTicks());
    }

    void SDL2VulkanContext::Draw()
    {
        // Stub - the real acquire/submit/present sequence needs a real
        // swapchain (VulkanRenderDevice, Step D) to call into. See the
        // class comment in the header.
    }

    void SDL2VulkanContext::HideMouse()
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
    void SDL2VulkanContext::ShowMouse()
    {
        SDL_ShowCursor(SDL_ENABLE);
    }

    // Buttons and Mouse
    void SDL2VulkanContext::KeyPressed(const uint32 key)
    {
       SetKeyPressed(MapSDLKeyboard[key]);
    }
    void SDL2VulkanContext::KeyReleased(const uint32 key)
    {
        SetKeyReleased(MapSDLKeyboard[key]);
    }
    void SDL2VulkanContext::TextEntered(const uint32 Code)
    {
        SetCharEntered(Code);
    }
    void SDL2VulkanContext::MouseButtonPressed(const uint32 button)
    {
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
    void SDL2VulkanContext::MouseButtonReleased(const uint32 button)
    {
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
    void SDL2VulkanContext::MouseMove(const f32 mousex, const f32 mousey)
    {
        SetMouseMove(mousex,mousey);
    }
    void SDL2VulkanContext::MouseWheel(const f32 delta)
    {
        SetMouseWheel(delta);
    }
    void SDL2VulkanContext::SetMousePosition(const uint32 mouseX, const uint32 mouseY)
    {
        SDL_WarpMouseInWindow(rview, mouseX, mouseY);
    }
    const Vec2 SDL2VulkanContext::GetMousePosition() const
    {
        int mousex, mousey;
        SDL_GetMouseState(&mousex,&mousey);
        return Vec2(mousex,mousey);
    }
    void SDL2VulkanContext::JoypadButtonPressed(const uint32 JoypadID, const uint32 Button)
    {

    }
    void SDL2VulkanContext::JoypadButtonReleased(const uint32 JoypadID, const uint32 Button)
    {

    }
    void SDL2VulkanContext::JoypadMove(const uint32 JoypadID, const uint32 Axis, const f32 Value)
    {

    }
    // virtuals methods
    void SDL2VulkanContext::Init() {}
    void SDL2VulkanContext::Update() {}
    void SDL2VulkanContext::Shutdown() {}
    void SDL2VulkanContext::Close()
    {
        Initialized = false;
    }

}

#endif /* VULKAN_BACKEND */
