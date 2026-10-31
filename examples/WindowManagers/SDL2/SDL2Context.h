//============================================================================
// Name        : SDL2Context.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : The window: SDL2's, with the device the renderer draws with
//               on it (Device) - Metal on a CAMetalLayer, Vulkan on the
//               platform's window (MoltenVK on a Mac's layer), WebGPU on the
//               browser's canvas or, natively, through Dawn. Which one: the
//               build's (PYROS_GRAPHICS), or PYROS_GFX_BACKEND=metal|vulkan|webgpu
//               among those built.
//============================================================================

#ifndef SDL2CONTEXT_H
#define SDL2CONTEXT_H

#include <Pyros3D/Utils/FrameClock/FrameClock.h>
#include <SDL2/SDL.h>
#include "Pyros3D/Core/Context/Context.h"
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <map>
#include <string>

namespace p3d {

    namespace WindowType {
        enum {
            Fullscreen         = 1 << 0,
            Resize             = 1 << 1,
            Close              = 1 << 2,
            None               = 1 << 3,
            Titlebar           = 1 << 4
        };
    }

    class SDL2Context : public Context {
        public:

            SDL2Context(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType = WindowType::Fullscreen);
            ~SDL2Context();

            virtual void Init();
            virtual void Update();
            virtual void Shutdown();
            virtual void GetEvents();
            virtual void Draw();
            virtual bool IsRunning() const;
            virtual void OnResize(const uint32 width, const uint32 height);
            void Close();

            SDL_Window* GetSDLWindow() const { return rview; }
            // The device on the window: NULL when none of those built could be made (the window then shows nothing)
            gfx::Device* GetDevice() const { return device; }
            void HideMouse();
            void ShowMouse();
            void SetMousePosition(const uint32 mouseX, const uint32 mouseY);
            const Vec2 GetMousePosition() const;

            virtual f64 GetTime() { return frameClock.Seconds(); }
            virtual f64 GetTimeMilliSeconds() { return frameClock.Seconds() * 1000.0; }
            virtual f64 GetTimeMicroSeconds() { return frameClock.Seconds() * 1000000.0; }

        protected:

            static std::map<uint32, uint32> MapSDLKeyboard;

            virtual void KeyPressed(const uint32 key);
            virtual void KeyReleased(const uint32 key);
            virtual void TextEntered(const uint32 Code);
            virtual void MouseButtonPressed(const uint32 button);
            virtual void MouseButtonReleased(const uint32 button);
            virtual void MouseMove(const f32 mousex, const f32 mousey);
            virtual void MouseWheel(const f32 delta);
            virtual void JoypadButtonReleased(const uint32 JoypadID, const uint32 Button);
            virtual void JoypadButtonPressed(const uint32 JoypadID, const uint32 Button);
            virtual void JoypadMove(const uint32 JoypadID, const uint32 Axis, const f32 Value);

            SDL_Window* rview = NULL;
            // (a Mac's or an iPhone's: the CAMetalLayer the device draws on, whichever API it is)
            void* metalView = NULL;
            gfx::Device* device = NULL;

            FPS fps;
            FrameClock frameClock;

            void CreateKeyboardMap();
            // What the device draws: the window's size in pixels
            void DrawableSize(int &width, int &height) const;
            gfx::Device* CreateDevice();

    };

}

#endif	/* SDL2CONTEXT_H */
