//============================================================================
// Name        : TerrainOcclusion.h
// Description : What the ground hides from where the camera stands.
//
//               From the eye, along a fan of directions, the terrain's height
//               is walked outwards and the steepest line of sight met so far
//               is kept for every distance: the horizon as it rises behind
//               each ridge. A thing wholly under that line, as far away as it
//               is, is behind a hill and need not be drawn.
//
//               It is made again when the eye has moved (not every frame),
//               and asked with a bounding sphere. The answer errs towards
//               "can be seen": only the ground hides, never a house or a
//               tree, and a sphere is hidden only if every direction it
//               spans, and the ones beside them, say so with room to spare.
//============================================================================

#ifndef TERRAINOCCLUSION_H
#define TERRAINOCCLUSION_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <vector>

namespace p3d {

	class SceneGraph;

	class PYROS3D_API TerrainOcclusion {
	public:
		// On unless said (a process-wide switch: to compare, or as an option).
		static void SetEnabled(const bool on);
		static bool GetEnabled();

		// Brings the horizons up to date for an eye in a scene. Cheap when the
		// eye has not moved; false when the scene has no terrain to ask.
		bool Update(SceneGraph* scene, const Vec3 &eye);
		// A bounding sphere in the world: true when the ground hides all of it.
		bool Hidden(const Vec3 &centre, const f32 radius) const;
		bool Ready() const { return ready; }
		// For checking Hidden(): whether the ground really is between an eye and
		// a point, walked a metre at a time along the line between them.
		static bool GroundBetween(SceneGraph* scene, const Vec3 &from, const Vec3 &to);

	private:
		static const int Directions = 256;
		static const int Steps = 72;
		bool ready = false;
		bool under = false;	// the eye is under the ground (a cave): nothing is hidden by a horizon then
		SceneGraph* forScene = NULL;
		Vec3 eye;
		uint32 generation = 0;
		f32 stepAt[Steps];                   // metres out, each step
		std::vector<f32> slope;              // Directions x Steps: the steepest sight line up to that step
		std::vector<f32> ridge;              // and how far out the ground that makes it is
		// The next set, made a few directions a frame (all at once it is 2-3 ms:
		// a hitch at every metre run) and put in place when it is whole.
		std::vector<f32> next, nextRidge;
		Vec3 nextEye;
		int nextDirection = -1;              // -1: none being made
		f32 drift = 0.f;                     // metres the eye is now from where the horizons in use were made
		bool Build(SceneGraph* scene, const int directions);
	};

}

#endif /* TERRAINOCCLUSION_H */
