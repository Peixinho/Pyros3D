//============================================================================
// Name        : SpriteRig2D.h
// Description : A cutout character as ONE object.
//
//               A 2D cutout character is a skeleton plus a set of sprites,
//               each following a bone.
//
//               A forearm is not a thing in a scene: it cannot be meaningfully
//               selected, moved, tagged, prefabbed or scripted on its own, so
//               modelling one as a GameObject filled the hierarchy with a
//               dozen entries per character and made clicking a character
//               select a limb.
//
//               A character is one GameObject with one RenderingComponent
//               holding N parts. Each part is a quad in the component's own
//               renderable, and the bone drives that mesh's Pivot - which the
//               renderer already composes with the owner's world matrix
//               (IRenderer: ModelMatrix = ownerWorld * rmesh->Pivot). No child
//               objects, no per-frame transform writes, no ordering hazard.
//============================================================================

#ifndef SPRITERIG2D_H
#define SPRITERIG2D_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	// One piece of a cutout character.
	struct PYROS3D_API SpritePart2D {
		// Shown in the editor and used to address the part; not required to be
		// unique by the engine, but the editor keeps it so.
		std::string name;
		// Bone this part follows. By NAME, not index: removing a bone
		// renumbers every bone after it, and an index binding would silently
		// start following a different one.
		std::string bone;
		// Texture path relative to the PROJECT ROOT, keeping the "assets/"
		// prefix ("assets/textures/torso.png") - the same form scenes and
		// materials use, so one resolver serves all three. Kept as authored so
		// it can be saved back unchanged; resolved to something loadable at
		// build time.
		std::string texture;
		// Applied in the bone's frame, so a forearm's artwork can sit away
		// from the elbow it rotates about.
		Math::Vec2 offset;
		// The part's size. Separate from the geometry, which is built at the
		// texture's aspect, so re-importing a different-sized texture does not
		// resize the character.
		Math::Vec2 scale;
		// Draw order within the character - the near arm in front of the
		// torso, the far arm behind it. A depth, not a sort index: these are
		// transparent quads and the renderer sorts them by distance.
		f32 z;
		// Where the artwork's own origin sits, normalised over its quad:
		// (0.5,0.5) centred, (0.5,0) bottom-centre. This is what makes a limb
		// rotate about its joint rather than about the middle of its texture.
		Math::Vec2 pivot;
		// Lit by the scene's 2D lights (ShaderUsage::Lighting2D: distance
		// falloff with no N.L) rather than drawn at full brightness.
		//
		// A flag rather than a material pointer. A part's material is entirely
		// derived from the fields above plus this one, so storing the built
		// material would mean the character file no longer described the
		// character - and re-authoring a sprite would have to remember to
		// rebuild it, which is the kind of thing that gets forgotten once and
		// then looks like a renderer bug.
		bool lit;

		SpritePart2D() : offset(0.f, 0.f), scale(1.f, 1.f), z(0.f), pivot(0.5f, 0.5f), lit(false) {}
	};

	// Built pieces of a rig: one geometry per part, one material per part.
	struct PYROS3D_API SpriteRig2DBuild {
		std::shared_ptr<Renderable> renderable;
		std::vector<std::shared_ptr<IMaterial> > materials;
		// Each quad's half-extents, needed to turn a normalised pivot into a
		// translation. Derived from the texture, so not part of the saved data.
		std::vector<Math::Vec2> halfExtents;
	};

	// Builds the renderable and materials for `parts`. `resolve` turns a
	// stored (project-relative) texture path into one that can be loaded;
	// pass a pass-through when the paths are already absolute.
	//
	// A part whose texture is missing still gets a quad, drawn white, so a
	// broken path is visible in the scene rather than silently absent.
	PYROS3D_API SpriteRig2DBuild BuildSpriteRig2D(
		const std::vector<SpritePart2D> &parts,
		const std::function<std::string(const std::string&)> &resolve);

}

#endif
