//============================================================================
// Name        : PhysicalSky.h
// Description : The physical air's light (the Sky post effect's uAtmosphere model, the same numbers) as an
//               environment for the ambient light: what the sky and the ground under it send toward a
//               surface, projected into spherical harmonics (AmbientMode 2).
//============================================================================

#ifndef PHYSICALSKY_H
#define PHYSICALSKY_H

#include <Pyros3D/Rendering/GI/SphericalHarmonics.h>

namespace p3d {

	// toSun: the way to the sun (normalised); sunStrength: IRenderer::GetSunHint's (the sun's radiance over
	// PI); haze and sunPower: the Sky effect's uAirHaze and uAirSunPower; groundAlbedo: what the ground below
	// the horizon reflects of the sun and the sky (0.15: grass, soil and rock on average). The result is in
	// the scene's light units, as the sky is drawn: a surface under it is lit as the picture of the sky says.
	PYROS3D_API void PhysicalSkySH(const Vec3 &toSun, const f32 sunStrength, const f32 haze, const f32 sunPower,
		const f32 groundAlbedo, SphericalHarmonicsL2 &out);

	// What the air leaves of the sun's light at sea level, the sun `sunY` up (the y of the way to it):
	// the same column of air and air mass the Sky effect lights its sky with - a sun's light reddens
	// as the sky does. haze: the Sky effect's uAirHaze.
	PYROS3D_API Vec3 PhysicalSunTransmittance(const f32 sunY, const f32 haze);
	// (the haze the scene's sky has - IRenderer::SetSkyAmbient keeps it; a sun's colour follows it)
	PYROS3D_API void SetSunAirHaze(const f32 haze);
	PYROS3D_API f32 GetSunAirHaze();

};

#endif /* PHYSICALSKY_H */
