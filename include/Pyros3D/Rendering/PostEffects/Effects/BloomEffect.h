//============================================================================
// Name        : BloomEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Bloom, as a bright pass and a composite.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef BLOOMEFFECT_H
#define	BLOOMEFFECT_H

namespace p3d {

	class PostEffectsManager;

	// The built-in "Bloom" entry. threshold/knee/intensity default to
	// 0.8, 0.35 and 1 when the chain has no overrides.
	PYROS3D_API void AppendBloom(PostEffectsManager &manager, const uint32 width, const uint32 height,
		const f32 threshold, const f32 knee, const f32 intensity);

	// Bloom is three things: find what is bright, blur it, add it back.
	//
	// The blur is a mip chain (half, quarter, eighth, sixteenth), not a
	// fixed number of texels. A 7-tap gaussian on a quarter-resolution
	// buffer is about twelve screen pixels whatever the window is, so the
	// same setting was a blob in a small viewport and a hard square halo
	// in a large one. Each mip is a fraction of the frame, so the halo
	// covers the same share of the picture at every resolution. The
	// downsample taps half a texel, and the upsample is a 3x3 tent, so a
	// viewport whose size is not a multiple of the mip ratio does not come
	// back as blocks or a shimmering grid.
	//
	// AppendBloom() builds that chain. The two classes below are the ends
	// of it; the mips in between are an implementation detail.

	// Keeps what is brighter than `threshold`, rolled in over a soft `knee`
	// so a surface drifting past the threshold brightens gradually instead of
	// popping. Reads luminance, not red.
	class PYROS3D_API BloomBrightPassEffect : public IEffect {
	public:
		BloomBrightPassEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~BloomBrightPassEffect();

		void SetThreshold(const f32 &v);
		void SetKnee(const f32 &v);

	private:
		Uniform *thresholdHandle, *kneeHandle;
		f32 threshold, knee;
	};

	// base + bloom * intensity. `base` is the image the bloom is added to:
	// RTT::Color when bloom is the first thing in a chain, otherwise the
	// texture of whatever ran before it, so bloom composes with the rest of
	// the chain instead of discarding it.
	class PYROS3D_API BloomCompositeEffect : public IEffect {
	public:
		BloomCompositeEffect(Texture* base, const uint32 Width, const uint32 Height);
		BloomCompositeEffect(const uint32 baseRTT, const uint32 Width, const uint32 Height);
		virtual ~BloomCompositeEffect();

		void SetIntensity(const f32 &v);

	private:
		void Build(const uint32 Width, const uint32 Height);
		Uniform *intensityHandle;
		f32 intensity;
	};

};

#endif	/* BLOOMEFFECT_H */
