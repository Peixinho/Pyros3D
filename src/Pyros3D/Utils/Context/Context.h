//============================================================================
// Name        : Context.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a Context
//============================================================================

#ifndef CONTEXT_H
#define	CONTEXT_H

#include "../../Core/Math/Math.h"
#include "../../Utils/DeltaTime/DeltaTime.h"
#include "../../SceneGraph/SceneGraph.h"
#include "../../Core/InputManager/InputManager.h"

namespace p3d {

    class Context {
        public:

            Context(const unsigned int &width, const unsigned int &height);            
            virtual ~Context();

            f64 GetTime();
            f64 GetTimeInterval();
            void ActivateBulletTime(const f32 &factor);
            void DeactivateBulletTime();
            void Pause();
            void Resume();
            
            // Init
            virtual void Init() = 0;
            // Update GameObjects
            virtual void Update() = 0;                 
            // Shutdown
            virtual void Shutdown() = 0;
            // Get Events
            virtual void GetEvents() = 0;
            // Draw
            virtual void Draw() = 0;
            // Resize
            virtual void OnResize(const uint32 &width, const uint32 &height) = 0;
            // Is Running		
            virtual bool IsRunning() const = 0;

        protected:
            
            // Buttons and Mouse
            virtual void KeyPressed(const unsigned &key) = 0;
            virtual void KeyReleased(const unsigned &key) = 0;
            virtual void MouseButtonPressed(const unsigned &button) = 0;
            virtual void MouseButtonReleased(const unsigned &button) = 0;
            virtual void MouseMove(const f32 &mousex, const f32 &mousey) = 0;
            virtual void MouseWheel(const f32 &delta) = 0;
            virtual void JoypadButtonPressed(const unsigned &JoypadID, const unsigned &Button) = 0;
            virtual void JoypadButtonReleased(const unsigned &JoypadID, const unsigned &Button) = 0;
            virtual void JoypadMove(const unsigned &JoypadID, const unsigned &Button, const f32 &Value) = 0;

            // Setters for InputManager
            void SetKeyPressed(const unsigned &key);
            void SetKeyReleased(const unsigned &key);
            void SetCharEntered(const unsigned &key);
            void SetMouseButtonPressed(const unsigned &button);
            void SetMouseButtonReleased(const unsigned &button);
            void SetMouseMove(const f32 &mousex, const f32 &mousey);
            void SetMouseWheel(const f32 &delta);
            void SetJoypadButtonPressed(const unsigned &JoypadID, const unsigned &Button);
            void SetJoypadButtonReleased(const unsigned &JoypadID, const unsigned &Button);
            void SetJoypadMove(const unsigned &JoypadID, const unsigned &Button, const f32 &Value);

            void SetTime(const f32 &Timer);

            // Window Size
            unsigned int Width, Height;

            // Close Context
            void Close();
            
            // Initialized Flag
            bool Initialized;
            
        private:

            // Delta Time
            DeltaTime deltaTime;                        

    };
}

#endif	/* CONTEXT_H */