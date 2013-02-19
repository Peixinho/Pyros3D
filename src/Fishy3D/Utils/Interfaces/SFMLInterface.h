//============================================================================
// Name        : GameBase.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Template
//============================================================================

#ifndef SFMLINTERFACE_H
#define SFMLINTERFACE_H

#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <assert.h>

#include "../../Core/Projection/Projection.h"
#include "../../Renderer/IRenderer.h"
#include "../../Core/GameObjects/GameObject.h"
#include "../../SceneGraph/SceneGraph.h"
#include "../../Utils/Pointers/SuperSmartPointer.h"
#include "../../Utils/FPS/FPS.h"
#include "../../Utils/DeltaTime/DeltaTime.h"
#include "../../Utils/Context/Context.h"
#include "../../Core/InputManager/InputManager.h"

namespace Fishy3D {

    namespace WindowType {
        enum {
            Fullscreen        = 1 << 0,
            Resize             = 1 << 1,
            Close              = 1 << 2,           
            None               = 1 << 3,
            Titlebar           = 1 << 4
        };
    }
    
    class SFMLInterface : public Context {
        public:            
            
            SFMLInterface(const int &width, const int &height, const std::string &title, const unsigned int &windowType = WindowType::Fullscreen);            
            ~SFMLInterface();
            
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
            void DisplayInfo(const std::string &text, const vec2 &scale = vec2(1.0,1.0), const vec4 &color = vec4(1.0,1.0,1.0,1.0));
            sf::Window GetWindow();            
            void HideMouse();
            void ShowMouse();
            void SetMousePosition(const unsigned &mouseX, const unsigned &mouseY);
            const vec2 &GetMousePosition() const;

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

            // SFML Timer
            sf::Clock clock;
            
            // SFML Context
            sf::RenderWindow rview;
            
            // Game FPS
            FPS fps;
            
            // save mouse positions
            float mouseX, mouseY;
            
            // Logo
            sf::Texture Logo;
            sf::Sprite LogoSprite;
            
            // Text
            sf::Text Text;            
            
            // Fullscreen Modes
            std::vector<sf::VideoMode> modes;
            sf::View view;
            
            // Display Info
            bool displayInfo;
            
    };

}

#endif	/* SFMLINTERFACE_H */
