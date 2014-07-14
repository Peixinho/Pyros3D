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
  
    ISound::ISound(const uint32 &type) 
    {
        _isPlaying = false;
        _isPaused = false;

        _type = type;
    }

    ISound::~ISound()
    {
        
    }

    bool ISound::LoadFromFile(const std::string &filename)
    {
        File* file = new File();
        file->Open(filename);
        switch(_type)
        {
            case SoundType::Music:
                _Music.openFromMemory(&file->GetData()[0],file->Size());
            break;
            case SoundType::Sound:
            default:
                _Buffer.loadFromMemory(&file->GetData()[0],file->Size());
                _Sound.setBuffer(_Buffer);
            break;
        };

        delete file;
    }

    void ISound::Play()
    {
        _isPlaying = true;

        switch(_type)
        {
            case SoundType::Music:
                _Music.play();
            break;
            case SoundType::Sound:
            default:
                _Sound.play();
            break;
        };
    }

    void ISound::Pause()
    {
        if (!_isPaused) 
        {
            _isPaused = true;
            switch(_type)
            {
                case SoundType::Music:
                    _Music.pause();
                break;
                case SoundType::Sound:
                default:
                    _Sound.pause();
                break;
            };
        } else {
            _isPaused = false;
            switch(_type)
            {
                case SoundType::Music:
                    _Music.play();
                break;
                case SoundType::Sound:
                    _Sound.play();
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
                _Music.stop();
            break;
            case SoundType::Sound:
                _Sound.stop();
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

    void ISound::Loop(bool loop)
    {
        _loop = loop;

        switch(_type)
        {
            case SoundType::Music:
                _Music.setLoop(_loop);
            break;
            case SoundType::Sound:
                _Sound.setLoop(_loop);
            break;
        }
    }

};
