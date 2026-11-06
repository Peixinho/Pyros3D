//============================================================================
// Name        : FluidVolumes.h
// Description : Smoke and fire simulated as a fluid on the GPU - a grid a
//               VolumetricSmoke cloud (fluid.comp), drawn by smoke.frag
//============================================================================

#ifndef FLUIDVOLUMES_H
#define FLUIDVOLUMES_H

#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <vector>
#include <Pyros3D/Rendering/Volumetrics/VolumetricSmoke.h>

namespace p3d {

	// What each VolumetricSmoke cloud is drawn from: the air in a box round it simulated - its smoke, heat and fuel
	// carried by a velocity that heat lifts, eddies keep swirling and nothing can pile up in - with an emitter that
	// makes the cloud what it is: a grenade's canister spewing smoke, a bed of fuel burning, a blast. The clouds'
	// colliders are walls to it. One storage buffer holds every grid's cells packed for drawing (the render pool).
	class FluidVolumes
	{
	public:
		static const uint32 MaxVolumes = 8;
		// A grid as smoke.frag reads it
		struct Volume
		{
			Vec3 origin; f32 cell;           // its low corner (world), a cell's size
			uint32 nx, ny, nz, offset;       // its cells, and where they start in the render pool
			Vec3 color; f32 emission;        // the cloud's colour; above 0 it burns
			f32 fade, thickness, kind, pad;  // 1 until it fades (0 gone); how thick at its thickest; 0 smoke, 1 fire, 2 blast
		};

		void Create(gfx::Device* device, gfx::Shader fluidShader);
		void Destroy();

		// The grids made, kept or let go to match the clouds - a new cloud's colliders found (its physics' rays) and its
		// grid emptied - and what each must be stepped by (the clouds' clock since it was last stepped). CPU only.
		void Prepare();
		// The steps, on the GPU: Prepare's, for every grid, then each packed for drawing (its light toward `sunDir`)
		void Record(gfx::CommandList* cl, const Vec3 &sunDir);

		bool Any() const { return !volumes.empty(); }
		const std::vector<Volume> &Volumes() const { return volumes; }
		gfx::Buffer RenderPool() const { return pool; }
		uint64 RenderPoolBytes() const { return (uint64)poolCells * 8; }

	private:
		struct Sim
		{
			int32 cloud = -1; uint32 serial = 0;
			uint32 nx = 0, ny = 0, nz = 0, cells = 0, offset = 0;
			Vec3 origin; f32 cell = 0.f;
			int kind = 0;
			gfx::Buffer vel0, vel1, scal0, scal1, pressure, aux, solid;
			gfx::BindGroup group;
			bool fresh = true;                 // (its grid to be emptied, its colliders laid in, before its first step)
			bool scalCur = false, velCur = false, presCur = false;
			f64 clock = 0.0;                   // (the clouds' clock it has been stepped to)
			f32 age = 0.f;                     // (seconds it has been simulated)
			uint32 obstacleEpoch = 0;          // (VolumetricSmoke's when its obstacles were laid)
			uint32 epochSeenFrame = 0;         // (the frame a newer epoch was first seen: laid a few frames on, once the physics has the change)
			bool closedSides = false;          // (indoors: the box's sides keep the smoke in)
			f32 enclosed = 0.f;                // (0 in the open .. 1 under a roof: the share of columns over the canister with a cover above it)
			std::vector<f32> pendingSteps;     // (Prepare's: the steps' seconds)
			uint64 lastUsed = 0;
			uint32 lightSlice = 0, lightAxis = 1;     // (the light pass's slice, and the axis it goes along)
			// (a fire's fuel spilled as a liquid - SpillFuel: two words a column after the solid bits, the top of the fuel
			// there (world y, a float's bits; below the box where none reached) and when it gets there (seconds))
			std::vector<uint32> pool;
		};
		void SpillFuel(Sim &s, const VolumetricSmoke::Cloud &c);
		void Make(Sim &s, int32 cloud);
		void Forget(Sim &s);
		void Step(gfx::CommandList* cl, Sim &s, const f32 dt);
		void LayObstacles(Sim &s, const VolumetricSmoke::Cloud &c);
		void Dispatch(gfx::CommandList* cl, Sim &s, const int pass, const f32 dt, const Vec3 &sunDir);
		uint32 Allocate(const uint32 cells);

		gfx::Device* device = nullptr;
		gfx::Pipeline pipeline;
		gfx::Buffer pool;
		uint32 poolCells = 0;
		std::vector<Sim> sims;
		std::vector<Volume> volumes;
		uint64 frame = 0;
		Vec3 sunNow;
	};

}

#endif /* FLUIDVOLUMES_H */
