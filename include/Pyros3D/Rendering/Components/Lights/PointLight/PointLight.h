//============================================================================
// Name        : PointLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Point Light
//============================================================================

#ifndef POINTLIGHT_H
#define	POINTLIGHT_H

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>

namespace p3d {

	class PYROS3D_API PointLight : public ILightComponent {

	public:

		PointLight() : ILightComponent(LIGHT_TYPE::POINT) { Color = Vec4(1, 1, 1, 1); Radius = 1.f; }
		PointLight(const Vec4 &color, const f32 radius);
		virtual ~PointLight() {}

		virtual void Start() {};
		virtual void Update(const f64 time = 0) {};
		virtual void Destroy() {};

		virtual const f32 &GetShadowFar() const
		{
			return Radius;
		}

		const f32 &GetLightRadius() const 
		{ 
			return Radius; 
		}

		void SetLightRadius(const f32 radius) 
		{
		   	Radius = radius; 
		}

		void EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near = 0.1f);

	protected:

		// Attenuation
		f32 Radius;

	};

}

#endif	/* POINTLIGHT_H */
