//============================================================================
// Name        : File.cpp
// Description : Android file I/O via SDL_RWops (APK assets / internal storage).
//============================================================================

#include <Pyros3D/Core/File/File.h>
#include <SDL.h>

namespace p3d {

#if defined(ANDROID)

	bool File::Open(const std::string &filename, bool write)
	{
		opened = false;
		file = NULL;
		data.clear();
		positionStream = 0;

		SDL_RWops *rw = SDL_RWFromFile(filename.c_str(), write ? "wb" : "rb");
		if (rw == NULL)
		{
			echo(std::string("Error: Couldn't Open File: ") + filename + " (" + SDL_GetError() + ")");
			return false;
		}

		Sint64 sz = SDL_RWsize(rw);
		if (sz > 0)
		{
			data.resize((size_t)sz);
			const size_t got = (size_t)SDL_RWread(rw, data.data(), 1, (size_t)sz);
			data.resize(got);
		}
		else
		{
			// Unknown size — read in chunks (same idea as the desktop path).
			int n_blocks = 1024;
			while (n_blocks != 0)
			{
				data.resize(data.size() + n_blocks);
				n_blocks = (int)SDL_RWread(rw, &data[data.size() - n_blocks], 1, n_blocks);
				data.resize(data.size() - (1024 - n_blocks));
			}
		}

		SDL_RWclose(rw);
		opened = true;
		return true;
	}

	void File::Read(const char *src, const uint32 size)
	{
		if (!opened) return;
		memcpy((char *)src, &data[positionStream], sizeof(unsigned char) * size);
		positionStream += size * sizeof(unsigned char);
	}

	void File::Write(const char *src, const uint32 size)
	{
		(void)src;
		(void)size;
		// APK assets are read-only; writing to internal storage can be added later.
	}

	void File::Rewind()
	{
		if (opened) positionStream = 0;
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
		data.clear();
		positionStream = 0;
		opened = false;
	}

#endif /* ANDROID */

} // namespace p3d
