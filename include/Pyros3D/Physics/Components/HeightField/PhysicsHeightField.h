//============================================================================
// Name        : PhysicsHeightField.h
// Author      : Duarte Peixinho
// Description : A terrain tile's collision: static, built from the same
//               HeightfieldData the tile renders from, so it collides with
//               exactly the triangles it draws. Local space as Heightfield's.
//============================================================================

#ifndef PHYSICSHEIGHTFIELD_H
#define	PHYSICSHEIGHTFIELD_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <memory>

namespace p3d {

	class PYROS3D_API PhysicsHeightField : public IPhysicsComponent {
	public:

		// Always static - a height field cannot move (Box3D refuses one on
		// anything else) - so there is no mass.
		PhysicsHeightField(IPhysics* engine, const std::shared_ptr<const HeightfieldData> &data);
		virtual ~PhysicsHeightField();

		const std::shared_ptr<const HeightfieldData> &GetData() const { return data; }

	protected:

		std::shared_ptr<const HeightfieldData> data;
	};

}

#endif	/* PHYSICSHEIGHTFIELD_H */
