//============================================================================
// Name        : SDL2Context.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See SDL2Context.h
//============================================================================

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include <chrono>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Core/InputManager/InputActions.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include "SDL2Context.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "../FileDropHook.h"
#include "../TextInputHook.h"
#include "../CloseHook.h"
#include "../GamepadHook.h"
#include <SDL2/SDL_syswm.h>
#if defined(__APPLE__)
#include <SDL2/SDL_metal.h>
#endif
#include <cstdlib>
#include <cstring>

namespace p3d {

    std::map<uint32, uint32> SDL2Context::MapSDLKeyboard;

    void SDL2Context::CreateKeyboardMap()
    {
        using namespace Event::Input;
        std::map<uint32, uint32> &k = MapSDLKeyboard;
        if (!k.empty()) return;
        for (int i = 0; i < 26; i++) k[SDLK_a + i] = Keyboard::A + i;
        // (the row above the letters is Num0..9, the keypad's digits Numpad0..9 - InputManager.h's names)
        for (int i = 0; i < 10; i++) k[SDLK_0 + i] = Keyboard::Num0 + i;
        k[SDLK_KP_0] = Keyboard::Numpad0;
        for (int i = 0; i < 9; i++) k[SDLK_KP_1 + i] = Keyboard::Numpad1 + i;
        for (int i = 0; i < 12; i++) k[SDLK_F1 + i] = Keyboard::F1 + i;
        k[SDLK_F13] = Keyboard::F13; k[SDLK_F14] = Keyboard::F14; k[SDLK_F15] = Keyboard::F15;
        k[SDLK_ESCAPE] = Keyboard::Escape;
        k[SDLK_LCTRL] = Keyboard::LControl; k[SDLK_LSHIFT] = Keyboard::LShift; k[SDLK_LALT] = Keyboard::LAlt; k[SDLK_LGUI] = Keyboard::LSystem;
        k[SDLK_RCTRL] = Keyboard::RControl; k[SDLK_RSHIFT] = Keyboard::RShift; k[SDLK_RALT] = Keyboard::RAlt; k[SDLK_RGUI] = Keyboard::RSystem;
        k[SDLK_MENU] = Keyboard::Menu;
        k[SDLK_LEFTBRACKET] = Keyboard::LBracket; k[SDLK_RIGHTBRACKET] = Keyboard::RBracket;
        k[SDLK_SEMICOLON] = Keyboard::SemiColon; k[SDLK_COMMA] = Keyboard::Comma; k[SDLK_PERIOD] = Keyboard::Period;
        k[SDLK_QUOTE] = Keyboard::Quote; k[SDLK_SLASH] = Keyboard::Slash; k[SDLK_BACKSLASH] = Keyboard::BackSlash;
        k[SDLK_BACKQUOTE] = Keyboard::Tilde; k[SDLK_EQUALS] = Keyboard::Equal; k[SDLK_MINUS] = Keyboard::Dash;
        k[SDLK_SPACE] = Keyboard::Space; k[SDLK_RETURN] = Keyboard::Return; k[SDLK_KP_ENTER] = Keyboard::Return;
        k[SDLK_BACKSPACE] = Keyboard::Back; k[SDLK_AC_BACK] = Keyboard::Back; k[SDLK_TAB] = Keyboard::Tab;
        k[SDLK_PAGEUP] = Keyboard::PageUp; k[SDLK_PAGEDOWN] = Keyboard::PageDown; k[SDLK_END] = Keyboard::End; k[SDLK_HOME] = Keyboard::Home;
        k[SDLK_INSERT] = Keyboard::Insert; k[SDLK_DELETE] = Keyboard::Delete;
        k[SDLK_KP_PLUS] = Keyboard::Add; k[SDLK_KP_MINUS] = Keyboard::Subtract; k[SDLK_KP_MULTIPLY] = Keyboard::Multiply; k[SDLK_KP_DIVIDE] = Keyboard::Divide;
        k[SDLK_LEFT] = Keyboard::Left; k[SDLK_RIGHT] = Keyboard::Right; k[SDLK_UP] = Keyboard::Up; k[SDLK_DOWN] = Keyboard::Down;
        k[SDLK_PAUSE] = Keyboard::Pause;
    }

    void SDL2Context::DrawableSize(int &width, int &height) const
    {
#ifdef EMSCRIPTEN
        // (the page sizes the canvas, in device pixels: it is the truth)
        emscripten_get_canvas_element_size("#canvas", &width, &height);
#elif defined(__APPLE__)
        if (metalView != NULL) { SDL_Metal_GetDrawableSize(rview, &width, &height); return; }
        SDL_GetWindowSizeInPixels(rview, &width, &height);
#else
        SDL_GetWindowSizeInPixels(rview, &width, &height);
#endif
    }

    gfx::Device* SDL2Context::CreateDevice()
    {
        gfx::SurfaceSource src;
        src.width = Width; src.height = Height; src.vsync = true;
#ifdef EMSCRIPTEN
        src.kind = gfx::SurfaceSource::Canvas; src.a = (void*)"#canvas";
#elif defined(__APPLE__)
        if (metalView == NULL) return NULL;
        src.kind = gfx::SurfaceSource::MetalLayer; src.a = SDL_Metal_GetLayer((SDL_MetalView)metalView);
#else
        SDL_SysWMinfo wm; SDL_VERSION(&wm.version);
        if (!SDL_GetWindowWMInfo(rview, &wm)) { echo(std::string("ERROR: the window's native handle: ") + SDL_GetError()); return NULL; }
#if defined(_WIN32)
        src.kind = gfx::SurfaceSource::Win32; src.a = (void*)wm.info.win.window; src.b = (void*)wm.info.win.hinstance;
#else
        src.kind = gfx::SurfaceSource::Xlib; src.a = (void*)wm.info.x11.display; src.b = (void*)(uintptr_t)wm.info.x11.window;
#endif
#endif
        // (the one asked for, among those built; else the build's own, Metal before Vulkan before WebGPU)
        const char* asked = getenv("PYROS_GFX_BACKEND");
        const std::string want = asked ? asked : "";
        gfx::Device* d = NULL;
#ifdef PYROS_GFX_WEBGPU
        if (want == "webgpu") d = gfx::CreateWebGPUDevice(src);
#endif
#ifdef PYROS_GFX_VULKAN
        if (d == NULL && want == "vulkan") d = gfx::CreateVulkanDevice(src);
#endif
#ifdef PYROS_GFX_METAL
        if (d == NULL && (want.empty() || want == "metal")) d = gfx::CreateMetalDevice(src);
#endif
#ifdef PYROS_GFX_VULKAN
        if (d == NULL && want != "webgpu") d = gfx::CreateVulkanDevice(src);
#endif
#ifdef PYROS_GFX_WEBGPU
        if (d == NULL) d = gfx::CreateWebGPUDevice(src);
#endif
        return d;
    }

    SDL2Context::SDL2Context(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : Context(width, height)
    {
        CreateKeyboardMap();

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        {
            echo(std::string("ERROR: SDL_Init failed: ") + SDL_GetError());
            return;
        }

        uint32 type = SDL_WINDOW_ALLOW_HIGHDPI;
        if (windowType & WindowType::Fullscreen) type |= SDL_WINDOW_FULLSCREEN;
        if (windowType & WindowType::None) type |= SDL_WINDOW_BORDERLESS;
        if (windowType & WindowType::Resize) type |= SDL_WINDOW_RESIZABLE;
#if defined(__APPLE__)
        // (a CAMetalLayer whichever API draws: Metal's, MoltenVK's, Dawn's)
        type |= SDL_WINDOW_METAL;
#if TARGET_OS_IPHONE
        type |= SDL_WINDOW_FULLSCREEN;
#endif
#endif

        rview = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, type);
        if (rview == NULL)
        {
            echo(std::string("ERROR: SDL_CreateWindow failed: ") + SDL_GetError());
            return;
        }
#if defined(__APPLE__)
        metalView = SDL_Metal_CreateView(rview);
#endif

        int w = (int)width, h = (int)height;
        DrawableSize(w, h);
        if (w > 0 && h > 0) { Width = (uint32)w; Height = (uint32)h; }

        device = CreateDevice();
        if (device != NULL)
            echo(std::string("SDL2Context: ") + device->Caps().name);
        else
            echo("ERROR: no graphics device could be made on this window (PYROS_GFX_BACKEND, and what the build has)");
    }

    SDL2Context::~SDL2Context()
    {
        Shutdown();
#if defined(__APPLE__)
        if (metalView != NULL)
            SDL_Metal_DestroyView((SDL_MetalView)metalView);
#endif
        if (rview != NULL)
            SDL_DestroyWindow(rview);
        SDL_Quit();
    }

    void SDL2Context::OnResize(const uint32 width, const uint32 height)
    {
        Width = width;
        Height = height;
        if (device != NULL)
            device->ResizeSurface(width, height);
    }

    bool SDL2Context::IsRunning() const
    {
        return Initialized;
    }

    void SDL2Context::GetEvents()
    {
        // (pads: opened here, the first time, rather than with the window - its own SDL subsystem)
        PyrosGamepad::Init();
        SDL_Event sdl_event;
        InputManager::BeginTouchFrame();
        while (SDL_PollEvent(&sdl_event) > 0)
        {
            if (PyrosGamepad::HandleEvent(sdl_event,
                [this](p3d::uint32 pad, p3d::uint32 b) { SetJoypadButtonPressed(pad, b); },
                [this](p3d::uint32 pad, p3d::uint32 b) { SetJoypadButtonReleased(pad, b); },
                [this](p3d::uint32 pad, p3d::uint32 axis, p3d::f32 v) { SetJoypadMove(pad, axis, v); }))
                continue;

            if (ImGui::GetCurrentContext() != NULL)
                ImGui_ImplSDL2_ProcessEvent(&sdl_event);

            if (sdl_event.type == SDL_QUIT
                || (sdl_event.type == SDL_WINDOWEVENT && sdl_event.window.event == SDL_WINDOWEVENT_CLOSE))
            {
                // (an application with unsaved work may say not yet - CloseHook.h)
                if (PyrosWindowClose::AllowClose())
                    Close();
            }

            if (sdl_event.type == SDL_TEXTINPUT)
                PyrosTextInput::Notify(sdl_event.text.text);

            if (sdl_event.type == SDL_KEYDOWN)
            {
                // (a held key repeats only where that edits text: the game sees a key go down once)
                const SDL_Keycode sym = sdl_event.key.keysym.sym;
                const bool edits = sym == SDLK_BACKSPACE || sym == SDLK_DELETE || sym == SDLK_LEFT || sym == SDLK_RIGHT || sym == SDLK_UP || sym == SDLK_DOWN;
                if (sdl_event.key.repeat == 0 || edits) KeyPressed(sym);
            }

            if (sdl_event.type == SDL_KEYUP)
                KeyReleased(sdl_event.key.keysym.sym);

            if (sdl_event.type == SDL_MOUSEBUTTONDOWN)
                MouseButtonPressed(sdl_event.button.button);

            if (sdl_event.type == SDL_MOUSEBUTTONUP)
                MouseButtonReleased(sdl_event.button.button);

            if (sdl_event.type == SDL_MOUSEMOTION)
            {
                // (SDL's points to the picture's pixels: a high-density screen has more of them than points)
                int pw = 0, ph = 0;
                SDL_GetWindowSize(rview, &pw, &ph);
                const f32 kx = pw > 0 ? (f32)Width / (f32)pw : 1.f, ky = ph > 0 ? (f32)Height / (f32)ph : 1.f;
                InputManager::AddMouseDelta((f32)sdl_event.motion.xrel * kx, (f32)sdl_event.motion.yrel * ky);
                MouseMove((f32)sdl_event.motion.x * kx, (f32)sdl_event.motion.y * ky);
            }

            if (sdl_event.type == SDL_FINGERDOWN || sdl_event.type == SDL_FINGERMOTION || sdl_event.type == SDL_FINGERUP)
            {
                const f32 tx = sdl_event.tfinger.x * (f32)Width, ty = sdl_event.tfinger.y * (f32)Height;
                const int64 tid = (int64)sdl_event.tfinger.fingerId;
                if (sdl_event.type == SDL_FINGERDOWN) InputManager::TouchBegan(tid, tx, ty);
                else if (sdl_event.type == SDL_FINGERMOTION) InputManager::TouchMoved(tid, tx, ty);
                else InputManager::TouchEnded(tid, tx, ty);
            }

            if (sdl_event.type == SDL_MOUSEWHEEL)
                MouseWheel(sdl_event.wheel.y);

            if (sdl_event.type == SDL_DROPFILE && sdl_event.drop.file)
            {
                PyrosFileDrop::Notify(sdl_event.drop.file);
                SDL_free(sdl_event.drop.file);
            }
        }
        // (actions read the keys and pads as this frame left them)
        InputActions::NewFrame();

#ifdef EMSCRIPTEN
        // In a browser the page sizes the canvas and SDL is not told: what it calls the window's size is what was asked
        // for when it was made. Making SDL's window the canvas's size is also what puts the pointer in the picture's
        // pixels (SDL scales it by window size over element size).
        {
            int canvasW = 0, canvasH = 0, windowW = 0, windowH = 0;
            emscripten_get_canvas_element_size("#canvas", &canvasW, &canvasH);
            SDL_GetWindowSize(rview, &windowW, &windowH);
            if (canvasW > 0 && canvasH > 0 && (canvasW != windowW || canvasH != windowH))
                SDL_SetWindowSize(rview, canvasW, canvasH);
        }
#endif
        // The size the window really has, each frame: a tiling window manager can resize it without
        // SDL_WINDOWEVENT_RESIZED ever arriving.
        int w = 0, h = 0;
        DrawableSize(w, h);
        if (w > 0 && h > 0 && ((uint32)w != Width || (uint32)h != Height))
            OnResize((uint32)w, (uint32)h);

        frameClock.Tick((f64)SDL_GetPerformanceCounter() / (f64)SDL_GetPerformanceFrequency());
        SetTime(frameClock.Seconds() * 1000.0);
        fps.setFPS(SDL_GetTicks());
    }

    // (the device presents its frame itself: Device::EndFrame)
    void SDL2Context::Draw() {}

    void SDL2Context::HideMouse()
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
    void SDL2Context::ShowMouse()
    {
        SDL_ShowCursor(SDL_ENABLE);
    }

    void SDL2Context::KeyPressed(const uint32 key)
    {
        std::map<uint32, uint32>::const_iterator it = MapSDLKeyboard.find(key);
        if (it != MapSDLKeyboard.end()) SetKeyPressed(it->second);
    }
    void SDL2Context::KeyReleased(const uint32 key)
    {
        std::map<uint32, uint32>::const_iterator it = MapSDLKeyboard.find(key);
        if (it != MapSDLKeyboard.end()) SetKeyReleased(it->second);
    }
    void SDL2Context::TextEntered(const uint32 Code)
    {
        SetCharEntered(Code);
    }
    void SDL2Context::MouseButtonPressed(const uint32 button)
    {
        switch (button)
        {
            case SDL_BUTTON_LEFT: SetMouseButtonPressed(Event::Input::Mouse::Left); break;
            case SDL_BUTTON_MIDDLE: SetMouseButtonPressed(Event::Input::Mouse::Middle); break;
            case SDL_BUTTON_RIGHT: SetMouseButtonPressed(Event::Input::Mouse::Right); break;
        }
    }
    void SDL2Context::MouseButtonReleased(const uint32 button)
    {
        switch (button)
        {
            case SDL_BUTTON_LEFT: SetMouseButtonReleased(Event::Input::Mouse::Left); break;
            case SDL_BUTTON_MIDDLE: SetMouseButtonReleased(Event::Input::Mouse::Middle); break;
            case SDL_BUTTON_RIGHT: SetMouseButtonReleased(Event::Input::Mouse::Right); break;
        }
    }
    void SDL2Context::MouseMove(const f32 mousex, const f32 mousey)
    {
        SetMouseMove(mousex, mousey);
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
        SDL_GetMouseState(&mousex, &mousey);
        return Vec2((f32)mousex, (f32)mousey);
    }
    // (pads come through GamepadHook.h, which calls the Context's own setters)
    void SDL2Context::JoypadButtonPressed(const uint32 JoypadID, const uint32 Button) {}
    void SDL2Context::JoypadButtonReleased(const uint32 JoypadID, const uint32 Button) {}
    void SDL2Context::JoypadMove(const uint32 JoypadID, const uint32 Axis, const f32 Value) {}
    void SDL2Context::Init() {}
    void SDL2Context::Update() {}
    void SDL2Context::Shutdown()
    {
        // (after the application has let go of what it made on the device: its renderers first)
        if (device != NULL)
        {
            delete device;
            device = NULL;
        }
    }
    void SDL2Context::Close()
    {
        Initialized = false;
    }

}
