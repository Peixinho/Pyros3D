//============================================================================
// Name        : SDL2MetalContext.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL2 Context for the native Metal backend - see the header
//               comment for scope.
//============================================================================

#include "SDL2MetalContext.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"

#ifdef METAL_BACKEND

#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#include <cstdio>

namespace p3d {

    std::map<uint32, uint32> SDL2MetalContext::MapSDLKeyboard;

    // Identical to SDL2Context::CreateKeyboardMap()/SDL2VulkanContext::
    // CreateKeyboardMap() - GL/Vulkan/Metal-agnostic, duplicated rather
    // than shared per those files' header comments (keeps each backend's
    // window path independently editable).
    void SDL2MetalContext::CreateKeyboardMap()
    {
        SDL2MetalContext::MapSDLKeyboard[SDLK_a] = Event::Input::Keyboard::A;
        SDL2MetalContext::MapSDLKeyboard[SDLK_b] = Event::Input::Keyboard::B;
        SDL2MetalContext::MapSDLKeyboard[SDLK_c] = Event::Input::Keyboard::C;
        SDL2MetalContext::MapSDLKeyboard[SDLK_d] = Event::Input::Keyboard::D;
        SDL2MetalContext::MapSDLKeyboard[SDLK_e] = Event::Input::Keyboard::E;
        SDL2MetalContext::MapSDLKeyboard[SDLK_f] = Event::Input::Keyboard::F;
        SDL2MetalContext::MapSDLKeyboard[SDLK_g] = Event::Input::Keyboard::G;
        SDL2MetalContext::MapSDLKeyboard[SDLK_h] = Event::Input::Keyboard::H;
        SDL2MetalContext::MapSDLKeyboard[SDLK_i] = Event::Input::Keyboard::I;
        SDL2MetalContext::MapSDLKeyboard[SDLK_j] = Event::Input::Keyboard::J;
        SDL2MetalContext::MapSDLKeyboard[SDLK_k] = Event::Input::Keyboard::K;
        SDL2MetalContext::MapSDLKeyboard[SDLK_l] = Event::Input::Keyboard::L;
        SDL2MetalContext::MapSDLKeyboard[SDLK_m] = Event::Input::Keyboard::M;
        SDL2MetalContext::MapSDLKeyboard[SDLK_n] = Event::Input::Keyboard::N;
        SDL2MetalContext::MapSDLKeyboard[SDLK_o] = Event::Input::Keyboard::O;
        SDL2MetalContext::MapSDLKeyboard[SDLK_p] = Event::Input::Keyboard::P;
        SDL2MetalContext::MapSDLKeyboard[SDLK_q] = Event::Input::Keyboard::Q;
        SDL2MetalContext::MapSDLKeyboard[SDLK_r] = Event::Input::Keyboard::R;
        SDL2MetalContext::MapSDLKeyboard[SDLK_s] = Event::Input::Keyboard::S;
        SDL2MetalContext::MapSDLKeyboard[SDLK_t] = Event::Input::Keyboard::T;
        SDL2MetalContext::MapSDLKeyboard[SDLK_u] = Event::Input::Keyboard::U;
        SDL2MetalContext::MapSDLKeyboard[SDLK_v] = Event::Input::Keyboard::V;
        SDL2MetalContext::MapSDLKeyboard[SDLK_w] = Event::Input::Keyboard::W;
        SDL2MetalContext::MapSDLKeyboard[SDLK_x] = Event::Input::Keyboard::X;
        SDL2MetalContext::MapSDLKeyboard[SDLK_y] = Event::Input::Keyboard::Y;
        SDL2MetalContext::MapSDLKeyboard[SDLK_z] = Event::Input::Keyboard::Z;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_0] = Event::Input::Keyboard::Num0;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_1] = Event::Input::Keyboard::Num1;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_2] = Event::Input::Keyboard::Num2;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_3] = Event::Input::Keyboard::Num3;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_4] = Event::Input::Keyboard::Num4;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_5] = Event::Input::Keyboard::Num5;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_6] = Event::Input::Keyboard::Num6;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_7] = Event::Input::Keyboard::Num7;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_8] = Event::Input::Keyboard::Num8;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_9] = Event::Input::Keyboard::Num9;
        SDL2MetalContext::MapSDLKeyboard[SDLK_ESCAPE] = Event::Input::Keyboard::Escape;
        SDL2MetalContext::MapSDLKeyboard[SDLK_LCTRL] = Event::Input::Keyboard::LControl;
        SDL2MetalContext::MapSDLKeyboard[SDLK_LSHIFT] = Event::Input::Keyboard::LShift;
        SDL2MetalContext::MapSDLKeyboard[SDLK_LALT] = Event::Input::Keyboard::LAlt;
        SDL2MetalContext::MapSDLKeyboard[SDLK_LGUI] = Event::Input::Keyboard::LSystem;
        SDL2MetalContext::MapSDLKeyboard[SDLK_RSHIFT] = Event::Input::Keyboard::RShift;
        SDL2MetalContext::MapSDLKeyboard[SDLK_RALT] = Event::Input::Keyboard::RAlt;
        SDL2MetalContext::MapSDLKeyboard[SDLK_RGUI] = Event::Input::Keyboard::RSystem;
        SDL2MetalContext::MapSDLKeyboard[SDLK_MENU] = Event::Input::Keyboard::Menu;
        SDL2MetalContext::MapSDLKeyboard[SDLK_LEFTBRACKET] = Event::Input::Keyboard::LBracket;
        SDL2MetalContext::MapSDLKeyboard[SDLK_RIGHTBRACKET] = Event::Input::Keyboard::RBracket;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_COLON] = Event::Input::Keyboard::SemiColon;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_COMMA] = Event::Input::Keyboard::Comma;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_PERIOD] = Event::Input::Keyboard::Period;
        SDL2MetalContext::MapSDLKeyboard[SDLK_QUOTE] = Event::Input::Keyboard::Quote;
        SDL2MetalContext::MapSDLKeyboard[SDLK_SLASH] = Event::Input::Keyboard::Slash;
        SDL2MetalContext::MapSDLKeyboard[SDLK_BACKSLASH] = Event::Input::Keyboard::BackSlash;
        SDL2MetalContext::MapSDLKeyboard[SDLK_EQUALS] = Event::Input::Keyboard::Equal;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_MINUS] = Event::Input::Keyboard::Dash;
        SDL2MetalContext::MapSDLKeyboard[SDLK_SPACE] = Event::Input::Keyboard::Space;
        SDL2MetalContext::MapSDLKeyboard[SDLK_RETURN] = Event::Input::Keyboard::Return;
        SDL2MetalContext::MapSDLKeyboard[SDLK_AC_BACK] = Event::Input::Keyboard::Back;
        SDL2MetalContext::MapSDLKeyboard[SDLK_TAB] = Event::Input::Keyboard::Tab;
        SDL2MetalContext::MapSDLKeyboard[SDLK_PAGEUP] = Event::Input::Keyboard::PageUp;
        SDL2MetalContext::MapSDLKeyboard[SDLK_PAGEDOWN] = Event::Input::Keyboard::PageDown;
        SDL2MetalContext::MapSDLKeyboard[SDLK_END] = Event::Input::Keyboard::End;
        SDL2MetalContext::MapSDLKeyboard[SDLK_AC_HOME] = Event::Input::Keyboard::Home;
        SDL2MetalContext::MapSDLKeyboard[SDLK_INSERT] = Event::Input::Keyboard::Insert;
        SDL2MetalContext::MapSDLKeyboard[SDLK_DELETE] = Event::Input::Keyboard::Delete;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_MEMADD] = Event::Input::Keyboard::Add;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_MEMSUBTRACT] = Event::Input::Keyboard::Subtract;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_MULTIPLY] = Event::Input::Keyboard::Multiply;
        SDL2MetalContext::MapSDLKeyboard[SDLK_KP_DIVIDE] = Event::Input::Keyboard::Divide;
        SDL2MetalContext::MapSDLKeyboard[SDLK_LEFT] = Event::Input::Keyboard::Left;
        SDL2MetalContext::MapSDLKeyboard[SDLK_RIGHT] = Event::Input::Keyboard::Right;
        SDL2MetalContext::MapSDLKeyboard[SDLK_UP] = Event::Input::Keyboard::Up;
        SDL2MetalContext::MapSDLKeyboard[SDLK_DOWN] = Event::Input::Keyboard::Down;
        SDL2MetalContext::MapSDLKeyboard[SDLK_0] = Event::Input::Keyboard::Numpad0;
        SDL2MetalContext::MapSDLKeyboard[SDLK_1] = Event::Input::Keyboard::Numpad1;
        SDL2MetalContext::MapSDLKeyboard[SDLK_2] = Event::Input::Keyboard::Numpad2;
        SDL2MetalContext::MapSDLKeyboard[SDLK_3] = Event::Input::Keyboard::Numpad3;
        SDL2MetalContext::MapSDLKeyboard[SDLK_4] = Event::Input::Keyboard::Numpad4;
        SDL2MetalContext::MapSDLKeyboard[SDLK_5] = Event::Input::Keyboard::Numpad5;
        SDL2MetalContext::MapSDLKeyboard[SDLK_6] = Event::Input::Keyboard::Numpad6;
        SDL2MetalContext::MapSDLKeyboard[SDLK_7] = Event::Input::Keyboard::Numpad7;
        SDL2MetalContext::MapSDLKeyboard[SDLK_8] = Event::Input::Keyboard::Numpad8;
        SDL2MetalContext::MapSDLKeyboard[SDLK_9] = Event::Input::Keyboard::Numpad9;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F1] = Event::Input::Keyboard::F1;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F2;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F3;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F4;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F5;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F16] = Event::Input::Keyboard::F6;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F17] = Event::Input::Keyboard::F7;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F18] = Event::Input::Keyboard::F8;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F19] = Event::Input::Keyboard::F9;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F10] = Event::Input::Keyboard::F10;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F11] = Event::Input::Keyboard::F11;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F12] = Event::Input::Keyboard::F12;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F13] = Event::Input::Keyboard::F13;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F14] = Event::Input::Keyboard::F14;
        SDL2MetalContext::MapSDLKeyboard[SDLK_F15] = Event::Input::Keyboard::F15;
        SDL2MetalContext::MapSDLKeyboard[SDLK_PAUSE] = Event::Input::Keyboard::Pause;
    }

    SDL2MetalContext::SDL2MetalContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : Context(width,height), metalView(NULL), metalDevice(NULL)
    {
        CreateKeyboardMap();

        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

        uint32 type = 0;
        if (windowType & WindowType::Fullscreen) { type = (type | SDL_WINDOW_FULLSCREEN); }
        if (windowType & WindowType::None) type = (type | SDL_WINDOW_BORDERLESS);
        if (windowType & WindowType::Resize) type = (type | SDL_WINDOW_RESIZABLE);
        // No SDL_GL_SetAttribute/SDL_GL_CreateContext (GL) and no
        // SDL_WINDOW_VULKAN (Vulkan) - this window's only job is to host a
        // CAMetalLayer, attached below via SDL_Metal_CreateView().
        type = type | SDL_WINDOW_METAL;

        rview = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            width,
            height,
            type
        );

        // Same HiDPI/drawable-vs-logical-size reasoning as
        // SDL2VulkanContext's constructor (see its comment) -
        // SDL_Metal_GetDrawableSize() is the Metal-path equivalent of
        // SDL_Vulkan_GetDrawableSize().
        int actualWidth = width, actualHeight = height;
        SDL_Metal_GetDrawableSize(rview, &actualWidth, &actualHeight);
        Width = (uint32)actualWidth;
        Height = (uint32)actualHeight;

        // Attach a CAMetalLayer-backed view to the window, then hand its
        // layer pointer to a fresh MetalRenderDevice - the entire "get a
        // window ready to draw" handshake (no VkInstance/VkSurfaceKHR
        // equivalents to create first, unlike Vulkan - see the header
        // comment on why). Ownership transfers to whichever IRenderer
        // gets constructed next via RegisterRenderDeviceForOwnership(),
        // same as SDL2VulkanContext::vulkanDevice - this class never
        // deletes metalDevice itself past that point.
        metalView = SDL_Metal_CreateView(rview);
        metalDevice = new MetalRenderDevice();
        if (metalView == NULL || !metalDevice->BindToLayer(SDL_Metal_GetLayer(metalView), Width, Height))
        {
            // Matches SDL2VulkanContext's constructor's existing
            // no-error-handling style - a NULL/unusable metalDevice
            // surfaces loudly the first time anything tries to use it.
            fprintf(stderr, "SDL2MetalContext: failed to bind MetalRenderDevice to a CAMetalLayer\n");
        }
        RegisterRenderDeviceForOwnership(metalDevice);
    }
    SDL2MetalContext::~SDL2MetalContext()
    {
        if (metalView != NULL)
            SDL_Metal_DestroyView(metalView);
        SDL_DestroyWindow(rview);
        SDL_Quit();
    }

    void SDL2MetalContext::OnResize(const uint32 width, const uint32 height)
    {
        Width = width;
        Height = height;

        // Unlike a Vulkan swapchain, a CAMetalLayer doesn't self-heal via
        // an OUT_OF_DATE/SUBOPTIMAL-style reactive signal at all - its
        // `drawableSize` is an explicit property the app must set, and
        // nothing does that automatically just because the NSView's frame
        // changed. Every real resize path (SDL_WINDOWEVENT_RESIZED below)
        // must reach this call, or nextDrawable keeps handing back
        // stale-size drawables the compositor then stretches - same
        // symptom SDL2VulkanContext::OnResize()'s comment describes for
        // the pre-fix Vulkan path, different mechanism.
        if (metalDevice != NULL)
            metalDevice->NotifySurfaceResized(width, height);
    }
    bool SDL2MetalContext::IsRunning() const
    {
        return Initialized;
    }
    void SDL2MetalContext::GetEvents()
    {
        SDL_Event sdl_event;
        while(SDL_PollEvent(&sdl_event) > 0)
        {
            if (ImGui::GetCurrentContext() != NULL)
                ImGui_ImplSDL2_ProcessEvent(&sdl_event);

            if (sdl_event.type == SDL_QUIT)
                Close();

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

            // Same window-vs-other-event-type union-aliasing guard as
            // SDL2VulkanContext::GetEvents() - see its comment.
            if (sdl_event.type == SDL_WINDOWEVENT && sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                int actualWidth = sdl_event.window.data1, actualHeight = sdl_event.window.data2;
                SDL_Metal_GetDrawableSize(rview, &actualWidth, &actualHeight);
                OnResize((uint32)actualWidth, (uint32)actualHeight);
            }
        }

        SetTime(SDL_GetTicks());
        fps.setFPS(SDL_GetTicks());
    }

    void SDL2MetalContext::Draw()
    {
        // Stub, same reasoning as SDL2VulkanContext::Draw() - the real
        // per-frame path goes through IRenderer/ForwardRenderer calling
        // MetalRenderDevice::BeginFrame()/EndFrame() directly, not through
        // this class.
    }

    void SDL2MetalContext::HideMouse()
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
    void SDL2MetalContext::ShowMouse()
    {
        SDL_ShowCursor(SDL_ENABLE);
    }

    void SDL2MetalContext::KeyPressed(const uint32 key)
    {
       SetKeyPressed(MapSDLKeyboard[key]);
    }
    void SDL2MetalContext::KeyReleased(const uint32 key)
    {
        SetKeyReleased(MapSDLKeyboard[key]);
    }
    void SDL2MetalContext::TextEntered(const uint32 Code)
    {
        SetCharEntered(Code);
    }
    void SDL2MetalContext::MouseButtonPressed(const uint32 button)
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
    void SDL2MetalContext::MouseButtonReleased(const uint32 button)
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
    void SDL2MetalContext::MouseMove(const f32 mousex, const f32 mousey)
    {
        SetMouseMove(mousex,mousey);
    }
    void SDL2MetalContext::MouseWheel(const f32 delta)
    {
        SetMouseWheel(delta);
    }
    void SDL2MetalContext::SetMousePosition(const uint32 mouseX, const uint32 mouseY)
    {
        SDL_WarpMouseInWindow(rview, mouseX, mouseY);
    }
    const Vec2 SDL2MetalContext::GetMousePosition() const
    {
        int mousex, mousey;
        SDL_GetMouseState(&mousex,&mousey);
        return Vec2(mousex,mousey);
    }
    void SDL2MetalContext::JoypadButtonPressed(const uint32 JoypadID, const uint32 Button)
    {
    }
    void SDL2MetalContext::JoypadButtonReleased(const uint32 JoypadID, const uint32 Button)
    {
    }
    void SDL2MetalContext::JoypadMove(const uint32 JoypadID, const uint32 Axis, const f32 Value)
    {
    }
    void SDL2MetalContext::Init() {}
    void SDL2MetalContext::Update() {}
    // Same ordering requirement as SDL2VulkanContext::Shutdown() - runs
    // after the example's own Shutdown() freed every GPU-backed object it
    // owns, since this must outlive all of them.
    void SDL2MetalContext::Shutdown()
    {
        if (metalDevice == NULL)
            return;
        metalDevice->WaitIdle();
        SetActiveRenderDevice(NULL);
        delete metalDevice;
        metalDevice = NULL;
    }
    void SDL2MetalContext::Close()
    {
        Initialized = false;
    }

}

#endif /* METAL_BACKEND */
