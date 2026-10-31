//============================================================================
// Name        : PointLight.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Point Light
//============================================================================

#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>

namespace p3d {

	PointLight::PointLight(const Vec4 &color, const f32 radius) : ILightComponent(LIGHT_TYPE::POINT)
	{
		Color = color;
		Radius = radius;
		ShadowBiasScale = 0.02f;

		// Bounding
		minBounds = Vec3(-radius*.5f, -radius*.5f, -radius*.5f);
		maxBounds = Vec3(radius*.5f, radius*.5f, radius*.5f);
		BoundingSphereCenter = Vec3();
		BoundingSphereRadius = radius;
	}

	void PointLight::EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near)
	{
		ShadowWidth = Width;
		ShadowHeight = Height;

		// Set Flag
		isCastingShadows = true;

		// Near and Far Clip Planes
		ShadowNear = Near;
	}

}