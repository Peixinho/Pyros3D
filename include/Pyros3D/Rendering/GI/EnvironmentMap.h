//============================================================================
// Name        : EnvironmentMap.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : An HDR panorama (a photographed sky, a light probe) that a scene
//               is lit by and seen against.
//============================================================================

#ifndef ENVIRONMENTMAP_H
#define ENVIRONMENTMAP_H

#include <Pyros3D/Rendering/GI/SphericalHarmonics.h>
#include <Pyros3D/Other/Export.h>
#include <string>

namespace p3d {

	class Texture;

	// One equirectangular HDR image (a Radiance .hdr) for the whole process, as the ambient light
	// is: the scene names it ("environment"), the Sky post effect draws it where nothing else was
	// drawn, and its diffuse light - nine SH coefficients, projected here from the pixels - is
	// what ambient mode 2 lights the scene with.
	//
	// The mapping, which the Sky effect's shader reads the same way: a direction d (y up) is at
	// u = 0.5 + atan2(d.x, -d.z) / 2pi across the image, and at acos(d.y) / pi down it from the
	// top row - turned about y by the yaw given to Load, so a sky can be swung round to put its sun
	// where the scene wants it.
	class PYROS3D_API EnvironmentMap
	{
	public:
		// Never NULL: a 1x1 black texture until something is loaded, so an effect can take it as
		// an input when it is built, and the same object is filled when a scene names a sky.
		static Texture* GetTexture();
		static bool Load(const std::string &path, const f32 yawDegrees = 0.f);
		static void Clear();
		static bool IsLoaded();
		static const std::string &GetPath();
		static f32 GetYaw();
		// Radiance projections (not irradiance): what IRenderer::SetAmbientSH takes.
		static const SphericalHarmonicsL2 &GetSH();
		// The direction toward the brightest patch above the horizon - the sun of a daylight sky.
		static Vec3 GetSunDirection();
	};

	// Projects an equirectangular panorama (rows from the top, `width` x `height` RGB floats with
	// `stride` floats per pixel) into SH, every pixel weighted by the solid angle it covers.
	PYROS3D_API void ProjectEquirectToSH(const f32 *pixels, const uint32 width, const uint32 height, const uint32 stride,
		const f32 yawDegrees, SphericalHarmonicsL2 &outSH, Vec3 *outSun = NULL);
};

#endif /* ENVIRONMENTMAP_H */
