//============================================================================
// Name        : SDL2MetalContext.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL2 Context for the native Metal backend - mirrors
//               SDL2VulkanContext (deliberately a sibling, not a shared
//               base, same reasoning as that file's header comment: the GL
//               and Vulkan paths stay untouched). Only built when CMake
//               finds Metal.framework/QuartzCore.framework (macOS/iOS,
//               BUILD_METAL_BACKEND) - the whole header is compiled out
//               otherwise, same pattern as VulkanRenderDevice.h/
//               SDL2VulkanContext.h.
//
//               Plain C++, not Objective-C++: SDL's own SDL_Metal_* API
//               already hides CAMetalLayer/NSView behind opaque `void*`
//               handles (SDL_MetalView, and SDL_Metal_GetLayer()'s return),
//               so this class never touches a real Objective-C type -
//               unlike MetalRenderDevice, which does and is therefore a
//               .mm file.
//============================================================================

#ifndef SDL2METALCONTEXT_H
#define SDL2METALCONTEXT_H

#ifdef METAL_BACKEND

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>
#include "Pyros3D/Core/Context/Context.h"

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

    // Owns the SDL window (created with SDL_WINDOW_METAL) plus the
    // SDL_MetalView attached to it - the two things MetalRenderDevice needs
    // to get from "a window" to "a working CAMetalLayer": SDL_Metal_GetLayer()'s
    // result, handed to MetalRenderDevice::BindToLayer() once in the
    // constructor. Unlike SDL2VulkanContext there is no separate instance/
    // device/surface split to reason about - Metal has no VkInstance-style
    // object and no per-platform "create a surface" step; MTLCreateSystemDefaultDevice()
    // + a CAMetalLayer pointer is the entire handshake.
    class SDL2MetalContext : public Context {
        public:

            SDL2MetalContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType = WindowType::Fullscreen);
            ~SDL2MetalContext();

            // virtuals
            virtual void Init();
            virtual void Update();
            virtual void Shutdown();
            virtual void GetEvents();
            virtual void Draw();
            virtual bool IsRunning() const;
            virtual void OnResize(const uint32 width, const uint32 height);
            void Close();

            // methods
            SDL_Window* GetSDLWindow() const { return rview; }
            // Non-owning - see the header comment on `metalDevice` for the
            // real ownership story (RegisterRenderDeviceForOwnership()).
            // Exists for standalone diagnostics (MetalHelloWindow) that
            // never construct a real IRenderer and so never go through
            // TakeRenderDeviceOwnership()/SetActiveRenderDevice() - normal
            // example code should keep going through GetActiveRenderDevice()
            // once a real IRenderer exists, not this.
            class MetalRenderDevice* GetMetalRenderDevice() const { return metalDevice; }
            void HideMouse();
            void ShowMouse();
            void SetMousePosition(const uint32 mouseX, const uint32 mouseY);
            const Vec2 GetMousePosition() const;

            // Time
            virtual f64 GetTime() { return SDL_GetTicks() * 0.001; }
            virtual f64 GetTimeMilliSeconds() { return SDL_GetTicks(); }
            virtual f64 GetTimeMicroSeconds() { return SDL_GetTicks(); }

        protected:

            static std::map<uint32, uint32> MapSDLKeyboard;

            // Buttons and Mouse
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

            // SDL2MetalContext Context
            SDL_Window* rview;
            SDL_MetalView metalView;

            // Constructed in the ctor (right after the window + view - a
            // CAMetalLayer can only be bound once one exists), then handed
            // off via RegisterRenderDeviceForOwnership() (see
            // IRenderDevice.h) - same ownership-transfer pattern as
            // SDL2VulkanContext::vulkanDevice, see its comment.
            class MetalRenderDevice* metalDevice;

            // Clock
            uint32 Clock;

            // Game FPS
            FPS fps;

            // save mouse positions
            f32 mouseX, mouseY;

            void CreateKeyboardMap();

    };

}

#endif /* METAL_BACKEND */

#endif	/* SDL2METALCONTEXT_H */
