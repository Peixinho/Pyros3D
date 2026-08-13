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
		data.clear();
		positionStream = 0;
		file = fopen(filename.c_str(), (write ? "wb" : "rb"));
		if (file == NULL)
		{
			echo("Error: Couldn't Open File: " + filename);
			return false;
		}

		if (write)
		{
			// Writers only fwrite(); keep an empty in-memory buffer.
			opened = true;
			return true;
		}

		// Old path resized +1024 and fread in a loop. For island.p3dm (~16MB)
		// that is ~16k realloc/copies (O(n²) memcpy) and the editor appeared
		// hung until the process was killed. Size the buffer once, then read.
		if (fseek(file, 0, SEEK_END) != 0)
		{
			fclose(file);
			file = NULL;
			echo("Error: Couldn't Open File: " + filename);
			return false;
		}
		const long sz = ftell(file);
		if (sz < 0)
		{
			fclose(file);
			file = NULL;
			echo("Error: Couldn't Open File: " + filename);
			return false;
		}
		if (fseek(file, 0, SEEK_SET) != 0)
		{
			fclose(file);
			file = NULL;
			echo("Error: Couldn't Open File: " + filename);
			return false;
		}

		data.resize((size_t)sz);
		size_t got = 0;
		while (got < (size_t)sz)
		{
			const size_t n = fread(&data[got], 1, (size_t)sz - got, file);
			if (n == 0)
				break;
			got += n;
		}
		data.resize(got);
		positionStream = 0;
		opened = true;
		return true;
	}

	void File::Read(const char* src, const uint32 size)
	{
		if (!opened || size == 0)
			return;
		if (positionStream + (uint32)size > (uint32)data.size())
		{
			echo("ERROR: File::Read past end of buffer");
			return;
		}
		memcpy((char*)src, &data[positionStream], size);
		positionStream += size;
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
		return (uint32)data.size();
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
			file = NULL;
			data.clear();
			positionStream = 0;
			opened = false;
		}
	}
#endif
}
