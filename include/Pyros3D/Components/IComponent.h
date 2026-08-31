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
			IK,
			// Screen-space UI. UIRect is the layout half (anchors, pivot,
			// offsets); UIImage/UIText are RenderingComponents, so they
			// reach the GPU through the machinery that already exists and
			// are kept out of the 3D pass by RenderLayer::UI rather than by
			// anything the world renderer has to know about.
			UICanvas,
			UIRect,
			UIImage,
			UIText,
			UIButton,
			// The rest of the widget set. All of them are UIWidgets (see
			// UIWidget.h) and reach input through the canvas the same way a
			// button does; what differs is what they do with it and which
			// child elements they drive.
			UIToggle,
			UISlider,
			UIInput,
			UIList,
			UIDropdown,
			// A menu is a tree of items rather than one control: UIMenu owns
			// the chain that is currently open, UIMenuItem is an entry in it
			// and may point at a submenu element of its own.
			UIMenu,
			UIMenuItem,
			// A dialog: shown over everything and, while it is, the only
			// thing that can be interacted with.
			UIPopup,
			// A 2D scene layer: draw order (its root's z, which is what the
			// orthographic camera a 2D scene uses sorts by anyway) and how
			// fast it scrolls relative to the camera. See Layer2D.h for why
			// it carries so little - membership, filtering and ordering all
			// already existed.
			Layer2D,
			// A Box2D rigid body. Not a Physics (Box3D) component behind
			// IPhysics: that interface is 3D throughout, and driving a 2D
			// solver through it would project every call in and back out
			// again. See Physics2D.h.
			Physics2D,
			// Blocks 2D light. Separate from Physics2D because casting a
			// shadow and being solid are different questions - see
			// Occluder2D.h.
			Occluder2D,
			// Cutout binding: a sprite takes the transform of a named bone on
			// an ancestor's 2D skeleton. See BoneBind2D.h.
			BoneBind2D
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
