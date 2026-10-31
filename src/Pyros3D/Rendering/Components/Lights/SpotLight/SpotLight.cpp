//============================================================================
// Name        : SpotLight.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Spot Light
//============================================================================

#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>

namespace p3d {

	SpotLight::SpotLight(const Vec4 &color, const f32 radius, const Vec3 &direction, const f32 OutterCone, const f32 InnerCone) : ILightComponent(LIGHT_TYPE::SPOT)
	{
		// ILightComponent's 2/1 polygon offset, like the other lights. This
		// used to be 40/64: a perspective shadow map's depth slack shrinks
		// with distance from the light, so a constant offset had to be huge
		// to clear a floor grazed far away - and even then GL, Vulkan and
		// Metal disagreed. The shaders' normal-offset bias
		// (SetShadowNormalBias) now scales with each texel's world
		// footprint instead, and measured clean on all three at 0/0; 2/1 is
		// kept only for the filter's own footprint on slopes.
		Color = color;
		Radius = radius;
		innerCone = InnerCone;
		outterCone = OutterCone;
		CosOutterCone = cosf((f32)DEGTORAD(OutterCone));
		CosInnerCone = cosf((f32)DEGTORAD(InnerCone));
		Direction = direction;

		// Bounding
		minBounds = Vec3(-radius*.5f, -radius*.5f, -radius*.5f);
		maxBounds = Vec3(radius*.5f, radius*.5f, radius*.5f);
		BoundingSphereCenter = Vec3();
		BoundingSphereRadius = radius;
	}

	void SpotLight::EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near)
	{
		ShadowWidth = Width;
		ShadowHeight = Height;

		// Set Flag
		isCastingShadows = true;

		// Near and Far Clip Planes
		ShadowNear = Near;
		ShadowFar = Radius;
	}

}