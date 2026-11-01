//============================================================================
// Name        : ImpostorBaker.h
// Author      : Duarte Peixinho
// Description : Octahedral impostors made by the engine: a model - or a tree, anything a GameObject draws - rendered
//               from a grid of directions over its upper half into two atlases (its colour; its normal, with its own
//               occlusion in alpha), what ShaderUsage::Impostor draws a quad from. The editor's
//               bake_octahedral_impostor writes them into a project; ImpostorBank makes them by itself - a model's
//               last level of detail, with no one baking anything - cached on disk so a second run only loads them.
//============================================================================

#ifndef IMPOSTORBAKER_H
#define IMPOSTORBAKER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class SceneRenderer;
	class GameObject;
	class Renderable;
	class GenericShaderMaterial;
	class Texture;
	class IMaterial;

	// What a bake made: N x N views of `size` pixels each (the atlases N * size square, RGBA8 - rows from the top), and
	// the thing's radius and the height of its centre in its own units (an impostor's quad and material take them)
	struct OctahedralImpostorBake
	{
		uint32 frames = 0, size = 0;
		f32 radius = 0.f, centre = 0.f;
		bool fullSphere = false;     // (every way round - the octahedron; else the upper half, a tree's)
		std::vector<uchar> albedo, normals;
	};

	// `subject` baked by `renderer` (a SceneRenderer of its own: the bake sets its tone curve, exposure and debug views)
	// on `device`. hostFrame: drawn inside a frame already begun - the editor's, or the renderer's own (ImpostorBank)
	PYROS3D_API bool BakeOctahedralImpostor(gfx::Device* device, SceneRenderer &renderer, const std::shared_ptr<GameObject> &subject,
		const uint32 frames, const uint32 size, const bool hostFrame, OctahedralImpostorBake &out, std::string &err, const bool fullSphere = false,
		const uint32 firstView = 0, const uint32 viewCount = ~0u,
		// (a bake whose reads are not waited for: 0 its views drawn into *keepTargets - read back by the caller - 1 those
		// reads taken in, *readBackIn, three a view in order; -1 both, waited for)
		const int onlyPhase = -1, std::vector<gfx::Texture>* keepTargets = nullptr, std::vector<std::vector<uint8_t> >* readBackIn = nullptr);

	// Automatic impostors, by model file: asked for by a model's levels of detail (RenderingComponent's automatic ones),
	// baked by a renderer between frames - one at a time - or loaded from the disk cache (PYROS_IMPOSTOR_CACHE, else
	// the system's temporary folder; keyed by the file's path, size and time). PYROS_AUTO_IMPOSTOR=0 makes none.
	class PYROS3D_API ImpostorBank
	{
	public:
		struct Entry
		{
			std::shared_ptr<Renderable> quad;
			std::shared_ptr<GenericShaderMaterial> material;
			std::shared_ptr<Texture> albedo, normal;
			f32 radius = 0.f, centre = 0.f;
		};
		// The model's impostor, as `material` colours it (a GenericShaderMaterial's colour, maps and shine: one impostor for
		// every object of the same model and the same values; null - the model's own materials), when made; else it is
		// asked for (made a frame or a few on) and this is null. Null for good when it could not be made.
		static const Entry* Request(const std::string &modelPath, const std::shared_ptr<IMaterial> &material = nullptr);
		static bool Enabled();
		static void SetEnabled(const bool on);
		// (a renderer, at the start of its frame: what was asked for baked - one model a call - with a renderer of its own)
		static void BakePending(gfx::Device* device, std::unique_ptr<SceneRenderer> &renderer);
		static bool AnyPending();
	};
};

#endif /* IMPOSTORBAKER_H */
