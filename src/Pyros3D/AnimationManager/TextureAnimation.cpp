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
        repeat              = 1;
        FrameSpeed          = fps;
        isPaused            = false;
        timerPauseLength    = 0.0f;
        _frame              = 0;
        _internalRepeat     = 0;
    }

    TextureAnimation::~TextureAnimation()
    {

    }

    Texture* TextureAnimation::GetTexture()
    {
        if (isPlaying)
        {
            // Actual Frame to be Played
            f32 _frameSpeed = (f32)FrameSpeed;
            f32 _timer = timer-(timeStart+timerPauseLength);

            // Get Frame
            if (!isPaused) _frame = static_cast<int32>(ceil(_timer/_frameSpeed)-1);

            // Check if there is Frames in Animation
            if (_frame<Frames.size())
            {
                return Frames[_frame];
            } else {
                if (isLooping) {
                    timeStart = timer;
                    timerPauseLength = 0;
                    _frame = 0;
                } else if (repeat>0 && _internalRepeat < repeat)
                {
                    timeStart = timer;
                    timerPauseLength = 0;
                    _frame = 0;
                    _internalRepeat++;
                } else {
                    _internalRepeat = 0;
                    isPlaying = false;
                    _frame = 0;
                    // On end Animation Call Back if needed
                }
            }
        }
        return Frames[_frame];
    }

    void TextureAnimation::YoYo(bool yo)
    {
        if (!yoyo && yo)
        {
            for (uint32 i=Frames.size();i>0;i--)
            {
                Frames.push_back(Frames[i-1]);
            }
            yoyo = true;
        }
        if (yoyo && !yo)
        {
            for (uint32 i=Frames.size();i>Frames.size()/2.f;i--)
            {
                Frames.pop_back();
            }
            yoyo = false;
        }
    }

    void TextureAnimation::AddFrame(Texture* texture)
    {
        Frames.push_back(texture);
    }

    void TextureAnimation::Update(const f32 &time)
    {
        timer = time;
    }
    
    void TextureAnimation::Play(const int32 &Repeat)
    {
        repeat = Repeat;

        // Set Loop
        if (repeat<=0) isLooping = true;
        else isLooping = false;

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