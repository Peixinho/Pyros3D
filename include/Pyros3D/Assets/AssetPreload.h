//============================================================================
// Name        : AssetPreload.h
// Description : Assets made ready before they are first wanted
//============================================================================

#ifndef ASSETPRELOAD_H
#define ASSETPRELOAD_H

#include <Pyros3D/Other/Export.h>
#include <cstdint>
#include <string>
#include <vector>

namespace p3d {

	// A model read the first time something uses it - the first shot fired,
	// the first crate opened - stops the game for as long as the file and its
	// textures take: a tenth of a second and more. AssetPreload reads a list
	// of them up front, under whatever the game shows while it loads, and
	// keeps each one alive (the model and something that draws it, which is
	// what reads its textures) until Clear().
	//
	// Paths are as a script would give them to Model.new / Texture: whatever
	// opens from the working directory. A file that is missing is skipped
	// with a warning.
	class PYROS3D_API AssetPreload {
	public:
		// Reads `path` now if it is not held already. Models (.p3dm) and
		// textures (.png .jpg .jpeg .tga .dds .ktx) are known; anything else
		// is skipped. Returns whether it is held afterwards.
		static bool Add(const std::string &path);
		static void Add(const std::vector<std::string> &paths);
		// Lets go of everything (a scene change, the end of play).
		static void Clear();
		static uint32_t Count();
		static std::vector<std::string> Held();
	};

}

#endif /* ASSETPRELOAD_H */
