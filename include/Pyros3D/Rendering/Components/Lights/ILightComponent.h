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

		FrameBuffer* GetShadowFBO();

		Texture* GetShadowMapTexture() { return ShadowMap.get(); }

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

		void SetShadowPCFTexelSize(f32 texel) { pcfTexel = texel; }
		f32 GetShadowPCFTexelSize() { return pcfTexel; }

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

		f32 pcfTexel;

		// See SetVolumetricScattering() - density 0 disables the march.
		f32 volumetricDensity, volumetricAnisotropy, volumetricSteps;

	};

};

#endif /* ILIGHTCOMPONENT_H */