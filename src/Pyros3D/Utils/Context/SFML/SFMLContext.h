//============================================================================
// Name        : GameBase.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Template
//============================================================================

#ifndef SFMLContext_H
#define SFMLContext_H

#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>

#include "../Context.h"
#include "../../../Utils/FPS/FPS.h"
#include "../../../Utils/DeltaTime/DeltaTime.h"
#include "../../../Core/InputManager/InputManager.h"

namespace p3d {

    namespace WindowType {
        enum {
            Fullscreen        = 1 << 0,
            Resize             = 1 << 1,
            Close              = 1 << 2,           
            None               = 1 << 3,
            Titlebar           = 1 << 4
        };
    }
    
    class SFMLContext : public Context {
        public:            
            
            SFMLContext(const int &width, const int &height, const std::string &title, const unsigned int &windowType = WindowType::Fullscreen);            
            ~SFMLContext();
            
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
            sf::Window GetWindow();            
            void HideMouse();
            void ShowMouse();
            void SetMousePosition(const unsigned &mouseX, const unsigned &mouseY);
            const Vec2 GetMousePosition() const;

        protected:
            
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

            // SFMLContext Timer
            sf::Clock clock;
            
            // SFMLContext Context
            sf::RenderWindow rview;
            
            // Game FPS
            FPS fps;
            
            // save mouse positions
            f32 mouseX, mouseY;
            
            // Fullscreen Modes
            std::vector<sf::VideoMode> modes;
            sf::View view;
            
            // Get Events
            sf::Event event;
            
    };

}

#endif	/* SFMLContext_H */
