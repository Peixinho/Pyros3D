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

		DirectionalLight() : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = Vec4(1, 1, 1, 1); Direction = Vec3(0, 1, 0); }
		DirectionalLight(const Vec4 &color) : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = color; Direction = Vec3(0, -1, 0); }
		DirectionalLight(const Vec4 &color, const Vec3 &direction) : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = color; Direction = direction; }
		virtual ~DirectionalLight() {}

		virtual void Start() {};
		virtual void Update() {};
		virtual void Destroy() {};

		void EnableCastShadows(const uint32 Width, const uint32 Height, const Projection &projection, const f32 Near, const f32 Far, const uint32 Cascades = 1);

		Matrix GetLightProjection(const Matrix &ShadowViewMatrix, const uint32 Cascade, const std::vector<RenderingMesh*> RCompList);
		void UpdateCascadeFrustumPoints(const uint32 Cascade, const Vec3 &CameraPosition, const Vec3 &CameraDirection);

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