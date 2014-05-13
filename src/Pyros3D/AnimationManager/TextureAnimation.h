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

            // Yoyo
            bool yoyo;

            // Repeat
            int32 repeat;
            int32 _internalRepeat;

        public:

            // Constructor
            TextureAnimation(const int32 &fps);

            // Yoyo - 0 or negative numbers set it to infinite loop
            void YoYo(bool yo);

            // Add Frame
            void AddFrame(Texture* texture);

            // Get Resultant Texture
            Texture* GetTexture();

            // Void Update
            void Update(const f32 &time);

            // Destructor
            virtual ~TextureAnimation();

            // Play, Stop, Pause
            void Play(const int32 &Repeat = 1);
            void Pause();
            void Stop();
    };
    
};

#endif /* TEXTUREANIMATION_H */