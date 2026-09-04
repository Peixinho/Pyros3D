//============================================================================
// Name        : Character2DAsset.h
// Description : A 2D cutout character as a FILE (.p3d2d).
//
//               A character is a skeleton, the artwork pinned to it, and the
//               clips that move it. Those three things are only meaningful
//               together: a bone with no sprite draws nothing, a sprite with
//               no bone never moves, and a clip is written against bone names
//               that only exist in one particular skeleton. So they are one
//               asset, not three.
//
//               This mirrors what a 3D rig already is - a .p3dm carrying the
//               skeleton plus a .p3da carrying the clips - collapsed into a
//               single file because, unlike a 3D rig, all three parts are
//               authored in the same editor and there is no upstream DCC
//               package that owns any of them.
//
//               What a SCENE stores is a path to one of these plus which clip
//               to start on. Nothing about a character's construction lives in
//               a scene file: two scenes referencing the same character get
//               the same character, and fixing an elbow fixes it everywhere.
//============================================================================

#ifndef CHARACTER2DASSET_H
#define CHARACTER2DASSET_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Utils/ModelLoaders/IModelLoader.h>          // Bone
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h>  // Animation
#include <Pyros3D/Rendering/Components/Rendering/SpriteRig2D.h>           // SpritePart2D
#include <string>
#include <vector>

namespace p3d {

	// The contents of one .p3d2d.
	struct PYROS3D_API Character2DAsset {
		// Skeleton in id order: bones[i].self == i, and every bone's parent is
		// a lower index. Both are invariants the runtime relies on when it
		// composes the pose through the parent chain, and LoadCharacter2D
		// enforces them rather than trusting the file.
		std::vector<Bone> bones;

		// The artwork. Each part names the bone it follows, so removing a bone
		// orphans a part visibly instead of silently repointing it at whatever
		// bone inherited its index.
		std::vector<SpritePart2D> parts;

		// Authored clips, in the engine's own Animation form - the same
		// structure a .p3da holds and the same one SkeletonAnimation plays.
		// Times are in seconds with TicksPerSecond 1, matching what
		// AnimationLoader::Load produces (see its Save() round-trip note).
		std::vector<Animation> clips;

		// Clip the character starts on when a scene runs it and nothing says
		// otherwise. Empty means "stand in the bind pose".
		std::string defaultClip;
		bool defaultClipLoops = true;

		// Half-height in world units the character is authored against, used
		// by the editor to frame its viewport and to size a newly placed
		// instance. Purely a hint; zero means "work it out from the bones".
		f32 authoredHalfHeight = 0.f;

		int FindBone(const std::string &name) const;
		int FindClip(const std::string &name) const;
		// Names not already taken by a bone / part / clip, so the editor's
		// "Add" buttons cannot produce a duplicate - which for bones and clips
		// would make a name ambiguous, and names are what everything here
		// refers to each other by.
		std::string UniqueBoneName(const std::string &wanted) const;
		std::string UniquePartName(const std::string &wanted) const;
		std::string UniqueClipName(const std::string &wanted) const;
	};

	// The same contents as text, without touching the filesystem. Exposed
	// because the editor's undo stack snapshots a whole character per edit,
	// and routing that through the FILE serializer is what guarantees an undo
	// can never restore something the format cannot express.
	PYROS3D_API std::string Character2DToString(const Character2DAsset &asset);
	PYROS3D_API bool Character2DFromString(const std::string &text,
		Character2DAsset &out, std::string *errorOut = NULL);

	// Reads a .p3d2d. Returns false and fills `errorOut` on a missing file,
	// malformed JSON, or a skeleton whose parent references do not form a
	// tree - the last of which would otherwise crash the pose composer rather
	// than report anything.
	PYROS3D_API bool LoadCharacter2D(const std::string &filename,
		Character2DAsset &out, std::string *errorOut = NULL);

	// Writes one. Texture paths are stored exactly as authored - relative to
	// the project root, "assets/" prefix and all (see SpritePart2D::texture).
	// Resolving them is the caller's job, because only the caller knows where
	// the project root is.
	PYROS3D_API bool SaveCharacter2D(const std::string &filename,
		const Character2DAsset &asset, std::string *errorOut = NULL);

}

#endif
