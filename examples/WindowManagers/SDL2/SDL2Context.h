//============================================================================
// Name        : SDL2Context.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL Context
//============================================================================

#ifndef SDL2CONTEXT_H
#define SDL2CONTEXT_H

#include <SDL2/SDL.h>
#include <SDL_mixer.h>
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
    
    class SDL2Context : public Context {
        public:
            
            SDL2Context(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType = WindowType::Fullscreen);
            ~SDL2Context();
            
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
            SDL_Window GetWindow();            
            void HideMouse();
            void ShowMouse();
            void SetMousePosition(const uint32 mouseX, const uint32 mouseY);
            const Vec2 GetMousePosition() const;

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
            
            // SDL2Context Context
            SDL_Window* rview;

            // GL Context
            SDL_GLContext mainGLContext;

            // Clock
            uint32 Clock;

            // Game FPS
            FPS fps;
            
            // save mouse positions
            f32 mouseX, mouseY;

            void CreateKeyboardMap();
            
    };

}

#endif	/* SDL2CONTEXT_H */
