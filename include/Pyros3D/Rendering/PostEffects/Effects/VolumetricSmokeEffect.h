//============================================================================
// Name        : VolumetricSmokeEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Draws VolumetricSmoke's clouds: a ray march through the
//               voxel volumes, and a composite over the frame.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Rendering/PostEffects/VolumetricSmoke.h>

#ifndef VOLUMETRICSMOKEEFFECT_H
#define	VOLUMETRICSMOKEEFFECT_H

namespace p3d {

	// The march. For each pixel: walk the view ray through whichever clouds
	// it crosses, stopping at the scene's depth, and write the light the
	// smoke sends back in rgb and how much of the scene still shows through
	// it in alpha. Meant to run at half resolution - smoke has no detail a
	// half-size buffer loses, and a camera standing inside a cloud marches
	// every pixel on screen.
	class PYROS3D_API VolumetricSmokeEffect : public IEffect {
	public:
		// (no cloud anywhere: nothing to march through and nothing to lay over the frame)
		virtual bool IsIdle() const { return VolumetricSmoke::GetActiveCount() == 0; }
		VolumetricSmokeEffect(const uint32 Width, const uint32 Height);
		virtual ~VolumetricSmokeEffect();

		// 1/m at full thickness: how fast the smoke swallows what is behind it.
		void SetDensity(const f32 v) { density = v; }
		// Metres between samples, and the most a ray may take.
		void SetStep(const f32 metres) { stepSize = metres; }
		void SetMaxSteps(const f32 steps) { maxSteps = steps; }
		// Size and depth of the billows cut into the cloud's surface.
		void SetNoise(const f32 scale, const f32 erosion) { noiseScale = scale; noiseErosion = erosion; }
		// How dark the side away from the sun gets.
		void SetShadow(const f32 v) { shadow = v; }

	protected:
		virtual void PreDraw();

	private:
		Texture* atlas;
		uint32 atlasVersion;
		f64 startTime;
		f32 density, stepSize, maxSteps, noiseScale, noiseErosion, shadow;
	};

	// Lays the half-resolution march over the full-resolution frame. Each
	// pixel takes the smoke from the half-res neighbours that were looking
	// at the same depth as it is, so a post standing in front of a cloud
	// keeps a clean edge instead of a smoke-coloured halo.
	class PYROS3D_API VolumetricSmokeCompositeEffect : public IEffect {
	public:
		// (no cloud anywhere: nothing to march through and nothing to lay over the frame)
		virtual bool IsIdle() const { return VolumetricSmoke::GetActiveCount() == 0; }
		// The frame to composite over: RTT::Color, or an earlier effect's output.
		VolumetricSmokeCompositeEffect(const uint32 TexColor, const uint32 Width, const uint32 Height);
		VolumetricSmokeCompositeEffect(Texture* color, const uint32 Width, const uint32 Height);
		virtual ~VolumetricSmokeCompositeEffect();
	protected:
		virtual void PreDraw();
	private:
		void Build();
	};

};

#endif
