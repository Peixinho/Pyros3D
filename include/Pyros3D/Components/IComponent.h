//============================================================================
// Name        : IComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component Interface
//============================================================================

#ifndef ICOMPONENT_H
#define	ICOMPONENT_H

#include <vector>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	// Circular Dependency
	class GameObject;
	class SceneGraph;

	// Real component-type discriminator - IComponent otherwise has zero
	// RTTI, so generic code (the scene serializer) can't tell what
	// concrete type a GameObject::GetComponents() entry is without this.
	// Deliberately a non-pure virtual defaulting to Unknown (not "= 0"),
	// so no existing or future third-party IComponent subclass is forced
	// to implement it - an unrecognized component type just can't be
	// generically serialized, which is the correct, honest behavior.
	namespace ComponentType {
		enum {
			Unknown = 0,
			RenderingComponent,
			RenderingInstancedComponent,
			ParticleSystem,
			DirectionalLight,
			PointLight,
			SpotLight,
			Physics,
			Vehicle,
			LuaComponent,
			AudioSource,
			IK
		};
	}

	class IComponent {

		friend class GameObject;

	public:

		IComponent() { Owner = NULL; Registered = false; active = true; }
		virtual ~IComponent() {}

		virtual void Register(SceneGraph* Scene) = 0;
		virtual void Init() = 0;
		virtual void Update(const f64 time = 0) = 0;
		virtual void Destroy() = 0;
		virtual void Unregister(SceneGraph* Scene) = 0;

		GameObject* GetOwner() { return Owner; }

		virtual uint32 GetComponentType() const { return ComponentType::Unknown; }

		bool IsActive() { return active; }
		void Disable() { active = false; }
		void Enable() { active = true; }

		virtual const f32 &GetBoundingSphereRadius() const { return BoundingSphereRadius; }
		virtual const Vec3 &GetBoundingSphereCenter() const { return BoundingSphereCenter; }
		virtual const Vec3 &GetBoundingMinValue() const { return minBounds; }
		virtual const Vec3 &GetBoundingMaxValue() const { return maxBounds; }

	protected:

		GameObject* Owner;

		bool Registered;

		bool active;

		// Bounds of the Component
		f32 BoundingSphereRadius;
		Vec3 BoundingSphereCenter;
		Vec3 maxBounds, minBounds;


	};

};

#endif /* ICOMPONENT_H */
