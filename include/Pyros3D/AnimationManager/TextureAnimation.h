//============================================================================
// Name        : TextureAnimation.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animated Textures
//============================================================================

#ifndef TEXTUREANIMATION_H
#define TEXTUREANIMATION_H

#include "../AssetManager/Assets/Texture/Texture.h"
#include "../Ext/Signals/Signal.h"
#include "../Ext/Signals/Delegate.h"
#include <vector>

namespace p3d {

	class TextureAnimation;

  	class TextureAnimationInstance {

		friend class TextureAnimation;

		private:

			// initial timer
			f32 timeStart;
			// Frame Speed
			f32 FrameSpeed;
			// Pause
			f32 timerPauseEnd;
			f32 timerPauseStart;
			f32 timerPauseLength;
			bool isPaused;
			// flags
			bool isPlaying;
			bool isLooping;
			// Last Texture Bound
			int32 _frame;
			// Yoyo
			bool yoyo;
			// Repeat
			int32 repeat;
			int32 _internalRepeat;
			// Keep Owner PTR
			TextureAnimation* Owner;

			// CallBack Functions
			Gallant::Signal0<void> OnStartFunction;
			Gallant::Signal0<void> OnUpdateFunction;
			Gallant::Signal0<void> OnEndFunction;
			bool haveOnStartFunction;
			bool haveOnUpdateFunction;
			bool haveOnEndFunction;
			
		public:

			TextureAnimationInstance(TextureAnimation* owner, const f32 &fps);

			// Play, Stop, Pause
			void Play(const int32 &Repeat = 1);
			void Pause();
			void Stop();
			// Is Playing Animation
			bool IsPlaying();
			// Yoyo
			void YoYo(bool yo);

			// Get Texture
			Texture* GetTexture();
			// Get Frame
			const uint32 GetFrame() const;
			// CallBacks
			void OnStart(void (*func) (void));
			template< class X, class Y >
			void OnStart( Y * obj, void (X::*func)() )
			{
				haveOnStartFunction = true;
				OnStartFunction.Connect(obj,func);
			}
			
			void OnUpdate(void (*func) (void));
			template< class X, class Y >
			void OnUpdate( Y * obj, void (X::*func)() )
			{
				haveOnUpdateFunction = true;
				OnUpdateFunction.Connect(obj,func);
			}
			
			void OnEnd(void (*func) (void));
			template< class X, class Y >
			void OnEnd( Y * obj, void (X::*func)() )
			{
				haveOnEndFunction = true;
				OnEndFunction.Connect(obj,func);
			}

      };

      class TextureAnimation {

        friend class TextureAnimationInstance;
    
        private:

			// internal timer
			f32 timer;
			// frames
			std::vector<Texture*> Frames;
			// Instances
			std::vector<TextureAnimationInstance*> Instances;

		public:

			Texture* GetFrame(const uint32 &frame);
			uint32 GetNumberFrames();

			// Constructor
			TextureAnimation();

			// Add Frame
			void AddFrame(Texture* texture);

			// Void Update
			void Update(const f32 &time);

			// Destructor
			virtual ~TextureAnimation();

			// Instance
			TextureAnimationInstance* CreateInstance(const f32 &fps = 30);

			// Destroy Instance
			void DestroyInstance(TextureAnimationInstance* Instance);
    };
    
};

#endif /* TEXTUREANIMATION_H */