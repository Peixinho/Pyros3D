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

	// Bloom is three things: find what is bright, blur it, add it back.
	// This used to be one pass that did all three at once - 48 taps in a
	// 0.004-spaced grid, squared, added to the source - which meant the blur
	// radius was fixed in texture space (so it changed size with the
	// viewport), the threshold was a branch on the red channel alone, and
	// bright pixels were squared into whatever came out. It also called
	// texture2D(), which does not exist in GLSL 3.30 core or ES3, so it did
	// not compile at all.
	//
	// Split in two now, with PostEffectChain::AppendBuiltIn() putting the
	// existing separable BlurX/BlurY between them at quarter resolution:
	// that is where a bloom's blur belongs, it costs a sixteenth of the taps,
	// and the radius then scales with the frame instead of drifting with it.

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
