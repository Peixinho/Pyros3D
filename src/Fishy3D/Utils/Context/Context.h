//============================================================================
// Name        : Context.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a Context
//============================================================================

#ifndef CONTEXT_H
#define	CONTEXT_H

#include "../Pointers/SuperSmartPointer.h"
#include "../../Core/Math/vec4.h"
#include "../../Utils/DeltaTime/DeltaTime.h"
#include "../../SceneGraph/SceneGraph.h"
#include "../../Renderer/IRenderer.h"
#include <assert.h>

namespace Fishy3D {

    class Context {
        public:

            Context(const unsigned int &width, const unsigned int &height);            
            virtual ~Context();

            long double GetTime();
            long double GetTimeInterval();
            void ActivateBulletTime(const float &factor);
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
            virtual void OnResize(const int &width, const int &height) = 0;            
            // Is Running		
            virtual bool IsRunning() const = 0;

        protected:
            
            // Buttons and Mouse
            virtual void KeyPressed(const unsigned &key) = 0;
            virtual void KeyReleased(const unsigned &key) = 0;
            virtual void MouseButtonPressed(const unsigned &button) = 0;
            virtual void MouseButtonReleased(const unsigned &button) = 0;
            virtual void MouseMove(const float &mousex, const float &mousey) = 0;
            virtual void MouseWheel(const float &delta) = 0;
            virtual void JoypadButtonPressed(const unsigned &JoypadID, const unsigned &Button) = 0;
            virtual void JoypadButtonReleased(const unsigned &JoypadID, const unsigned &Button) = 0;
            virtual void JoypadMove(const unsigned &JoypadID, const unsigned &Button, const float &Value) = 0;

            void SetTime(const float &Timer);

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