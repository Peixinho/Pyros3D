//============================================================================
// Name        : Occluder2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A shape that blocks 2D light
//============================================================================

#ifndef OCCLUDER2D_H
#define OCCLUDER2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	namespace Occluder2DShape
	{
		enum {
			Box = 0,
			Circle = 1
		};
	}

	// Marks a shape as blocking 2D light.
	//
	// Its own component rather than a property of Physics2D, because casting a
	// shadow and being solid are different questions. A painted backdrop wall
	// should cast without needing a rigid body and a solver step; a trigger
	// volume is solid to the simulation and must not cast. Tying the two
	// together would have forced a physics body onto anything decorative that
	// wanted a shadow.
	//
	// Physics2D can still publish its own shape as an occluder - see its
	// CastsShadow() - because a body usually *is* the thing that blocks light,
	// and making every such object carry both components would be noise. This
	// is the way to say it without one.
	class PYROS3D_API Occluder2D : public IComponent {

	public:

		Occluder2D(const uint32 shape = Occluder2DShape::Box,
			const Vec2 &size = Vec2(0.5f, 0.5f));
		virtual ~Occluder2D();

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Occluder2D; }

		uint32 GetShapeType() const { return shapeType; }
		void SetShapeType(const uint32 t) { shapeType = t; }
		// Half-extents, matching Physics2D - a 1x1 box is 0.5, 0.5. For a
		// circle only .x is used, as the radius.
		const Vec2 &GetSize() const { return size; }
		void SetSize(const Vec2 &s) { size = s; }

		bool IsEnabled() const { return enabled; }
		void SetEnabled(const bool e) { enabled = e; }

		// Walks a scene and hands IRenderer every occluding shape in it, in
		// world space. A free function over the SceneGraph rather than a
		// method on Physics2DWorld, which was where this started: a scene with
		// no physics at all still has walls, and making shadows depend on a
		// solver would have been the wrong coupling.
		//
		// Gathers Occluder2D components and, separately, any Physics2D body
		// that has not opted out.
		static void PublishSceneOccluders(SceneGraph* Scene);

	private:

		uint32 shapeType;
		Vec2 size;
		bool enabled;
	};

};

#endif
