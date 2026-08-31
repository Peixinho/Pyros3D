//============================================================================
// Name        : BoneBind2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Makes a sprite follow a bone of its parent's 2D skeleton
//============================================================================

#ifndef BONEBIND2D_H
#define BONEBIND2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>

namespace p3d {

	// Cutout binding: this GameObject takes the transform of a named bone.
	//
	// The bone belongs to the skeleton on an ANCESTOR of this object, and bone
	// transforms are model space relative to whichever object carries that
	// skeleton - so writing the bone's global transform into this object's
	// LOCAL transform is exactly right, and the existing parent chain does the
	// rest. That is the whole reason cutout needs no skinning path: a sprite
	// parented under the rig simply inherits a bone's matrix.
	//
	// Bound by NAME rather than by bone index on purpose. Indices are
	// positional - removing a bone renumbers every bone after it (see
	// SceneEditor::OpRemoveBone2D) - so an index binding would silently start
	// following a different bone. A name either resolves or does not.
	//
	// `weights` is unused today and is here so weighted deformation can be
	// added without changing the authored data or the file format: a cutout
	// binding is the single-influence case of the same thing.
	class PYROS3D_API BoneBind2D : public IComponent {

	public:

		BoneBind2D(const std::string &bone = std::string());
		virtual ~BoneBind2D();

		const std::string &GetBone() const { return boneName; }
		void SetBone(const std::string &name) { boneName = name; }

		// Applied on top of the bone's transform, so a sprite can sit off its
		// joint - a forearm's artwork is not centred on the elbow.
		const Vec2 &GetOffset() const { return offset; }
		void SetOffset(const Vec2 &o) { offset = o; }

		// The sprite's own scale, kept here because Update() overwrites the
		// object's whole transform with the bone's - so a part sized in the
		// editor lost that size the moment it was bound, and every limb
		// snapped to its raw texture dimensions. Captured at bind time, so
		// binding preserves how the sprite was already sized.
		const Vec2 &GetScale() const { return scale; }
		void SetScale(const Vec2 &s) { scale = s; }

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::BoneBind2D; }

	private:

		std::string boneName;
		Vec2 offset;
		Vec2 scale;
	};

};

#endif
