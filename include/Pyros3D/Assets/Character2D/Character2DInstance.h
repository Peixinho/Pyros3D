//============================================================================
// Name        : Character2DInstance.h
// Description : Turning a .p3d2d into something on screen.
//
//               One function, used by BOTH the scene loader and the editor's
//               character viewport. That is the whole point of it existing:
//               the editor shows you what you are authoring, and the only way
//               to be sure it shows the truth is for it to build the character
//               through the same code the game does. Two implementations of
//               "apply a character" is two sets of rules about pivots, bone
//               order and draw order, and they drift.
//
//               Lives outside RenderingComponent because attaching clips needs
//               SkeletonAnimation, which includes RenderingComponent.h - the
//               component cannot depend on it back.
//============================================================================

#ifndef CHARACTER2DINSTANCE_H
#define CHARACTER2DINSTANCE_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Assets/Character2D/Character2DAsset.h>
#include <functional>
#include <memory>
#include <string>

namespace p3d {

	class RenderingComponent;
	class SkeletonAnimation;
	class SkeletonAnimationInstance;

	// What applying a character leaves behind. The SkeletonAnimation owns the
	// clips AND the instance (its destructor deletes them), so the caller has
	// to keep the shared_ptr alive for as long as the component is rendered -
	// dropping it leaves `instance` dangling and the component posing through
	// freed memory.
	struct PYROS3D_API Character2DInstance {
		std::shared_ptr<SkeletonAnimation> animation;
		// Owned by `animation`; never delete.
		SkeletonAnimationInstance* instance = NULL;
	};

	// Builds `asset` onto `rc`: skeleton, sprite quads, and the clips attached
	// to a fresh SkeletonAnimation instance. Replaces whatever the component
	// had, so re-applying after an edit is how the editor refreshes.
	//
	// `resolve` turns a stored (project-relative) texture path into one that
	// can be loaded.
	//
	// Returns false only when `rc` is null; a character with no bones, no
	// sprites or no clips is a perfectly good half-authored one and builds
	// fine.
	PYROS3D_API bool ApplyCharacter2D(RenderingComponent* rc,
		const Character2DAsset &asset,
		const std::function<std::string(const std::string&)> &resolve,
		Character2DInstance &out);

	// Re-attaches just the clips, leaving the skeleton and the artwork alone.
	// What the editor calls after a keyframe edit: rebuilding the quads there
	// would re-upload every texture on every keystroke.
	PYROS3D_API void SetCharacter2DClips(Character2DInstance &inst,
		const std::vector<Animation> &clips);

}

#endif
