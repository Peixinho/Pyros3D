//============================================================================
// Name        : ColourSpace.h
// Author      : Duarte Peixinho
// Description : The engine lights in linear light. Every colour a person picks (a material's colour, a
//               light's, the ambient, the background) and every colour picture is sRGB-encoded - what
//               looked right on a screen - and is decoded once, where it enters the renderer. The frame
//               is encoded again once, at its end (the backbuffer's sRGB store, after the tonemap). The
//               shaders' SrgbDecode() (common.glsl) is the same curve for what is decoded on the GPU.
//============================================================================

#ifndef COLOURSPACE_H
#define COLOURSPACE_H

#include <Pyros3D/Core/Math/Vec4.h>
#include <cmath>

namespace p3d {

	inline f32 SrgbToLinear(const f32 c)
	{
		return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
	}
	inline f32 LinearToSrgb(const f32 c)
	{
		return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.f / 2.4f) - 0.055f;
	}
	// rgb decoded, alpha (coverage, not light) as it is
	inline Vec4 SrgbToLinear(const Vec4 &c) { return Vec4(SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z), c.w); }
	inline Vec3 SrgbToLinear(const Vec3 &c) { return Vec3(SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z)); }

}

#endif /* COLOURSPACE_H */
