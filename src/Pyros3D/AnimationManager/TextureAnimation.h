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
#include <vector>

namespace p3d {

	class TextureAnimation;

  	class TextureAnimationInstance {

		friend class TextureAnimation;

		private:

			// initial timer
			f32 timeStart;
			// Frame Speed
			uint32 FrameSpeed;
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

		public:

			TextureAnimationInstance(TextureAnimation* owner, const uint32 &fps);

			// Play, Stop, Pause
			void Play(const int32 &Repeat = 1);
			void Pause();
			void Stop();

			void YoYo(bool yo);

			// Get Texture
			Texture* GetTexture();

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

			// Constructor
			TextureAnimation();

			// Add Frame
			void AddFrame(Texture* texture);

			// Void Update
			void Update(const f32 &time);

			// Destructor
			virtual ~TextureAnimation();

			// Instance
			TextureAnimationInstance* CreateInstance(const uint32 &fps = 30);

			// Destroy Instance
			void DestroyInstance(TextureAnimationInstance* Instance);
    };
    
};

#endif /* TEXTUREANIMATION_H */