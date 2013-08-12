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
#include <assert.h>

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
            virtual void Resize(const int& width, const int& height);                       
            virtual bool IsRunning() const;
            virtual void OnResize(const int &width, const int &height);
            void Close();

            // methods
            void DisplayInfo(const std::string &text, const Vec2 &scale = Vec2(1.0,1.0), const Vec4 &color = Vec4(1.0,1.0,1.0,1.0));
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
            void MouseMove(const float &mousex, const float &mousey);                
            void MouseWheel(const float &delta);
            void JoypadButtonReleased(const unsigned &JoypadID, const unsigned &Button);
            void JoypadButtonPressed(const unsigned &JoypadID, const unsigned &Button);
            void JoypadMove(const unsigned &JoypadID, const unsigned &Axis, const float &Value);

            // SFMLContext Timer
            sf::Clock clock;
            
            // SFMLContext Context
            sf::RenderWindow rview;
            
            // Game FPS
            FPS fps;
            
            // save mouse positions
            float mouseX, mouseY;
            
            // Text
            sf::Text Text;
			sf::Font Font;
            
            // Fullscreen Modes
            std::vector<sf::VideoMode> modes;
            sf::View view;
            
            // Display Info
            bool displayInfo;
            
    };

}

#endif	/* SFMLContext_H */
