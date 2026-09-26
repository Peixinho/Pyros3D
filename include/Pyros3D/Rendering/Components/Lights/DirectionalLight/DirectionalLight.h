//============================================================================
// Name        : DirectionalLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Directional Light
//============================================================================

#ifndef DIRECTIONALLIGHT_H
#define	DIRECTIONALLIGHT_H

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

namespace p3d {

// Maximum Number of Splits
#define MAX_SPLITS 4
// Split Weight
#define SPLIT_WEIGHT 0.75
// the 0.2f factor is important because we might get artifacts at
#define CASCADE_FACTOR 0.2

	struct PYROS3D_API Cascade {

		// Sub Frustum Properties
		f32 Near;
		f32 Far;
		f32 Ratio;
		f32 Fov;
		f32 Width;
		f32 Height;
		Vec3 point[8];
		Matrix CropMatrix;
		Projection ortho;

		void UpdateFrustumPoints(const Vec3& position, const Vec3& direction);

		// this function builds a projection matrix for rendering from the shadow's POV.
		// First, it computes the appropriate z-range and sets an orthogonal projection.
		// Then, it translates and scales it, so that it exactly captures the bounding box
		// of the current frustum slice
		Matrix CreateCropMatrix(const Matrix &viewMatrix, std::vector<RenderingMesh*> rcomps);

		Matrix GetCropMatrix();
	};

	class PYROS3D_API DirectionalLight : public ILightComponent {

	public:

		DirectionalLight() : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = Vec4(1, 1, 1, 1); Direction = Vec3(0, 1, 0); ShadowCascades = 0; }
		DirectionalLight(const Vec4 &color) : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = color; Direction = Vec3(0, -1, 0); ShadowCascades = 0; }
		DirectionalLight(const Vec4 &color, const Vec3 &direction) : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = color; Direction = direction; ShadowCascades = 0; }
		virtual ~DirectionalLight() {}

		virtual void Start() {};
		virtual void Update(const f64 time = 0) {};
		virtual void Destroy() {};

		virtual uint32 GetComponentType() const { return ComponentType::DirectionalLight; }

		void EnableCastShadows(const uint32 Width, const uint32 Height, const Projection &projection, const f32 Near, const f32 Far, const uint32 Cascades = 1);

		// Legacy crop-box fit, kept for the Lua binding only. The renderer
		// uses FitCascade() - see there for what was wrong with this one.
		Matrix GetLightProjection(const Matrix &ShadowViewMatrix, const uint32 Cascade, const std::vector<RenderingMesh*> RCompList);
		void UpdateCascadeFrustumPoints(const uint32 Cascade, const Vec3 &CameraPosition, const Vec3 &CameraDirection);

		// The light's shadow view: a pure rotation looking down the light
		// direction. The up vector switches away from -Z when the light
		// itself runs along Z, where LookAt would otherwise degenerate.
		static Matrix ShadowViewMatrix(const Vec3 &worldDirection);

		// Fits cascade `Cascade` to the camera that is actually rendering
		// this frame and returns its orthographic projection for the given
		// light view. A bounding sphere of the frustum slice, not a box:
		// the sphere's size does not change as the camera turns, so the
		// shadow map's texel footprint stays constant. Its centre is then
		// snapped to whole texels in light space, so moving the camera
		// slides the map by whole texels rather than re-rasterising every
		// edge at a sub-texel offset. Those two together are what stop the
		// cascades shimmering - the old crop box was refitted to the
		// frustum's light-space AABB every frame and failed both.
		//
		// Depth covers the sphere and is extended toward the light over
		// every shadow-casting mesh whose bounds overlap it, so a caster
		// outside the view still throws its shadow in.
		Matrix FitCascade(const uint32 Cascade, const Matrix &CameraWorld, const Projection &CameraProjection, const Matrix &LightView, const std::vector<RenderingMesh*> &Casters);

		// Linear view-space far distance of each cascade (unused ones 0).
		// The shaders select a cascade by comparing the fragment's view
		// depth against these.
		Vec4 GetCascadeSplits();

		// Fraction of each cascade, at its far end, over which it is
		// cross-faded into the next - and the last one faded out - so there
		// is no visible seam where the resolution steps. The next cascade
		// is fitted to start that much earlier to cover the band.
		static constexpr f32 CascadeBlendFraction = 0.1f;

		uint32 GetNumberCascades() 
		{
			return ShadowCascades; 
		}

		Cascade GetCascade(const uint32 Cascade)
		{
			return Cascades[Cascade];
		}

		const Vec3 &GetLightDirection() const 
		{ 
			return Direction; 
		}
		void SetLightDirection(const Vec3 &direction) 
		{
			Direction = direction; 
		}

	private:

		// Light Direction
		Vec3 Direction;

		// Shadow Cast
		uint32 ShadowCascades;
		Matrix ShadowProjection;
		Cascade Cascades[4];

	};

}

#endif	/* DIRECTIONALLIGHT_H */