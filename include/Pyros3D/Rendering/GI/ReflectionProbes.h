//============================================================================
// Name        : ReflectionProbes.h
// Description : What a place looks like from inside it, for what shines there.
//============================================================================

#ifndef REFLECTIONPROBES_H
#define REFLECTIONPROBES_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	class SceneGraph;

	// A reflection probe: the scene seen from this object's position, in every direction, kept
	// blurred for each roughness - what a metal or a polished floor inside its box reflects, where
	// the sky's reflection (EnvironmentMap) would show the sky through the ceiling. The box is the
	// room, world-aligned and centred on the object: reflections are looked up where the reflected
	// ray leaves it (box projection), so a wall is reflected where the wall is and not at infinity,
	// and a surface takes the probe in full inside the box, fading out over `blend` metres at its
	// faces. Several probes may overlap; a pixel takes the smallest box it is inside.
	//
	// Captured, not live: when the scene has loaded and whenever Capture() is asked for (a door
	// opened, a light switched) - the renderer sees it, a probe at a time, into its probe atlas
	// (SceneRenderer::SetReflectionProbes).
	class PYROS3D_API ReflectionProbe : public IComponent {
	public:
		ReflectionProbe();
		virtual ~ReflectionProbe();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);
		virtual uint32 GetComponentType() const { return ComponentType::ReflectionProbe; }

		// Half the box's size on each axis, metres.
		void SetExtents(const Vec3 &halfExtents);
		const Vec3 &GetExtents() const { return extents; }
		void SetBlend(const f32 metres) { blend = metres > 0.f ? metres : 0.f; }
		f32 GetBlend() const { return blend; }

		// See it again
		void Capture() { captureRevision++; }
		// (the renderer sees it again whenever this changes - Capture() asked for it)
		uint32 GetCaptureRevision() const { return captureRevision; }
		SceneGraph* GetScene() const { return scene; }

	private:
		friend class ReflectionProbes;
		SceneGraph* scene;
		Vec3 extents;
		f32 blend;
		uint32 captureRevision = 1;
	};

	// Every probe of the process
	class PYROS3D_API ReflectionProbes {
	public:
		// Every probe of this scene seen again (NULL: of every scene).
		static void CaptureAll(SceneGraph* scene);
		// The probes registered in this scene (NULL: in every scene)
		static uint32 Count(SceneGraph* scene = NULL);
		// Every probe registered in a scene (the renderer sees them, into an atlas of its own)
		static const std::vector<ReflectionProbe*>& All();

	private:
		friend class ReflectionProbe;
		static void Add(ReflectionProbe* p);
		static void Remove(ReflectionProbe* p);
	};
}

#endif /* REFLECTIONPROBES_H */
