//============================================================================
// OceanSim: a Tessendorf FFT ocean - four cascades of 256x256, each a square of sea (600, 85, 12 and 2.1 m)
// whose waves are drawn from a wind spectrum (JONSWAP wind sea plus a swell, cos-2s spread) and evolved in time
// at each wave's own speed, gravity-capillary dispersion for the smallest.
//
// The design - cascade sizes and the cuts between them, the spectrum, the packing of two real fields into one
// complex transform, slopes and their squares for roughness (LEAN), foam from the Jacobian with a recovery rate,
// time looped so phase never loses precision - follows Ultra Water (https://github.com/Humpalumps/Ultra-Water,
// MIT, (c) 2026 Humpalumps; see THIRD_PARTY_NOTICES.md). Its GPU passes are done here on the CPU, across the
// job workers, and uploaded: the same on every backend.
//
// Each cascade gives two textures, RGBA32F (one layout every backend uploads alike), mipmapped, repeating:
//   displacement  (lambda Dx, Dy, lambda Dz, turbulence - 1 calm, under ~0.6 where a crest folds: foam)
//   slope         (sx, sz, sx^2, sz^2)  - their mips hold the mean square slope: roughness that does not alias
//============================================================================
#ifndef OCEANSIM_H
#define OCEANSIM_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <memory>
#include <vector>
#include <complex>

namespace p3d {

	class Texture;

	struct PYROS3D_API OceanSimParams
	{
		f32 windSpeed = 9.f, windDirection = 20.f, fetchKm = 120.f;
		f32 spreadBlend = 0.85f, windSwell = 0.25f, gamma = 3.3f, shortWavesFade = 0.04f;
		f32 capillaryFade = 0.08f;
		f32 waveScale = 1.f, swellScale = 0.7f, swellWindSpeed = 7.f, swellDirection = -15.f, swellFetchKm = 900.f, swellSwell = 1.f;
		f32 choppiness = 0.85f, foamRecovery = 0.7f;
		// 1: the spectrum's own energy (the reference ran at 8 - see OceanSim.cpp)
		f32 energy = 1.f;
		uint32 seed = 1337;
	};

	class PYROS3D_API OceanSim
	{
	public:
		static const uint32 N = 256;
		static const uint32 LogN = 8;
		static const uint32 Cascades = 4;

		explicit OceanSim(const OceanSimParams &params = OceanSimParams());
		~OceanSim();

		// The wind changed: the spectrum made again (it is not cheap - call it when a setting moves, not each frame).
		void SetParams(const OceanSimParams &params);
		const OceanSimParams &GetParams() const { return params; }
		// Advance by dt seconds and upload this moment's fields.
		void Update(const f32 dt);

		f32 GetLength(const uint32 cascade) const;
		std::shared_ptr<Texture> GetDisplacement(const uint32 cascade) const { return displacement[cascade % Cascades]; }
		std::shared_ptr<Texture> GetSlope(const uint32 cascade) const { return slope[cascade % Cascades]; }
		f32 GetTime() const { return (f32)time; }
		// Last update's cost on the calling thread (the upload, and waiting if the workers were late), ms.
		f32 GetLastUpdateMs() const { return lastMs; }
		// What the workers spent on the last moment, all cascades together, ms.
		f32 GetLastWorkMs() const { return stepMs[0] + stepMs[1] + stepMs[2] + stepMs[3]; }
		// The surface's height at world (x, z) as drawn this frame: the choppy sideways travel undone (the point
		// whose displaced position lands at x, z, by fixed-point steps) and the three displacing cascades summed,
		// each weighted (the sea shader's damping over the shallows).
		f32 HeightAt(const f32 x, const f32 z, const Vec3 &weights = Vec3(1.f, 1.f, 1.f)) const;
		// A check on the spectrum: cascade's rms height (m), mean square slope, and the share of it that folds (foam).
		Vec3 GetStats(const uint32 cascade);

	private:
		typedef std::complex<f32> cf;
		void BuildSpectrum();
		void Evolve(const uint32 c);
		void Assemble(const uint32 c);
		void Fft(f32* re, f32* im);
		void FftColumns(f32* re, f32* im);
		void Step(const uint32 c);

		OceanSimParams params;
		f64 time = 0.0;
		f32 lastMs = 0.f, lastDt = 0.f;
		JobCounter counter;
		f32 stepMs[Cascades] = { 0.f, 0.f, 0.f, 0.f };
		bool running = false;
		// h0(k) and conj(h0(-k)) per cascade
		std::vector<cf> h0[Cascades], h0mc[Cascades];
		// four complex fields per cascade, each two real fields packed: (Dx + iDz), (Dy + iDxz), (Dyx + iDyz), (Dxx + iDzz),
		// their real and imaginary parts apart (so the transform's loops vectorise)
		std::vector<f32> wre[Cascades][4], wim[Cascades][4];
		// per wave: its wave vector, 1/k, and its frequency as a whole number of 2 pi / LOOP
		std::vector<f32> kxs[Cascades], kzs[Cascades], iks[Cascades];
		std::vector<f64> quant[Cascades];
		std::vector<f32> turbulence[Cascades];
		std::vector<f32> dispOut[Cascades], slopeOut[Cascades];
		// what was uploaded last: (Dx, Dy, Dz) of the three displacing cascades, for HeightAt
		std::vector<f32> shown[3];
		Vec3 Displacement(const f32 x, const f32 z, const Vec3 &weights) const;
		std::vector<cf> twiddle;
		std::vector<uint32> bitrev;
		std::shared_ptr<Texture> displacement[Cascades], slope[Cascades];
	};

}

#endif
