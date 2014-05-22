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
    
    TextureAnimation::TextureAnimation() 
    {
        timer 				= 0;
    }

    TextureAnimation::~TextureAnimation()
    {

    }

    TextureAnimationInstance::TextureAnimationInstance(TextureAnimation* owner, const uint32 &fps)
    {
        isPlaying           = isLooping = false;
        repeat              = 1;
        isPaused            = false;
        timerPauseLength    = 0.0f;
        _frame              = 0;
        _internalRepeat     = 1;
        yoyo 				= false;
        Owner 				= owner;
        FrameSpeed 			= fps;
    }

    Texture* TextureAnimation::GetFrame(const uint32 &frame)
    {
       return Frames[frame];
    }

    uint32 TextureAnimation::GetNumberFrames()
    {
        return Frames.size();
    }

    // Instance
    TextureAnimationInstance* TextureAnimation::CreateInstance(const uint32 &fps)
    {
        TextureAnimationInstance* i = new TextureAnimationInstance(this, fps);
        Instances.push_back(i);
        return i;
    }

    // Destroy Instance
    void TextureAnimation::DestroyInstance(TextureAnimationInstance* Instance)
    {
        for (std::vector<TextureAnimationInstance*>::iterator i=Instances.begin();i!=Instances.end();i++)
        {
            if ((*i)==Instance)
            {   
                delete Instance;
                Instances.erase(i);
                break;
            }
        }
    }

    Texture* TextureAnimationInstance::GetTexture()
    {
        return Owner->Frames[_frame];
    }

    void TextureAnimationInstance::YoYo(bool yo)
    {
        yoyo = yo;
    }

    void TextureAnimation::AddFrame(Texture* texture)
    {
        Frames.push_back(texture);
    }

    void TextureAnimation::Update(const f32 &time)
    {
        timer = time;

        for (std::vector<TextureAnimationInstance*>::iterator i=Instances.begin();i!=Instances.end();i++)
        {
            if ((*i)->isPlaying && !(*i)->isPaused)
            {
                // Actual Frame to be Played
                f32 _frameSpeed = 1.f/(f32)(*i)->FrameSpeed;
                f32 _timer = timer-((*i)->timeStart+(*i)->timerPauseLength);

                uint32 frameSize = static_cast<int32>((*i)->yoyo?Frames.size()*2:Frames.size());

                // Get Frame
                uint32 frame = 0;
                if (!(*i)->isPaused) frame = static_cast<int32>(ceil(_timer/_frameSpeed)-1);

                // Check if there is Frames in Animation
                if (frame<frameSize)
                {
                    if ((*i)->yoyo && frame>=Frames.size()) (*i)->_frame=static_cast<int32>(Frames.size()-(frame-Frames.size()+1));
                    else (*i)->_frame = frame;
                } else {
                    if ((*i)->isLooping) {
                        (*i)->timeStart = timer;
                        (*i)->timerPauseLength = 0;
                        (*i)->_frame = 0;
                    } else if ((*i)->repeat>0 && (*i)->_internalRepeat < (*i)->repeat)
                    {
                        (*i)->timeStart = timer;
                        (*i)->timerPauseLength = 0;
                        (*i)->_frame = 0;
                        (*i)->_internalRepeat++;
                    } else {
                        (*i)->_internalRepeat = 1;
                        (*i)->isPlaying = false;
                        (*i)->_frame = 0;
                        // On end Animation Call Back if needed
                    }
                }
            }
        }
    }
    
    void TextureAnimationInstance::Play(const int32 &Repeat)
    {
        repeat = Repeat;

        // Set Loop
        if (repeat<=0) isLooping = true;
        else isLooping = false;

        timeStart = Owner->timer;
        isPlaying = true;
    }
    void TextureAnimationInstance::Pause()
    {
        if (!isPaused)
        {
            isPaused = true;
            timerPauseStart = Owner->timer;
        } else {
            isPaused = false;
            timerPauseEnd = Owner->timer;
            timerPauseLength += timerPauseEnd-timerPauseStart;
        }
    }
    void TextureAnimationInstance::Stop()
    {
        isPlaying = false;
    }
}