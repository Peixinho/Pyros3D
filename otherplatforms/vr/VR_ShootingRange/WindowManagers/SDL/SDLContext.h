//============================================================================
// Name        : SDLContext.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL Context
//============================================================================

#ifndef SDLCONTEXT_H
#define SDLCONTEXT_H

#include <SDL/SDL.h>
#if defined(EMSCRIPTEN)
    #include <SDL_mixer.h>
#endif
#include <Pyros3D/Core/Context/Context.h>

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
    
    class SDLContext : public Context {
        public:
            
            SDLContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType = WindowType::Fullscreen);
            ~SDLContext();
            
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
            
            // SDLContext Context
            SDL_Surface* rview;

            // Clock
            uint32 Clock;

            // Game FPS
            FPS fps;
            
            // save mouse positions
            f32 mouseX, mouseY;

            void CreateKeyboardMap();
            
    };

}

#endif	/* SDLCONTEXT_H */
