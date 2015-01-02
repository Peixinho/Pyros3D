//============================================================================
// Name        : Context.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a Context
//============================================================================

#ifndef CONTEXT_H
#define CONTEXT_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Utils/DeltaTime/DeltaTime.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Utils/FPS/FPS.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

    class PYROS3D_API Context {
        public:

            Context(const uint32 &width, const uint32 &height);            
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
            virtual void KeyPressed(const uint32 &key) = 0;
            virtual void KeyReleased(const uint32 &key) = 0;
            virtual void TextEntered(const uint32 &Code) = 0;
            virtual void MouseButtonPressed(const uint32 &button) = 0;
            virtual void MouseButtonReleased(const uint32 &button) = 0;
            virtual void MouseMove(const f32 &mousex, const f32 &mousey) = 0;
            virtual void MouseWheel(const f32 &delta) = 0;
            virtual void JoypadButtonPressed(const uint32 &JoypadID, const uint32 &Button) = 0;
            virtual void JoypadButtonReleased(const uint32 &JoypadID, const uint32 &Button) = 0;
            virtual void JoypadMove(const uint32 &JoypadID, const uint32 &Button, const f32 &Value) = 0;

            // Setters for InputManager
            void SetKeyPressed(const uint32 &key);
            void SetKeyReleased(const uint32 &key);
            void SetCharEntered(const uint32 &key);
            void SetMouseButtonPressed(const uint32 &button);
            void SetMouseButtonReleased(const uint32 &button);
            void SetMouseMove(const f32 &mousex, const f32 &mousey);
            void SetMouseWheel(const f32 &delta);
            void SetJoypadButtonPressed(const uint32 &JoypadID, const uint32 &Button);
            void SetJoypadButtonReleased(const uint32 &JoypadID, const uint32 &Button);
            void SetJoypadMove(const uint32 &JoypadID, const uint32 &Button, const f32 &Value);
            
            // Set Time
            void SetTime(const f32 &Timer);
            
            // Window Size
            uint32 Width, Height;

            // Close Context
            void Close();
            
            // Initialized Flag
            bool Initialized;
            
        private:

            // Delta Time
            DeltaTime deltaTime;                        

    };
}

#endif /* CONTEXT_H */