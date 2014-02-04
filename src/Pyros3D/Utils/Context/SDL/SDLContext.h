//============================================================================
// Name        : SDLContext.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SDL Context
//============================================================================

#ifndef SDLContext_H
#define SDLContext_H

#include <SDL2/SDL.h>

#include "../Context.h"
#include "../../../Utils/FPS/FPS.h"
#include "../../../Utils/DeltaTime/DeltaTime.h"
#include "../../../Core/InputManager/InputManager.h"

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
            
            SDLContext(const uint32 &width, const uint32 &height, const std::string &title, const unsigned int &windowType = WindowType::Fullscreen);            
            ~SDLContext();
            
            // virtuals
            virtual void Init();
            virtual void Update();
            virtual void Shutdown();
            virtual void GetEvents();
            virtual void Draw();
            virtual bool IsRunning() const;
            virtual void OnResize(const uint32 &width, const uint32 &height);
            void Close();

            // methods
            SDL_Window GetWindow();            
            void HideMouse();
            void ShowMouse();
            void SetMousePosition(const unsigned &mouseX, const unsigned &mouseY);
            const Vec2 GetMousePosition() const;

        protected:

            static std::map<uint32, uint32> MapSDLKeyboard;

            // Buttons and Mouse
            void KeyPressed(const unsigned &key);
            void KeyReleased(const unsigned &key);
            void TextEntered(const unsigned &Code);
            void MouseButtonPressed(const unsigned &button);
            void MouseButtonReleased(const unsigned &button);                            
            void MouseMove(const f32 &mousex, const f32 &mousey);                
            void MouseWheel(const f32 &delta);
            void JoypadButtonReleased(const unsigned &JoypadID, const unsigned &Button);
            void JoypadButtonPressed(const unsigned &JoypadID, const unsigned &Button);
            void JoypadMove(const unsigned &JoypadID, const unsigned &Axis, const f32 &Value);
            
            // SDLContext Context
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

#endif	/* SDLContext_H */
