//============================================================================
// Name        : TextureAnimation.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animated Textures
//============================================================================

#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <math.h>

namespace p3d {
    
    TextureAnimation::TextureAnimation() 
    {
        timer 				= 0;
    }

    TextureAnimation::~TextureAnimation()
    {

    }

    TextureAnimationInstance::TextureAnimationInstance(TextureAnimation* owner, const f32 &fps)
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
        reverse             = false;
        haveOnStartFunction = haveOnUpdateFunction = haveOnEndFunction = false;
    }

    void TextureAnimationInstance::OnStart(void (*func) (void))
    {
        haveOnStartFunction = true;
        OnStartFunction.Connect(func);
    }

    void TextureAnimationInstance::OnUpdate(void (*func) (void))
    {
        haveOnUpdateFunction = true;
        OnUpdateFunction.Connect(func);
    }

    void TextureAnimationInstance::OnEnd(void (*func) (void))
    {
        haveOnEndFunction = true;
        OnEndFunction.Connect(func);
    }
    
    bool TextureAnimationInstance::IsPlaying()
    {
       return isPlaying;
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
    TextureAnimationInstance* TextureAnimation::CreateInstance(const f32 &fps)
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
    const uint32 TextureAnimationInstance::GetFrame() const
    {
        return _frame;
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
                    if ((*i)->haveOnUpdateFunction) (*i)->OnUpdateFunction();
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
                        (*i)->_frame = ((*i)->yoyo?0:frameSize-1);
                        // On End
                        if ((*i)->haveOnEndFunction) (*i)->OnEndFunction();
                    }
                }

                // Reverse Shit
                if ((*i)->reverse)
                {
                    (*i)->_frame = (((*i)->yoyo?.5f:1.f)*frameSize - (*i)->_frame)-1;
                }
            }
        }
    }
    
    void TextureAnimationInstance::Reset()
    {
        if (reverse)
            _frame = Owner->Frames.size()-1;
        else 
            _frame = 0;
    }
    
    void TextureAnimationInstance::Play(const int32 &Repeat)
    {
        repeat = Repeat;

        // Set Loop
        if (repeat<=0) isLooping = true;
        else isLooping = false;

        timeStart = Owner->timer;
        isPlaying = true;

        if (reverse)
        {
            _frame = Owner->Frames.size()-1;
        }

        if (haveOnStartFunction) OnStartFunction();
    }

    void TextureAnimationInstance::Reverse(bool Reverse)
    {
        reverse = Reverse;

        if (reverse)
        {
            _frame = Owner->Frames.size()-1;
        }
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