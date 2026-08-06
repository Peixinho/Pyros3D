//============================================================================
// Name        : SDL2VulkanContext.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL2 Context for the Vulkan backend - Vulkan roadmap
//               Phase 5 Step C, see VULKAN_ROADMAP.md. Deliberately a
//               sibling of SDL2Context, not a modification of it in place,
//               so the GL path's window creation stays completely
//               untouched. Only built when CMake finds the Vulkan SDK +
//               vulkan-volk (BUILD_VULKAN_BACKEND) - the whole header is
//               compiled out otherwise, same pattern as VulkanRenderDevice.h.
//============================================================================

#ifndef SDL2VULKANCONTEXT_H
#define SDL2VULKANCONTEXT_H

#ifdef VULKAN_BACKEND

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <volk.h>
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

    // Owns only the SDL window (created with SDL_WINDOW_VULKAN, no GL
    // context) plus the two SDL_Vulkan integration points a render device
    // needs to get from "a window" to "a working swapchain": the required
    // instance extensions, and surface creation given an already-created
    // VkInstance. Deliberately does NOT create a VkInstance/VkDevice/
    // swapchain itself - see VULKAN_ROADMAP.md Phase 5 for why (in short:
    // VulkanRenderDevice already owns instance creation as of Step B, and
    // splitting instance-vs-device-vs-surface ownership across two classes
    // without a real swapchain to drive the design yet risks guessing the
    // wrong shape - Step D resolves this once there's a concrete target).
    // Draw() is a stub for the same reason: the real acquire/submit/present
    // sequence needs a real swapchain (VulkanRenderDevice, Step D) to call
    // into, which doesn't exist yet.
    class SDL2VulkanContext : public Context {
        public:

            SDL2VulkanContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType = WindowType::Fullscreen);
            ~SDL2VulkanContext();

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
            void HideMouse();
            void ShowMouse();
            void SetMousePosition(const uint32 mouseX, const uint32 mouseY);
            const Vec2 GetMousePosition() const;

            // Vulkan integration points - the two things a render device
            // needs from this window; both just forward to the matching
            // SDL_Vulkan_* call.
            std::vector<const char*> GetRequiredInstanceExtensions() const;
            bool CreateSurface(VkInstance instance, VkSurfaceKHR *outSurface) const;

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

            // SDL2VulkanContext Context
            SDL_Window* rview;

            // Constructed in the ctor (right after the window - a
            // VkSurfaceKHR can only be created once one exists), then
            // handed off via RegisterRenderDeviceForOwnership() (see
            // IRenderDevice.h) so whichever IRenderer gets constructed
            // next (every example's `new ForwardRenderer(Width, Height)`)
            // adopts and owns it - this class does NOT delete it and must
            // not touch it again after the ctor once ownership transfers,
            // except through the non-owning GetActiveRenderDevice()
            // reference the rest of the engine already uses. Kept as a
            // raw pointer (not owned here) purely so a later Draw()
            // implementation has something to call into once IRenderer's
            // real per-frame Vulkan path exists - see VULKAN_ROADMAP.md.
            class VulkanRenderDevice* vulkanDevice;

            // Clock
            uint32 Clock;

            // Game FPS
            FPS fps;

            // save mouse positions
            f32 mouseX, mouseY;

            void CreateKeyboardMap();

    };

}

#endif /* VULKAN_BACKEND */

#endif	/* SDL2VULKANCONTEXT_H */
