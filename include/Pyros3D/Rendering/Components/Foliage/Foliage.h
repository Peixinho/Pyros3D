//============================================================================
// Name        : Foliage.h
// Author      : Duarte Peixinho
// Description : Grass, bushes, rocks and trees scattered over a terrain tile.
//
//               A tile carries layers; each layer is one mesh and material
//               placed at a density, filtered by slope, height and an
//               optional density map. Instances are generated from the
//               tile's own heights, deterministically (the same tile always
//               grows the same field), on any thread - a streamed cell does
//               it on the loader thread alongside the tile's mesh.
//
//               Each layer is split into square blocks, one instanced draw
//               each. Within a block instances are in random order, so any
//               PREFIX of the block is an even, thinner copy of it: distance
//               density is just an instance count, nothing is rebuilt.
//               FoliageComponent sets that count every frame from the
//               viewer's distance (full up to fullDistance, thinning to
//               nothing at fadeDistance), and drops shadows past
//               shadowDistance. A layer may name a far mesh (a card
//               standing in for a clump) that takes over past lodDistance.
//
//               The viewer is process-wide - FoliageComponent::SetViewer -
//               because the scene has no idea which camera is the player's.
//============================================================================

#ifndef FOLIAGE_H
#define FOLIAGE_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class RenderingInstancedComponent;
	class Renderable;
	class IMaterial;
	class GameObject;
	class PaintableImage;

	struct PYROS3D_API FoliageLayerSpec
	{
		std::string name;
		f32 density = 0.2f;			// instances per square metre, before filtering
		f32 blockSize = 32.f;		// metres per side of one draw
		f32 minScale = 0.8f, maxScale = 1.2f;
		Vec4 tintLow = Vec4(1.f, 1.f, 1.f, 1.f), tintHigh = Vec4(1.f, 1.f, 1.f, 1.f);
		f32 maxSlopeDegrees = 35.f;	// steeper ground grows nothing
		f32 minHeight = -1e9f, maxHeight = 1e9f;
		f32 alignToGround = 0.f;	// 0 upright, 1 along the ground's normal
		f32 sink = 0.05f;			// metres pushed into the ground
		uint32 seed = 1;
		f32 fullDistance = 60.f, fadeDistance = 120.f, shadowDistance = 60.f;
		// Thinning: from thinFrom to thinTo metres the plants drawn fall to
		// thinDensity of them (1: none taken), and those left grow, so that
		// the ground is covered as it was. Far off a plant is a few pixels,
		// and what a field costs to draw is how many plants it is - not how
		// much of the picture they fill.
		f32 thinFrom = 0.f, thinTo = 0.f, thinDensity = 1.f;
		f32 lodDistance = 0.f;		// 0: no far mesh
		bool castShadows = true;
		// Optional greyscale PNG over the tile (0..1 = keep probability).
		std::string densityMap;
	};

	// One layer's instances, in blocks. Matrices are local to their block's
	// origin, which is itself local to the tile.
	struct PYROS3D_API FoliageBlock
	{
		Vec3 origin;
		f32 radius = 0.f;			// bounding sphere, centred on origin + (blockSize/2, *, blockSize/2)
		f32 centreY = 0.f;
		std::vector<Matrix> transforms;
		std::vector<Vec4> tints;
	};

	struct PYROS3D_API PreparedFoliageLayer
	{
		std::vector<FoliageBlock> blocks;

		// Any thread. densityMapPath is the resolved densityMap, or empty.
		static void Generate(const HeightfieldData &ground, const FoliageLayerSpec &spec,
			const std::string &densityMapPath, PreparedFoliageLayer &out);
		// The same from a density map already in memory (being painted);
		// NULL for none. Channel 0 is the keep probability.
		static void Generate(const HeightfieldData &ground, const FoliageLayerSpec &spec,
			const PaintableImage* densityMap, PreparedFoliageLayer &out);
	};

	class PYROS3D_API FoliageComponent : public IComponent
	{
	public:
		struct Layer
		{
			FoliageLayerSpec spec;
			// What every block draws, kept for saving the layer back.
			std::shared_ptr<Renderable> mesh, lodMesh;
			std::shared_ptr<IMaterial> material, lodMaterial;
			std::vector<std::shared_ptr<RenderingInstancedComponent> > blocks;
			std::vector<std::shared_ptr<GameObject> > blockObjects;	// parallel to blocks
			std::vector<Vec3> centres;	// tile-local, parallel to blocks
			std::vector<uint32> counts;	// full instance count per block
			// Loaded on first use by painting; what Regrow() reads when set.
			std::shared_ptr<PaintableImage> densityMap;
			// spec.densityMap resolved: what Regrow() reads when the map is
			// not in memory (set by whoever builds the layer).
			std::string densityMapPath;
		};

		FoliageComponent() {}
		virtual ~FoliageComponent() {}

		// The block components live on child GameObjects of the owner, made
		// by whoever builds the layer (SceneSerializer); this only drives
		// them.
		void AddLayer(const Layer &layer) { layers.push_back(layer); }
		const std::vector<Layer> &GetLayers() const { return layers; }
		std::vector<Layer> &GetLayers() { return layers; }

		// Creates one transient child GameObject per block of `prepared`
		// under `owner`, drawing the layer's mesh - what a load does, and
		// what Regrow() does again. Main thread.
		static void BuildBlocks(GameObject* owner, Layer &layer, PreparedFoliageLayer &prepared,
			std::vector<std::shared_ptr<GameObject> >* created = NULL);

		// Regenerates every layer from `ground` (after sculpting) or one
		// layer (after painting its density map, -1 = all). The old blocks
		// leave the scene at once and are destroyed a few Updates later,
		// once no frame in flight can still be drawing them.
		void Regrow(const HeightfieldData &ground, const int32 layerIndex = -1);

		// Where the viewer is, in world space, this frame. Every foliage
		// component thins and fades against it.
		static void SetViewer(const Vec3 &worldPosition);
		static const Vec3 &GetViewer();

		virtual void Register(SceneGraph* Scene) { Registered = true; }
		virtual void Unregister(SceneGraph* Scene) { Registered = false; }
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}

		// Fraction of a block drawn at distance d, for a spec.
		static f32 DensityAt(const FoliageLayerSpec &spec, const f32 d);
		// The share of plants a layer's thinning leaves at a distance.
		static f32 ThinningAt(const FoliageLayerSpec &spec, const f32 d);
		// Thinning for every layer that asks for it (on unless said): off, a
		// field is drawn plant for plant, as before there was any.
		// A process that draws nothing (a dedicated server) grows none: the
		// plants are only ever looked at.
		static void SetHeadless(const bool on);
		static void SetThinning(const bool on);
		static bool GetThinning();

	private:
		std::vector<Layer> layers;
		struct Retired { std::shared_ptr<GameObject> object; uint32 updatesLeft; };
		std::vector<Retired> retired;
	};

}

#endif /* FOLIAGE_H */
