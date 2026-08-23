//============================================================================
// Name        : SDL2VulkanContext.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL2 Context for the Vulkan backend - see the header
//               comment for scope (Vulkan roadmap Phase 5 Step C).
//============================================================================

#include "SDL2VulkanContext.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "../FileDropHook.h"
#include "../CloseHook.h"

#ifdef VULKAN_BACKEND

#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#include <cstdio>

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#include <SDL2/SDL_syswm.h>
#include <cstdlib>

namespace {

// MoltenVK maps IMMEDIATE to Metal, but CAMetalLayer still defaults to
// displaySyncEnabled=YES on macOS, so nextDrawable paces to refresh (~60)
// even after we pick IMMEDIATE. Force it off on every CAMetalLayer under
// the SDL window (MoltenVK's view/layer and any subviews).
void DisableDisplaySyncOnLayer(id layer)
{
	if (!layer) return;
	Class metalCls = objc_getClass("CAMetalLayer");
	if (!metalCls) return;
	const bool isMetal = ((BOOL(*)(id, SEL, Class))objc_msgSend)(layer, sel_registerName("isKindOfClass:"), metalCls);
	if (!isMetal) return;
	((void(*)(id, SEL, BOOL))objc_msgSend)(layer, sel_registerName("setDisplaySyncEnabled:"), NO);
}

void DisableMetalDisplaySync(SDL_Window *window)
{
	if (!window) return;
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(window, &info)) return;
#if defined(SDL_VIDEO_DRIVER_COCOA)
	id nsWindow = (id)info.info.cocoa.window;
	if (!nsWindow) return;
	id contentView = ((id(*)(id, SEL))objc_msgSend)(nsWindow, sel_registerName("contentView"));
	if (!contentView) return;
	DisableDisplaySyncOnLayer(((id(*)(id, SEL))objc_msgSend)(contentView, sel_registerName("layer")));
	id subviews = ((id(*)(id, SEL))objc_msgSend)(contentView, sel_registerName("subviews"));
	if (!subviews) return;
	const unsigned long count = ((unsigned long(*)(id, SEL))objc_msgSend)(subviews, sel_registerName("count"));
	for (unsigned long i = 0; i < count; ++i)
	{
		id sub = ((id(*)(id, SEL, unsigned long))objc_msgSend)(subviews, sel_registerName("objectAtIndex:"), i);
		DisableDisplaySyncOnLayer(((id(*)(id, SEL))objc_msgSend)(sub, sel_registerName("layer")));
	}
#endif
}

} // namespace
#endif // __APPLE__

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
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        {
            echo(std::string("ERROR: SDL_Init failed: ") + SDL_GetError());
            exit(EXIT_FAILURE);
        }

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

        // SDL_CreateWindow with SDL_WINDOW_VULKAN fails outright when SDL
        // cannot load the Vulkan loader, which is the normal outcome on a
        // machine with no Vulkan-capable driver. Going on from here with a
        // NULL window is what turned that into a silent exit.
        if (!rview)
        {
            echo(std::string("ERROR: SDL_CreateWindow (Vulkan) failed: ") + SDL_GetError());
            echo("ERROR: this usually means no Vulkan driver is installed - try an OpenGL build.");
            exit(EXIT_FAILURE);
        }

        // If the window manager silently clamps the requested size (a
        // real thing SDL_CreateWindow() can't report through its return
        // value - only the window's own actual size afterward reflects
        // it), Width/Height (this class's own members, read by every
        // example for its camera's aspect ratio and G-buffer/render-
        // target sizing) must not stay at the stale, too-large requested
        // value - no SDL_WINDOWEVENT_RESIZED fires for a size decided at
        // creation time. SDL_Vulkan_GetDrawableSize() (not
        // SDL_GetWindowSize(), which returns logical points and would
        // undershoot by the display's HiDPI scale factor) is the correct
        // query for this - see its own doc comment: "This may differ
        // from SDL_GetWindowSize() if we're rendering to a high-DPI
        // drawable."
        //
        // NOTE: this does NOT fix the specific clipped-sphere-grid report
        // from this session - in that dev environment SDL itself reports
        // both SDL_GetWindowSize() and SDL_Vulkan_GetDrawableSize() as
        // the full requested 1280x720 (confirmed via temporary debug
        // prints), while the real on-screen window is macOS-clamped to
        // ~947x518 in a way SDL's own APIs never see (that machine's
        // actual display is 4K - no genuine screen-space constraint - so
        // this is a remote/screen-sharing session artifact, not SDL
        // mis-tracking a real resize). Kept anyway: it's still the
        // correct query for the general "window manager genuinely
        // resized the window and SDL knows about it" case, e.g. a tiling
        // window manager on Linux.
        int actualWidth = width, actualHeight = height;
        SDL_Vulkan_GetDrawableSize(rview, &actualWidth, &actualHeight);
        Width = (uint32)actualWidth;
        Height = (uint32)actualHeight;

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
            !vulkanDevice->InitializeSwapchain(surface, Width, Height))
        {
            // This used to fprintf and carry on, on the reasoning that an
            // unusable device would "surface loudly the first time anything
            // tries to use it". It does not: the first use is a call through
            // a null volk function pointer, which is an execute access
            // violation with no output at all. Stop here instead, where
            // there is still something useful to say.
            echo("ERROR: failed to initialize the Vulkan device or swapchain.");
            exit(EXIT_FAILURE);
        }
#if defined(__APPLE__)
        else
            DisableMetalDisplaySync(rview);
#endif
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
        // Returns SDL_FALSE without touching count when SDL has no Vulkan
        // loader, so a bare `extensions(count)` would size from whatever
        // was on the stack.
        if (SDL_Vulkan_GetInstanceExtensions(rview, &count, NULL) != SDL_TRUE)
        {
            echo(std::string("ERROR: SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
            return std::vector<const char*>();
        }
        std::vector<const char*> extensions(count);
        if (SDL_Vulkan_GetInstanceExtensions(rview, &count, extensions.data()) != SDL_TRUE)
        {
            echo(std::string("ERROR: SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
            return std::vector<const char*>();
        }
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

        // Proactively rebuild the swapchain here - see
        // IRenderDevice::NotifySurfaceResized()'s comment for why the
        // reactive-only (VK_ERROR_OUT_OF_DATE_KHR/VK_SUBOPTIMAL_KHR) path
        // alone isn't enough: MoltenVK doesn't reliably raise either for
        // a tiling-WM-driven resize, so without this call the swapchain
        // silently keeps presenting its old-size image forever, which
        // the compositor stretches into the window's real (now
        // different) bounds - permanently, since nothing ever prompts a
        // rebuild otherwise. vulkanDevice is only non-NULL once Init()
        // has constructed it (see its own header comment) - every real
        // OnResize() call happens well after that.
        if (vulkanDevice != NULL)
            vulkanDevice->NotifySurfaceResized(width, height);
#if defined(__APPLE__)
        DisableMetalDisplaySync(rview);
#endif

        // Deliberately does NOT call SDL_SetWindowSize() - every caller
        // of OnResize() (the real SDL_WINDOWEVENT_RESIZED handler and the
        // drawable-extent self-heal poll below, both in GetEvents()) is
        // reporting a size the window ALREADY IS, not requesting a new
        // one. Calling SDL_SetWindowSize(width, height) here used to feed
        // it back in - harmless if width/height happened to be in the
        // logical points SDL_SetWindowSize() expects, but the self-heal
        // poll passes physical pixels (see QueryRealSurfaceExtent()) to
        // stay consistent with Width/Height's established convention
        // (set from SDL_Vulkan_GetDrawableSize() at Init() - see its
        // comment). On any HiDPI/Retina display (scale factor != 1) that
        // requested the window grow to its own *physical* pixel count
        // *again*, which changed its real size, which the self-heal poll
        // then saw as yet another mismatch next frame - an unbounded
        // runaway resize loop fighting yabai's own tiling every tick,
        // paying for a full swapchain+G-buffer rebuild each time. Zero
        // real frames ever got through (FPS pinned at 0), and the last
        // frame that did looked stretched (the G-buffer and swapchain
        // mid-flight at different, no-longer-matching sizes). Nothing in
        // this codebase needs OnResize() to *request* a resize - only to
        // record one that already happened.
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
            // Process ImGui events first - guarded, same reasoning as
            // SDL2Context.cpp's identical line: ImGui_ImplSDL2_ProcessEvent()
            // asserts hard on a null backend/context if called before
            // ImGui::CreateContext() (InitImGui()'s job), and not every
            // example calls it.
            if (ImGui::GetCurrentContext() != NULL)
                ImGui_ImplSDL2_ProcessEvent(&sdl_event);

            if (sdl_event.type == SDL_QUIT
                || (sdl_event.type == SDL_WINDOWEVENT && sdl_event.window.event == SDL_WINDOWEVENT_CLOSE))
            {
                if (PyrosWindowClose::AllowClose())
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

            if (sdl_event.type == SDL_DROPFILE && sdl_event.drop.file)
            {
                PyrosFileDrop::Notify(sdl_event.drop.file);
                SDL_free(sdl_event.drop.file);
            }

            // Adjust the viewport when the window is resized. `window` is
            // only a valid member of this union when type==SDL_WINDOWEVENT
            // - reading sdl_event.window.event unconditionally (the
            // previous version of this check) reads whatever bytes another
            // event type's own struct happens to have at that offset (e.g.
            // SDL_MouseMotionEvent's `which`/`state` fields overlap it),
            // which can spuriously equal SDL_WINDOWEVENT_RESIZED and call
            // OnResize() with garbage data1/data2 read from equally
            // unrelated fields - a real, silent source of an occasional
            // garbage-sized swapchain recreation during any event flood
            // (a tiling WM's resize/move/focus events are exactly that).
            if (sdl_event.type == SDL_WINDOWEVENT && sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                // sdl_event.window.data1/data2 are logical points (SDL's
                // own documented unit for SDL_WINDOWEVENT_RESIZED), but
                // Width/Height's established convention is physical
                // pixels - set that way at Init() via
                // SDL_Vulkan_GetDrawableSize() (see its comment) and
                // relied on by QueryRealSurfaceExtent()'s self-heal poll
                // below. Feeding logical points into OnResize() here
                // underscaled Width/Height by the display's HiDPI factor,
                // which the self-heal poll then "corrected" back up -
                // repeatedly, since OnResize() used to also call
                // SDL_SetWindowSize() with whatever unit it received,
                // see that comment for the runaway loop this produced.
                // Query the real drawable size directly instead, so both
                // call sites agree on units.
                int actualWidth = sdl_event.window.data1, actualHeight = sdl_event.window.data2;
                SDL_Vulkan_GetDrawableSize(rview, &actualWidth, &actualHeight);
                OnResize((uint32)actualWidth, (uint32)actualHeight);
            }
        }

        // Fallback for window managers that resize the window without
        // ever delivering SDL_WINDOWEVENT_RESIZED - tiling window
        // managers reposition/resize windows through the Accessibility
        // API rather than a normal Cocoa resize drag, and SDL doesn't
        // always translate that into a native resize event. A first
        // version of this polled SDL_Vulkan_GetDrawableSize() - proven
        // wrong, not just theoretically insufficient: real debug prints
        // showed it kept reporting the original requested size (e.g.
        // 1280x720) for the entire lifetime of a window a tiling WM had
        // already visibly resized to something completely different.
        // Querying the real swapchain surface extent directly from
        // Vulkan instead - vkGetPhysicalDeviceSurfaceCapabilitiesKHR's
        // currentExtent can't go stale the way SDL's own bookkeeping
        // does, since it's not SDL's bookkeeping; VulkanRenderDevice::
        // RecreateSwapchain()'s own self-heal path (on
        // VK_ERROR_OUT_OF_DATE_KHR) already relies on the exact same
        // query. That self-heal only ever fixed the swapchain itself,
        // though - Width/Height (and everything downstream: camera
        // aspect ratio, DeferredRenderer's separately-sized G-buffer
        // textures) never found out, leaving a real, persistent size
        // mismatch between the swapchain and whatever's compositing into
        // it - reported as "resize sometimes hangs or draws garbage
        // under this WM" on the deferred pipeline specifically (forward
        // rendering draws directly into the swapchain's own render pass
        // each frame, so it has no separately-sized buffer to fall out
        // of sync with - not a coincidence that only deferred examples
        // were reported broken). Polling every frame and diffing against
        // the last known size is the general, WM-agnostic fix - cheap
        // (this query is O(1), no allocation) and idempotent when
        // nothing has actually changed.
        uint32 realW = 0, realH = 0;
        if (vulkanDevice->QueryRealSurfaceExtent(realW, realH) && (realW != Width || realH != Height))
            OnResize(realW, realH);

		SetTime(SDL_GetTicks());
        fps.setFPS(SDL_GetTicks());
#if defined(__APPLE__)
        // MoltenVK creates/replaces the CAMetalLayer on first present (and
        // sometimes on swapchain recreate). displaySyncEnabled defaults YES
        // → hard ~60 FPS even with IMMEDIATE. Must re-apply after the layer
        // exists; a one-shot at Init() runs too early and leaves vsync on.
        DisableMetalDisplaySync(rview);
#endif
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
    // Runs after the example's own Shutdown() has freed every GPU-backed
    // object it owns (each override calls ClassName::Shutdown() last), so
    // this is the only safe point to destroy the device - it must outlive
    // all of them. Ownership used to transfer to the first IRenderer, which
    // deleted it far too early; see ResolveInitialDevice() in IRenderer.cpp.
    void SDL2VulkanContext::Shutdown()
    {
        if (vulkanDevice == NULL)
            return;
        vulkanDevice->WaitIdle();
        // Nothing may resolve it after this point - GetActiveRenderDevice()
        // would otherwise keep handing out a dangling pointer.
        SetActiveRenderDevice(NULL);
        delete vulkanDevice;
        vulkanDevice = NULL;
    }
    void SDL2VulkanContext::Close()
    {
        Initialized = false;
    }

}

#endif /* VULKAN_BACKEND */
