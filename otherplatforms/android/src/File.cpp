//============================================================================
// Name        : File.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a File - Android Version using SDL
//============================================================================

#include <Pyros3D/Core/File/File.h>
#include <SDL.h>

namespace p3d {

    #if defined(ANDROID)
    void File::Open(const std::string &filename, bool write)
    {
        // Using SDL_Rwops
        SDL_RWops *file;
        file = SDL_RWFromFile(filename.c_str(), (write?"wb":"rb"));
        int n_blocks = 1024;
        while(n_blocks != 0)
        {
            data.resize(data.size() + n_blocks);
            n_blocks = SDL_RWread(file, &data[data.size() - n_blocks], 1, n_blocks);
        }
        positionStream = 0;
        SDL_RWclose(file);
    }
    
    void File::Read(const void* src, const uint32 &size)
    {
    	memcpy((char*)src, &data[positionStream], sizeof(unsigned char)*size);
		positionStream += size * sizeof(unsigned char);
	}

	void File::Write(const void* src, const uint32 &size)
	{
        // Not Implemented Yet
		//fwrite(src,1,size,file);
	}

    void File::Rewind()
    {
        positionStream = 0;
    }

    const uint32 File::Size() const
    {
        return data.size();
    }

    const std::vector<uchar> &File::GetData() const
    {
        return data;
    }

	void File::Close()
	{
		data.clear();
		positionStream = 0;
	}
    #endif
}