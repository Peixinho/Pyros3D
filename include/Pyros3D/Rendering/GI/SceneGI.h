//============================================================================
// Name        : SceneGI.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Scene -> ray scene -> baked DDGI volume, in one call.
//============================================================================

#ifndef SCENEGI_H
#define SCENEGI_H

#include <Pyros3D/Rendering/GI/DDGIVolume.h>
#include <Pyros3D/Other/Export.h>
#include <string>

namespace p3d {

	class SceneGraph;

	// What a scene stores about its GI, as opposed to the baked result.
	// Small enough to serialize; the atlases are not (a modest volume is
	// megabytes) and are rebuilt on load instead.
	struct PYROS3D_API SceneGISettings
	{
		bool enabled;
		uint32 counts[3];
		uint32 raysPerProbe;
		// Frames of refinement at bake time. Each pass rotates the ray
		// set, so more passes means more distinct directions sampled -
		// this is the knob that trades bake time for noise.
		uint32 passes;
		Vec3 skyColor;
		// Fraction of the scene's own size to pad the volume by, so
		// surfaces at the boundary sit inside it rather than exactly on
		// its clamped face.
		f32 padding;
		// Where gi/*.glsl lives. The compute kernels are assembled from
		// those files at runtime rather than embedded, so the GPU
		// traversal cannot silently drift from the CPU one that is
		// tested against it - the cost is that the path has to be known.
		std::string shaderRoot;

		SceneGISettings()
			: enabled(false), raysPerProbe(128), passes(6),
			  skyColor(0.f, 0.f, 0.f), padding(0.1f), shaderRoot("resources/shaders")
		{
			counts[0] = 6; counts[1] = 4; counts[2] = 6;
		}
	};

	// Extracts the scene's triangles and lights, builds a BVH, sizes a
	// volume to the scene's bounds, and traces it.
	//
	// One call because every caller wants the same five steps in the
	// same order, and getting the order wrong is silent: bake before the
	// lights exist and every probe is black, size the volume before the
	// geometry loads and it covers nothing.
	//
	// Synchronous and CPU-side. For a scene of a few thousand triangles
	// and a few hundred probes that is a second or two - acceptable at
	// load, not acceptable per frame, which is what the compute port is
	// for.
	//
	// Returns false when the scene has no renderable geometry, or when
	// settings.enabled is false.
	PYROS3D_API bool BakeSceneGI(SceneGraph *scene, const SceneGISettings &settings, DDGIVolume &outVolume);

	// One frame's worth of refresh on an already-allocated volume.
	//
	// Re-extracts the lights every call, which is the entire point: a
	// light that moves, dims or turns off changes the indirect light
	// within (probes / probeBudget) frames. The geometry is NOT
	// re-extracted - `rays` is passed in and reused, because rebuilding
	// a BVH per frame would cost far more than the tracing does and
	// static geometry is the common case.
	PYROS3D_API void UpdateSceneGI(SceneGraph *scene, const RayScene &rays,
		DDGIVolume &volume, const uint32 raysPerProbe, const uint32 frame,
		const f32 hysteresis, const uint32 probeBudget);

	// The lights a ray tracer needs, pulled out of the scene graph.
	// Exposed separately because a caller doing its own bake loop still
	// wants this translation, and it is the part that knows about
	// ILightComponent.
	PYROS3D_API void CollectRayLights(SceneGraph *scene, std::vector<RayLight> &outLights);

};

#endif /* SCENEGI_H */
