//============================================================================
// Name        : ISound.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sound Interface
//============================================================================

#include <Pyros3D/Assets/Sounds/ISound.h>
#include <Pyros3D/Core/File/File.h>

namespace p3d {
  
    ISound::ISound(const uint32 type) 
    {
        _isPlaying = false;
        _isPaused = false;
        _type = type;
        _volume = 100;
    }

    ISound::~ISound()
    {
        #if defined(_SDL2) || defined(_SDL)
            switch(_type)
            {
                case SoundType::Music:
                    Mix_HaltMusic();
                    Mix_FreeMusic(_Music);
                break;
                case SoundType::Sound:
                    delete _Sound;
                break;
            }
        #endif
    }

    bool ISound::LoadFromFile(const std::string &filename)
    {
        switch(_type)
        {
            case SoundType::Music:
            {

                #if defined(_SDL2) || defined(_SDL)

                    _Music = Mix_LoadMUS(filename.c_str());
					return true;
                #else

                    File* file = new File();
                    file->Open(filename);
                    _Music.openFromMemory(&file->GetData()[0],file->Size());
                    delete file;
					return true;
                #endif 
            }
            break;
            case SoundType::Sound:
            default:
            {
                #if defined(_SDL2) || defined(_SDL)

                    _Sound = Mix_LoadWAV(filename.c_str());
					return true;
                #else
                    
                    File* file = new File();
                    file->Open(filename);
                    _Buffer.loadFromMemory(&file->GetData()[0],file->Size());
                    _Sound.setBuffer(_Buffer);
					return true;
                    delete file;

                #endif
            }
            break;
        };
		return false;
    }

    void ISound::Play(bool loop)
    {
        _isPlaying = true;

        _loop = loop;

        switch(_type)
        {
            case SoundType::Music:
                
                #if defined(_SDL2) || defined(_SDL)

                    Mix_PlayMusic(_Music, (_loop?-1:0));
                    Mix_VolumeMusic(_volume*100/128);

                #else

                    _Music.setLoop(_loop);
                    _Music.setVolume(_volume);
                    _Music.play();

                #endif

            break;
            case SoundType::Sound:
            default:

                #if defined(_SDL2) || defined(_SDL)

                    channel = Mix_PlayChannel(-1, _Sound, (_loop?-1:0));
                    Mix_Volume(channel,_volume*100/128);

                #else

                    _Sound.setLoop(_loop);
                    _Sound.setVolume(_volume);
                    _Sound.play();

                #endif
            break;
        };
    }

    void ISound::SetVolume(const uint32 vol)
    {
        _volume = (vol>=100?100:vol);

        #if defined(_SDL2) || defined(_SDL)

            switch(_type)
            {
                case SoundType::Music:
                {
                    Mix_VolumeMusic(vol*100/128);
                }
                break;
                case SoundType::Sound:
                default:
                {
                    if (channel>-1)
                        Mix_Volume(channel,vol*100/128);
                }
                break;
            };

        #else

            switch(_type)
            {
                case SoundType::Music:
                    _Music.setVolume(_volume);
                break;
                case SoundType::Sound:
                default:
                    _Sound.setVolume(_volume);
                break;
            };

        #endif
    }

    void ISound::Pause()
    {
        if (!_isPaused) 
        {
            _isPaused = true;
            switch(_type)
            {
                case SoundType::Music:
                        
                    #if defined(_SDL2) || defined(_SDL)
                        Mix_PausedMusic();
                    #else
                        _Music.pause();
                    #endif

                break;
                case SoundType::Sound:
                default:
                        
                    #if defined(_SDL2) || defined(_SDL)
                        Mix_Pause(channel);
                    #else
                        _Sound.pause();
                    #endif

                break;
            };
        } else {
            _isPaused = false;
            switch(_type)
            {
                case SoundType::Music:
                    
                    #if defined(_SDL2) || defined(_SDL)
                        Mix_ResumeMusic();
                    #else
                        _Music.play();
                    #endif

                break;
                case SoundType::Sound:
                    
                    #if defined(_SDL2) || defined(_SDL)
                        Mix_Resume(channel);
                    #else
                        _Sound.play();
                    #endif
                break;
            };
        }
    }

    void ISound::Stop()
    {
        _isPlaying = false;

        switch(_type)
        {
            case SoundType::Music:
                
                #if defined(_SDL2) || defined(_SDL)
                    Mix_HaltMusic();
                #else
                    _Music.stop();
                #endif
                    
            break;
            case SoundType::Sound:
                
                #if defined(_SDL2) || defined(_SDL)
                    Mix_HaltChannel(channel);
                #else
                    _Sound.stop();
                #endif

            break;
        }
    }

    bool ISound::isPlaying()
    {
        return _isPlaying;
    }

    bool ISound::isPaused()
    {
        return _isPaused;
    }

};