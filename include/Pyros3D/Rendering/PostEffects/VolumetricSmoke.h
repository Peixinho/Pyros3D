//============================================================================
// Name        : VolumetricSmoke.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Smoke clouds as voxel volumes that fill the space around
//               them and stop at whatever is solid.
//============================================================================

#ifndef VOLUMETRICSMOKE_H
#define VOLUMETRICSMOKE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <vector>

namespace p3d {

	class IPhysics;
	class IComponent;

	// A smoke grenade's cloud is not a shape, it is a quantity of smoke let
	// loose in a place: in the open it settles into a dome, in a room it
	// takes the shape of the room, and through a doorway it spills out. So a
	// cloud here is a small voxel grid around the point it started from,
	// filled outward cell by cell - nearest first, until the smoke runs out
	// - and a cell is only reached from a neighbour the physics world has a
	// clear line to. Walls, floors and ceilings are whatever has a collider.
	//
	// The fill is done once, when the cloud is spawned. What changes
	// afterwards is two numbers per cloud: how far along that fill the
	// visible smoke has got (`growth`) and how thick it is (`density`). Each
	// voxel stores when the fill reached it, so growing the cloud is the
	// shader comparing that against `growth`, and nothing is re-uploaded.
	//
	// All clouds share one texture: a cloud's grid is NY slices of NX x NZ
	// texels laid out in a tile, and the tiles sit side by side in an atlas.
	// VolumetricSmokeEffect owns the GPU copy and draws them; this class is
	// the CPU side and has no device dependencies, so a script can drive it
	// whether or not anything is drawing.
	//
	// Process-wide, like the ambient light: every scene and viewport in the
	// process sees the same clouds.
	class PYROS3D_API VolumetricSmoke {
	public:
		enum {
			MaxClouds = 64,
			NX = 32, NY = 16, NZ = 32,
			SlicesPerRow = 4,                  // NY slices as a 4 x 4 block
			TileSize = NX * SlicesPerRow,      // 128
			TilesPerRow = 8,
			AtlasWidth = TileSize * TilesPerRow,                 // 1024
			AtlasHeight = TileSize * (MaxClouds / TilesPerRow)   // 1024
		};

		struct Cloud
		{
			bool active;
			Vec3 boxMin;          // world position of the grid's low corner
			// World box around the voxels the fill actually reached. The
			// grid is sized for the worst case and mostly empty, and a ray
			// that only has to cross this instead spends its steps on smoke.
			Vec3 boundsMin, boundsMax;
			Vec3 color;
			f32 growth;           // 0..1, how much of the fill is showing
			f32 density;          // 0..1, fades the whole cloud
			f32 age;
			f32 growTime, lifeTime, fadeTime;
			// 0 for smoke. Above it the cloud is fire: it gives off its
			// colour instead of being lit, scaled by this, and is drawn as
			// flames rising off wherever the fill reached.
			f32 emission;
			// For fire only. 0: flames, thinning to tongues toward the top.
			// 1: a blast, burning solid right through.
			f32 blast;
			uint32 cells;         // voxels the fill reached
			Cloud() : active(false), growth(0.f), density(0.f), age(0.f),
				growTime(1.f), lifeTime(1.f), fadeTime(1.f), emission(0.f), blast(0.f), cells(0) {}
		};

		// Starts a cloud at `position` and returns its slot, or -1 when all
		// MaxClouds are in use. `radius` is the dome the smoke would make on
		// open ground; it is really a volume, so the same grenade in a small
		// room fills the room and pushes out of the door. `physics` may be
		// NULL, and the smoke then ignores the world.
		//
		// The cloud grows for growTime seconds, holds, and thins out over
		// the last fadeTime seconds of lifeTime - but only if something
		// calls Update().
		//
		// `rise` is what a step up costs the fill against a step sideways:
		// under 1 it stands up as a ball (smoke, 0.85), well over 1 it stays
		// low and runs along the floor (burning fuel, 2 to 3). `emission`
		// above 0 makes it fire - see Cloud::emission - and `blast` says
		// which kind, flames (0) or the solid ball of an explosion (1).
		static int32 Spawn(IPhysics* physics, const Vec3 &position, const Vec3 &color,
			const f32 radius, const f32 growTime, const f32 lifeTime, const f32 fadeTime,
			const f32 rise = 0.85f, const f32 emission = 0.f, const f32 blast = 0.f);

		// Ages every cloud and retires the ones that have run out.
		static void Update(const f32 dt);

		static void Remove(const int32 id);
		static void Clear();

		static uint32 GetActiveCount();
		static const Cloud &GetCloud(const uint32 id) { return clouds[id]; }

		// Side of a voxel in metres. Applies to clouds spawned afterwards
		// only if none are alive - the shader has one cell size for all.
		static void SetCellSize(const f32 size);
		static f32 GetCellSize() { return cellSize; }

		// A point or spot light near the smoke.
		struct LocalLight
		{
			Vec3 position;
			f32 radius;
			Vec3 color;           // radiance: colour x intensity
			Vec3 direction;       // the way a spot points; unused for a point
			f32 cosInner, cosOuter;   // both -1 for a point light
		};

		// What the smoke is lit by. A renderer calls this with the scene's
		// lights and ambient each time it draws, so smoke follows the scene
		// by itself: the brightest directional light is its sun, and point
		// and spot lights light it where they reach. `ambient` is the
		// scene's ambient as the renderer has it.
		static void CaptureLights(const std::vector<IComponent*> &lights, const Vec3 &ambient);

		// Overrides for a scene with no lights of its own, or a look that
		// wants something else: once either is called, CaptureLights stops
		// touching that value. UseSceneLighting() hands them back.
		static void SetLight(const Vec3 &towardLight, const Vec3 &color);
		static void SetAmbient(const Vec3 &color);
		static void UseSceneLighting();
		static const std::vector<LocalLight> &GetLocalLights() { return localLights; }
		// Metres per second the detail in the smoke drifts.
		static void SetWind(const Vec3 &wind);
		static const Vec3 &GetLightDirection() { return lightDirection; }
		static const Vec3 &GetLightColor() { return lightColor; }
		static const Vec3 &GetAmbient() { return ambient; }
		static const Vec3 &GetWind() { return wind; }

		// Washes the whole frame toward a colour: 0 leaves it alone, 1 is
		// nothing but the colour. What a stun grenade does to whoever was
		// looking at it. Lives here because the composite that draws the
		// smoke is the last thing to touch the frame anyway.
		static void SetScreenFlash(const Vec3 &color, const f32 amount);
		static f32 GetScreenFlash() { return screenFlash; }
		static const Vec3 &GetScreenFlashColor() { return screenFlashColor; }

		// RGBA8, AtlasWidth x AtlasHeight. R: how much smoke the voxel may
		// hold, lowered next to a wall so the interpolated field reaches
		// zero at the wall instead of half a voxel past it. G: when the fill
		// reached it, 0..1; 255 where it never did. B: whether the sun
		// reaches the voxel past the world's geometry - what keeps smoke
		// under a roof from being lit like smoke in a field.
		static const std::vector<uchar> &GetAtlas();
		// Bumped whenever the atlas changes; a holder of a GPU copy compares.
		static uint32 GetAtlasVersion() { return atlasVersion; }

	private:
		static void ClearTile(const uint32 id);

		static Cloud clouds[MaxClouds];
		static std::vector<uchar> atlas;
		static uint32 atlasVersion;
		static f32 cellSize;
		static Vec3 lightDirection, lightColor, ambient, wind;
		static bool manualLight, manualAmbient;
		static f32 screenFlash;
		static Vec3 screenFlashColor;
		static std::vector<LocalLight> localLights;
	};

}

#endif /* VOLUMETRICSMOKE_H */
