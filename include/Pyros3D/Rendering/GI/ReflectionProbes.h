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
	class Texture;

	// A reflection probe: the scene seen from this object's position, in every direction, kept
	// blurred for each roughness - what a metal or a polished floor inside its box reflects, where
	// the sky's reflection (EnvironmentMap) would show the sky through the ceiling. The box is the
	// room, world-aligned and centred on the object: reflections are looked up where the reflected
	// ray leaves it (box projection), so a wall is reflected where the wall is and not at infinity,
	// and a surface takes the probe in full inside the box, fading out over `blend` metres at its
	// faces. Several probes may overlap; a pixel takes the smallest box it is inside.
	//
	// Captured, not live: when the scene has loaded and whenever Capture() is asked for (a door
	// opened, a light switched) - ReflectionProbes::Update does the work, a probe a frame.
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

		// See it again (next ReflectionProbes::Update).
		void Capture() { wanted = true; bounces = 0; }
		bool IsCaptured() const { return slot >= 0 && ready; }

	private:
		friend class ReflectionProbes;
		SceneGraph* scene;
		Vec3 extents;
		f32 blend;
		bool wanted, ready;
		uint32 bounces;          // times seen since asked: twice, the second with the first in the atlas
		int32 slot;              // its block in the atlas
		Vec3 capturedAt;         // where it was seen from
	};

	// Every probe of the process, as the environment map is: their pictures in one texture (one
	// sampler however many probes there are - a PBR stage is near the 16 it may have) and their
	// boxes in a uniform block.
	class PYROS3D_API ReflectionProbes {
	public:
		static const uint32 kMaxProbes = 16;
		static const uint32 kBlockWidth = 256;        // a probe's sharpest level: 256x128
		static const uint32 kFaceSize = 128;          // what each of its six views is drawn at
		static const uint32 kColumns = 4;

		// Inside a frame, before the scene is drawn, once a frame: the next probe waiting is seen
		// (six views, read back), its filtering is handed to another thread, and what has finished
		// filtering is put in the atlas. Returns true while there is more to do.
		// followMoves: a probe whose object has moved since it was seen is seen again (the editor,
		// where probes are placed by dragging them).
		static bool Update(SceneGraph* scene, const bool followMoves = false);
		// Every probe of this scene seen again.
		static void CaptureAll(SceneGraph* scene);

		// Never NULL: 1x1 black until a probe has been captured.
		static Texture* GetAtlas();
		// The uniform block (PyrosShader.glsl's ReflectionProbeBlock): vec4 count, then per probe
		// three vec4s - box min (w = blend), box max (w = its block), where it was seen from.
		static const uint32 kBlockVec4s = 1 + 3 * kMaxProbes;
		static void FillBlock(Vec4 out[kBlockVec4s]);
		static uint32 Revision();          // changes whenever the block or the atlas does
		static uint32 Count();             // captured ones
		static uint32 Pending();           // waiting to be seen or still filtering

	private:
		friend class ReflectionProbe;
		static void Add(ReflectionProbe* p);
		static void Remove(ReflectionProbe* p);
	};
}

#endif /* REFLECTIONPROBES_H */
