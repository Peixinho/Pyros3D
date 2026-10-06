//============================================================================
// Name        : TerrainHorizon.h
// Description : The terrain's own shadow, baked as a horizon map
//============================================================================

#ifndef TERRAINHORIZON_H
#define TERRAINHORIZON_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <memory>
#include <vector>

namespace p3d {

	class SceneGraph;
	class Texture;

	// Hills shade the land behind them, and a terrain drawn into the sun's
	// shadow maps is most of what those maps cost - for a shadow that reaches
	// no further than the maps do. The ground does not move, so its shadow can
	// be worked out once: for every point of the terrain, how high the sun has
	// to stand, in each of sixteen directions round the compass, before it shows
	// over the ground between. A light's pass then needs one look at that to
	// know whether the terrain is in the sun's way - whatever the hour, and as
	// far off as the terrain goes. (Horizon mapping. Only the terrain is in it:
	// what stands on the terrain keeps its shadow maps.)
	//
	// The picture is `resolution` texels a side over the terrain's square, four
	// of them side by side (directions 0-3, 4-7, 8-11, 12-15), each channel the
	// sine of the horizon's height in that direction. Direction k looks along
	// (cos k*22.5deg, sin k*22.5deg) in x and z.
	class PYROS3D_API TerrainHorizon {
	public:
		// How many directions round the compass a horizon is kept for.
		static const int Directions = 16;

		~TerrainHorizon();

		// From the terrain tiles the scene has loaded now. `reach`: how far a
		// hill is looked for, in metres. Empty where the scene has no terrain.
		static std::shared_ptr<TerrainHorizon> Bake(SceneGraph* scene, const uint32 resolution = 512, const f32 reach = 800.f);

		Texture* GetTexture() const { return texture; }
		// (min x, min z, 1 / size x, 1 / size z): world x,z to 0-1 over the picture
		const Vec4 &GetRect() const { return rect; }
		uint32 GetResolution() const { return resolution; }
		// What the light's pass works out, on the CPU: 1 where the sun (towards
		// `toSun`, a unit vector) shows over the terrain from world x,z, 0 where
		// the ground is in its way. For checking the bake, and for a script
		// that wants to know whether somewhere is in a hill's shade.
		f32 ShadeAt(const f32 x, const f32 z, const Vec3 &toSun) const;

		// How wide the edge of the shadow is, as a sine (0.02: about a degree of sun).
		f32 softness = 0.025f;
		// The baked values, for whoever wants to look at them: resolution * resolution * Directions.
		const std::vector<unsigned char> &GetData() const { return horizons; }

	private:
		TerrainHorizon() {}
		Texture* texture = NULL;
		Vec4 rect;
		uint32 resolution = 0;
		std::vector<unsigned char> horizons;
	};

}

#endif /* TERRAINHORIZON_H */
