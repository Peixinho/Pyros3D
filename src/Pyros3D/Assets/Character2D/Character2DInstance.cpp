//============================================================================
// Name        : Character2DInstance.cpp
// Description : See the header - the one shared "apply a character" path.
//============================================================================

#include <Pyros3D/Assets/Character2D/Character2DInstance.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

namespace p3d {

	bool ApplyCharacter2D(RenderingComponent* rc, const Character2DAsset &asset,
		const std::function<std::string(const std::string&)> &resolve,
		Character2DInstance &out)
	{
		if (!rc) return false;

		// Skeleton first. The sprite quads are positioned from bone
		// transforms, so building them against a stale skeleton would place
		// every part using the PREVIOUS character's bones for one frame.
		rc->SetSkeleton(asset.bones);

		// The artwork. Replaces the component's geometry wholesale.
		rc->SetSpriteRig2D(asset.parts, resolve);

		// The clips, and the instance that plays them. A skeleton needs an
		// instance even with no clips at all: the instance is what holds the
		// pose the sprites are placed from, so without one a character with
		// artwork and bones but no animation draws every part at the origin.
		out.animation = std::make_shared<SkeletonAnimation>();
		out.animation->SetAnimations(asset.clips);
		out.instance = out.animation->CreateInstance(rc);

		// The component keeps it alive. Every caller used to have to remember
		// to hold `out.animation` themselves, and one that did not - the Lua
		// scene-load binding, which passes no asset list - left the component
		// posing through a freed instance on every frame.
		rc->KeepCharacter2DAnimation(out.animation);

		// Place the parts once now, rather than leaving them at the origin
		// until the first Update() - a character dropped into a scene should
		// look right on the frame it appears.
		rc->RefreshSpriteParts2D();
		return true;
	}

	void SetCharacter2DClips(Character2DInstance &inst, const std::vector<Animation> &clips)
	{
		if (!inst.animation) return;
		inst.animation->SetAnimations(clips);
	}

}
