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
		opened = false;
		file = fopen(filename.c_str(), (write ? "wb" : "rb"));
		if (file != NULL)
		{
			int32 n_blocks = 1024;
			while (n_blocks != 0)
			{
				data.resize(data.size() + n_blocks);
				n_blocks = fread(&data[data.size() - n_blocks], 1, n_blocks, file);
			}
			positionStream = 0;
			opened = true;
			return opened;
		}

		echo("Error: Couldn't Open File: " + filename);
		return opened;
	}

	void File::Read(const char* src, const uint32 size)
	{
		if (opened)
		{
			memcpy((char*)src, &data[positionStream], sizeof(unsigned char)*size);
			positionStream += size * sizeof(unsigned char);
		}
	}

	void File::Write(const char* src, const uint32 size)
	{
		if (opened)
			fwrite(src, 1, size, file);
	}

	void File::Rewind()
	{
		if (opened)
			positionStream = 0;
	}

	const uint32 File::Size() const
	{
		return data.size();
	}

	std::vector<uchar> &File::GetData()
	{
		return data;
	}

	void File::Close()
	{
		if (opened)
		{
			fclose(file);
			data.clear();
			positionStream = 0;
		}
	}
#endif
}