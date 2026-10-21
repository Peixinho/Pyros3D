//============================================================================
// Name        : VolumetricLightEffect.h
// Description : Light in the air: height fog lit by the sun through its shadow.
//============================================================================
#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef VOLUMETRICLIGHTEFFECT_H
#define	VOLUMETRICLIGHTEFFECT_H

namespace p3d {

	// The air between the eye and what it sees: fog that thins with height, lit by the sun where
	// the sun reaches it - so it is dark in the shade of a building and bright in the gap
	// beside, and the light comes in shafts (through trees, down a street, through a window).
	// Marched at half resolution along each pixel's ray, the sun's own shadow map deciding each
	// step's light (IRenderer::GetSunShadow); VolumetricLightCompositeEffect lays it over the
	// frame at full resolution. The "VolumetricLight" post effect is the pair.
	class PYROS3D_API VolumetricLightEffect : public IEffect {
	public:
		VolumetricLightEffect(const uint32 Width, const uint32 Height);
		virtual ~VolumetricLightEffect();

		// Fog per metre at the base height, and how fast it thins above it (per metre).
		void SetDensity(const f32 v) { density = v; }
		void SetHeightFalloff(const f32 v) { falloff = v; }
		void SetBaseHeight(const f32 v) { baseHeight = v; }
		// How much of the sun the air sends towards the eye (1: as much as physics says).
		void SetScattering(const f32 v) { scattering = v; }
		// -1..1: 0 the same every way; towards 1 bright looking into the sun (haze), towards -1 away.
		void SetAnisotropy(const f32 v) { anisotropy = v; }
		// The sky's light in the fog where the sun does not reach (its colour times this).
		void SetAmbient(const Vec3 &colour, const f32 strength) { ambientColour = colour; ambient = strength; }
		void SetMaxDistance(const f32 metres) { maxDistance = metres; }
		void SetSteps(const f32 n) { steps = n; }

	protected:
		virtual void PreBind();
		virtual void PreDraw();

	private:
		Texture* noShadow;          // (bound when no sun casts: the slot is a shadow sampler either way)
		f32 density, falloff, baseHeight, scattering, anisotropy, ambient, maxDistance, steps;
		Vec3 ambientColour;
		f64 startTime;
	};

	// Lays the march over the frame: the frame dimmed by what the air hides, the air's light
	// added - upsampled from half resolution following depth, so the fog does not bleed over
	// the edges of what is in front of it.
	class PYROS3D_API VolumetricLightCompositeEffect : public IEffect {
	public:
		// (the march is the effect just before it: LastRTT)
		VolumetricLightCompositeEffect(const uint32 TexColor, const uint32 Width, const uint32 Height);
		VolumetricLightCompositeEffect(Texture* color, const uint32 Width, const uint32 Height);
		virtual ~VolumetricLightCompositeEffect() {}
	private:
		void Build();
	};
};

#endif /* VOLUMETRICLIGHTEFFECT_H */
