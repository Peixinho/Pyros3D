//============================================================================
// Name        : ILightComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Lights
//============================================================================

#ifndef ILIGHTCOMPONENT_H
#define	ILIGHTCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Other/Export.h>
#include <vector>
#include <map>
#include <memory>

namespace p3d {

	namespace LIGHT_TYPE
	{
		enum {
			DIRECTIONAL = 0,
			POINT,
			SPOT
		};
	}

	class PYROS3D_API ILightComponent : public IComponent {

	public:

		ILightComponent(const uint32 type);

		virtual ~ILightComponent();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual bool NeedsUpdate() const { return false; }
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		static std::vector<IComponent*> &GetComponents();
		static std::vector<IComponent*> &GetLightsOnScene(SceneGraph* Scene);
		const Vec4 &GetLightColor() const;
		void SetLightColor(const Vec4 &color) { Color = color; }

		// Scalar multiplier on Color, kept separate from it so a light's hue
		// and its brightness stay independently authorable (a Vec4 colour
		// clamped at 1.0 per channel can't express "white, but 3x") - the
		// deferred second pass is a real Cook-Torrance BRDF whose diffuse
		// lobe is albedo/PI, so reproducing ForwardRenderer's non-physical
		// bare-Lambert brightness needs roughly a PI multiplier here rather
		// than a washed-out colour. Defaults to 1.0, so every existing light
		// keeps its exact current output.
		f32 GetLightIntensity() const { return Intensity; }
		void SetLightIntensity(const f32 intensity) { Intensity = intensity; }

		// What every renderer feeds its shaders - GetLightColor() stays the
		// raw authored value (that's what serialization and any UI wants to
		// round-trip). Alpha is deliberately left unscaled: it isn't part of
		// the light's radiance, it's carried through into the forward path's
		// _diffuse.w accumulator.
		Vec4 GetLightRadiance() const { return Vec4(Color.x * Intensity, Color.y * Intensity, Color.z * Intensity, Color.w); }

		bool IsCastingShadows() { return isCastingShadows; }
		void DisableCastShadows();

	protected:

		// Releases the shadow FBO and map, waiting for the GPU to finish with
		// them first. The wait is the whole point on Vulkan and Metal, where
		// the frames that sampled this shadow map may still be in flight when
		// a reconfiguration destroys it - the same in-flight-submission hazard
		// PostEffectsManager's destructor guards against. It is a genuine
		// no-op on GL, so this costs nothing there.
		void ReleaseShadowResources();

	public:

		FrameBuffer* GetShadowFBO();

		Texture* GetShadowMapTexture() { return ShadowMap.get(); }

		// The shadow of what does not move, kept apart from the map read by the lighting: six plain
		// R32F pictures (a point light's faces) and a framebuffer to draw them, made on first use.
		// Each frame a face that changed is put back from here and only what moves is drawn over it
		// (IRenderer's point shadow pass) - a pedestrian under a lamp no longer redraws the street.
		bool EnsureStaticShadowFaces();
		Texture* GetStaticShadowFace(const int face) { return (face >= 0 && face < 6) ? staticShadowFaces[face].get() : NULL; }
		// (one framebuffer for each face, the face attached once: under Vulkan another picture attached to
		// the same framebuffer was drawn into the first one)
		FrameBuffer* GetStaticShadowFBO(const int face) { return (face >= 0 && face < 6) ? staticShadowFBO[face].get() : NULL; }
		// A point light's map, one face at a time: a framebuffer a face, each always on its own face
		// (and one depth between them). Made the first time it is asked for, NULL for a light that is
		// not a point light casting shadows.
		FrameBuffer* GetShadowFaceFBO(const int face);

		// Volumetric in-scattering - how much light this one scatters back
		// to the eye out of the medium it passes through, on top of what it
		// does to surfaces. Consumed by DeferredRenderer's point and spot
		// passes (see secondpassSpot.glsl's march); directional lights
		// ignore it for now.
		//
		// Density defaults to 0, which skips the march entirely - so this
		// costs nothing at all for every light that doesn't ask for it, and
		// no existing scene changes behaviour.
		void SetVolumetricScattering(const f32 density) { volumetricDensity = density; }
		f32 GetVolumetricScattering() const { return volumetricDensity; }
		// Henyey-Greenstein g in (-1,1). Positive is forward-scattering,
		// which is what makes a beam brighten as you look into it; 0 is
		// isotropic. Clamped to +-0.95 in the shader.
		void SetVolumetricAnisotropy(const f32 g) { volumetricAnisotropy = g; }
		f32 GetVolumetricAnisotropy() const { return volumetricAnisotropy; }
		// Samples along the view ray. Cost is linear in this and paid per
		// pixel the light's volume covers.
		void SetVolumetricSteps(const uint32 steps) { volumetricSteps = (f32)steps; }
		uint32 GetVolumetricSteps() const { return (uint32)volumetricSteps; }

		// Shadow filter radius, in shadow-map texels: 0 is a single bilinear
		// comparison (hard edge), 1 a 3x3 tent (the default), up to 3 for a
		// 7x7 one. Whole texels, because the filter is built from one-texel
		// comparisons weighted like bilinear taps - that is what makes it
		// smooth rather than banded - and a fractional radius cannot keep
		// its taps on texel centres. The value is rounded where it is used.
		//
		// This replaced SetShadowPCFTexelSize(), whose value was a kernel
		// step in UV units defaulting to 0.0001: a fifth of a texel on a
		// 2048 map, so all 16 taps of the "4x4 PCF" landed in the same
		// texel and every shadow was a single hard, aliased comparison.
		void SetShadowSoftness(const f32 texels) { ShadowSoftness = texels < 0.f ? 0.f : (texels > (f32)MaxShadowSoftness ? (f32)MaxShadowSoftness : texels); }
		f32 GetShadowSoftness() const { return ShadowSoftness; }
		static const uint32 MaxShadowSoftness = 3;
		// Deprecated - kept so scripts written against it still run. The
		// old kernel spanned +-1.5 steps of `uvStep`, so this converts to
		// the radius in texels that covers the same footprint on this
		// light's current map (call it after enableShadows()).
		void SetShadowPCFTexelSize(const f32 uvStep) { SetShadowSoftness(1.5f * uvStep * (f32)(ShadowWidth > 0 ? ShadowWidth : 1024)); }

		// Normal-offset bias, in shadow-map texels: how far the receiver is
		// pushed along its own geometric normal before the lookup. A texel
		// of the shadow map covers a footprint that grows with distance and
		// with how obliquely the light grazes the surface; offsetting by a
		// multiple of that footprint removes acne at every distance and
		// angle at once, which a constant depth offset cannot - the reason
		// spot lights needed polygon offsets like 40/64 and still acned on
		// one backend but not another. Scaled by the receiver's slope to
		// the light in the shader, so a surface facing the light straight
		// on is barely moved.
		void SetShadowNormalBias(const f32 texels) { ShadowNormalBias = texels < 0.f ? 0.f : (texels > MaxShadowNormalBias ? MaxShadowNormalBias : texels); }
		f32 GetShadowNormalBias() const { return ShadowNormalBias; }
		static constexpr f32 MaxShadowNormalBias = 7.9f;

		// Both of the above, packed into the one float slot the forward
		// light matrix and the deferred passes carry for them: the integer
		// part is the filter radius, the fraction is normal bias / 8.
		// Decoded by ShadowFilterRadius()/ShadowNormalOffset() in the
		// shaders (PyrosShader.glsl, secondpass*.glsl, MaterialCodegen).
		//
		// And a third, the penumbra (SetShadowPenumbra): its centimetres are four times
		// the integer part's worth over the radius - integer = radius + 4 x centimetres -
		// so a light without one packs exactly as it did.
		f32 GetShadowFilterPacked() const
		{
			const f32 radius = floorf(ShadowSoftness + 0.5f);
			const f32 centimetres = floorf(ShadowPenumbra * 100.f + 0.5f);
			return radius + 4.f * centimetres + ShadowNormalBias / 8.f;
		}

		// How wide a shadow's soft edge is, in metres, wherever it falls: the same in every
		// cascade and at every distance, where a filter radius in texels is wider the further
		// a cascade reaches. 0 (the default) leaves the shadow to Softness alone. Directional
		// lights only - a sun's shadows (point and spot keep their filter). Up to 2.5 m.
		void SetShadowPenumbra(const f32 metres) { ShadowPenumbra = metres < 0.f ? 0.f : (metres > MaxShadowPenumbra ? MaxShadowPenumbra : metres); }
		f32 GetShadowPenumbra() const { return ShadowPenumbra; }
		static constexpr f32 MaxShadowPenumbra = 2.5f;

		uint32 GetShadowWidth()
		{
			return ShadowWidth;
		}

		uint32 GetShadowHeight()
		{
			return ShadowHeight;
		}

		const f32 &GetShadowNear() const
		{
			return ShadowNear;
		}

		virtual const f32 &GetShadowFar() const
		{
			return ShadowFar;
		}

		void SetShadowNear(const f32 Near)
		{
			ShadowNear = Near;
		}

		void SetShadowFar(const f32 Far)
		{
			ShadowFar = Far;
		}

		void SetShadowBias(const f32 factor, const f32 units)
		{
			ShadowBiasFactor = factor;
			ShadowBiasUnits = units;
		}
		const f32 &GetShadowBiasFactor() const
		{
			return ShadowBiasFactor;
		}

		const f32 &GetShadowBiasUnits() const
		{
			return ShadowBiasUnits;
		}

		const uint32 &GetLightType() const
		{
			return LightType;
		}

	protected:

		// Shadows Mapping
		// FrameBuffer - owned whenever isCastingShadows is true; assigning a
		// new one via .reset() always releases whichever was previously owned,
		// so re-enabling shadows can't leak the prior FBO/texture.
		std::unique_ptr<FrameBuffer> shadowsFBO;
		std::unique_ptr<FrameBuffer> staticShadowFBO[6];
		// (see GetShadowFaceFBO: one framebuffer re-aimed at each face in turn began every pass on the
		// face it was left on, and Metal and Vulkan clear what a pass begins on - the face drawn last the
		// frame before was wiped, and with the shadow cache it was not drawn again)
		std::unique_ptr<FrameBuffer> shadowFaceFBO[6];
		std::unique_ptr<Texture> shadowFaceDepth;
		Texture* shadowFaceFBOFor = NULL;
		std::unique_ptr<Texture> staticShadowFaces[6];
		// Dimensions
		uint32 ShadowWidth, ShadowHeight;
		// Shadow Map Texture
		std::unique_ptr<Texture> ShadowMap;
		// Far ane Near for Projection
		f32 ShadowNear, ShadowFar;
		// Flag
		bool isCastingShadows;
		// Bias Offset
		f32 ShadowBiasFactor, ShadowBiasUnits;
		// Light Color
		Vec4 Color;
		// Brightness multiplier on Color - see GetLightIntensity().
		f32 Intensity;

		// Internal - List of Lights
		static std::vector<IComponent*> Components;

		uint32 LightType;

		// See SetShadowSoftness() / SetShadowNormalBias().
		f32 ShadowSoftness, ShadowNormalBias, ShadowPenumbra = 0.f;

		// See SetVolumetricScattering() - density 0 disables the march.
		f32 volumetricDensity, volumetricAnisotropy, volumetricSteps;

	};

};

#endif /* ILIGHTCOMPONENT_H */