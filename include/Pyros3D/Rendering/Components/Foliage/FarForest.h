//============================================================================
// Name        : FarForest.h
// Author      : Duarte Peixinho
// Description : A terrain's forests out to the horizon - every tree past where
//               the loaded tiles' own trees end, as a canopy of a few dozen
//               triangles, standing where the tree does.
//============================================================================

#ifndef FARFOREST_H
#define FARFOREST_H

#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class SceneGraph;
	class GameObject;
	class TerrainComponent;
	class InstanceCullGroup;
	class RenderingInstancedComponent;

	// A streamed terrain has its trees only on the tiles loaded round the eye: past them the
	// forest stopped, and a hill two kilometres off was bare ground painted green. The far
	// forest is every tree of the whole terrain, made once from what the near ones are made
	// from - the same layer numbers and seeds, its density maps for the whole terrain, the
	// ground of its overview - so each stands on the spot its tree does when its tile loads,
	// as a rounded canopy (or a cone, for a conifer) tinted the kind's colour. It is drawn
	// in a few blocks culled on the GPU (InstanceCullGroup), one draw a shape: it comes in
	// where the near trees go (fadeIn), and thins and goes with distance as they do.
	//
	// The config, a JSON file (paths relative to the asset root):
	//   { "terrain": "<object name>", "blockTiles": 4, "keep": 0.3,
	//     "fadeIn": [700, 850], "thin": [1500, 5000, 0.35], "fade": [6500, 8000],
	//     "kinds": [ { "name", "shape": "round" | "cone", "size": [radius, height],
	//                  "color": [r, g, b], "densityMap": "<whole terrain, row 0 at the
	//                  terrain's -z edge>", and the near layer's own numbers: "density",
	//                  "seed", "blockSize", "minScale", "maxScale", "maxSlopeDegrees",
	//                  "minHeight", "maxHeight", "sink" } ] }
	// keep: the share of each near block the far forest stands for - what the near layer
	// keeps of it where it is thinnest (its thinDensity), so that the trees on both sides of
	// the turn are the same trees; they are grown as the near ones are, to cover the ground.
	class PYROS3D_API FarForest
	{
	public:
		// NULL (and why, in `error`) when the scene has no such terrain with an overview, or
		// the config cannot be read. Its objects are the scene's until it goes.
		static std::shared_ptr<FarForest> Build(SceneGraph* scene, const std::string &configPath, const std::string &assetRoot, std::string &error);
		~FarForest();
		uint32 Trees() const { return trees; }

	private:
		FarForest() {}
		SceneGraph* scene = NULL;
		std::shared_ptr<GameObject> owner;
		std::vector<std::shared_ptr<InstanceCullGroup> > groups;
		std::vector<std::shared_ptr<GameObject> > blocks;
		uint32 trees = 0;
	};

}

#endif /* FARFOREST_H */
