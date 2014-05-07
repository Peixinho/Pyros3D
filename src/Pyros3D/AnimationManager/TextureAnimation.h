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
    
    class TextureAnimation {
        
        private:

            // internal timer
            f32 timer;
            // frames
            std::vector<Texture*> Frames;
            // initial timer
            f32 timeStart;
            // Pause
            f32 timerPauseEnd;
            f32 timerPauseStart;
            f32 timerPauseLength;
            bool isPaused;
            // Frame Speed
            int32 FrameSpeed;
            // flags
            bool isPlaying;
            bool isLooping;

            // Last Texture Bound
            int32 _frame;

        public:

            // Constructor
            TextureAnimation(const int32 &fps = 30);

            // Add Frame
            void AddFrame(Texture* texture);

            // Get Resultant Texture
            Texture* GetTexture();

            // Void Update
            void Update(const f32 &time);

            // Destructor
            virtual ~TextureAnimation();

            // Play, Stop, Pause
            void Play(bool loop = false);
            void Pause();
            void Stop();

	// Texture Ptr
	Texture* tex;
    };
    
};

#endif /* TEXTUREANIMATION_H */
