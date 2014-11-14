//============================================================================
// Name        : File.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Creates a File - PC/MAC/Linux version
//============================================================================

#include <Pyros3D/Core/File/File.h>

namespace p3d {

#if !defined(ANDROID)
    bool File::Open(const std::string &filename, bool write)
    {
    	file = fopen(filename.c_str(), (write?"wb":"rb"));
		if (file!=NULL)
		{
    		int32 n_blocks = 1024;
    		while(n_blocks != 0)
			{
				data.resize(data.size() + n_blocks);
				n_blocks = fread(&data[data.size() - n_blocks], 1, n_blocks, file);
			}
			positionStream = 0;
			return true;
		}

		echo("Error: Couldn't Open File");
		return false;
    }
    
    void File::Read(const void* src, const uint32 &size)
    {
    	memcpy((char*)src, &data[positionStream], sizeof(unsigned char)*size);
		positionStream += size * sizeof(unsigned char);
	}

	void File::Write(const void* src, const uint32 &size)
	{
		fwrite(src,1,size,file);
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
		fclose(file);
		data.clear();
		positionStream = 0;
	}
#endif
}