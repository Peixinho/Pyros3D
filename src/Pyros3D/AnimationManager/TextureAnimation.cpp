//============================================================================
// Name        : TextureAnimation.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animated Textures
//============================================================================

#include "TextureAnimation.h"
#include <math.h>

namespace p3d {
    
    TextureAnimation::TextureAnimation(const int32 &fps) 
    {
        isPlaying           = isLooping = false;
        FrameSpeed          = fps;
        isPaused            = false;
        timerPauseLength    = 0.0f;
	_frame = 0;
    }

    TextureAnimation::~TextureAnimation()
    {

    }

    Texture* TextureAnimation::GetTexture()
    {
        if (isPlaying)
        {
            // Actual Frame to be Played
            f32 _frameSpeed = (f32)Frames.size()/(f32)FrameSpeed;
            f32 _timer = timer-(timeStart+timerPauseLength);
            if (!isPaused) _frame = static_cast<int32>(ceil(_timer/_frameSpeed)-1);
           if (_frame<Frames.size())
            {
                return Frames[_frame];
            } else if (isLooping) {
                timeStart = timer;
		timerPauseLength = 0;
		_frame = 0;
            }
        }
        return Frames[_frame];
    }

    void TextureAnimation::AddFrame(Texture* texture)
    {
        Frames.push_back(texture);
    }

    void TextureAnimation::Update(const f32 &time)
    {
        timer = time;
    }
    
    void TextureAnimation::Play(bool loop)
    {
        isLooping = loop;
        timeStart = timer;
        isPlaying = true;
    }
    void TextureAnimation::Pause()
    {
        if (!isPaused)
        {
            isPaused = true;
            timerPauseStart = timer;
        } else {
            isPaused = false;
            timerPauseEnd = timer;
            timerPauseLength += timerPauseEnd-timerPauseStart;
        }
    }
    void TextureAnimation::Stop()
    {
        isPlaying = false;
    }

}
