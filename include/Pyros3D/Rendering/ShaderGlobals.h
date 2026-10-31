//============================================================================
// Name        : ShaderGlobals.h
// Author      : Duarte Peixinho
// Description : Sixteen vec4 that belong to the application: whatever it puts in one, every renderer gives its
//               shaders (uGlobals[16] - a Material Editor shader that names it, the renderer's surfaces). Process-wide,
//               like the ambient: what a script knows about the world is the same in every view of it.
//============================================================================

#ifndef PYROS_SHADERGLOBALS_H
#define PYROS_SHADERGLOBALS_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	namespace ShaderGlobals {
		static const uint32 kCount = 16;
		PYROS3D_API Vec4* Values();          // kCount of them
		inline void Set(const uint32 index, const Vec4 &value) { if (index < kCount) Values()[index] = value; }
		inline const Vec4 &Get(const uint32 index) { return Values()[index < kCount ? index : 0]; }
	}

	// What a game sets for every view the process draws, whatever renderer draws it (a script's day-night cycle,
	// a flash of lightning): the renderer applies them over each scene's own settings
	namespace RenderGlobals {
		// The ambient light, scaled (1: as each scene says)
		PYROS3D_API f32 &AmbientScale();
		// A background colour every view clears to in place of its scene's (sRGB; w 0: none set)
		PYROS3D_API Vec4 &BackgroundOverride();
		// The weather a game sets (setPostEffectParam("Sky", ...) - its uCloudCover, uHaze, uRain): the clouds' cover
		// 0..1 and the haze (1 the real air's) over each scene's own, negative for the scene's; and the rain 0..1 -
		// what faces up wet and puddled in proportion, everywhere
		PYROS3D_API f32 &CloudCover();
		PYROS3D_API f32 &Haze();
		PYROS3D_API f32 &Rain();
		// (all of it back to the engine's: a game ended)
		PYROS3D_API void Reset();
	}

}

#endif /* PYROS_SHADERGLOBALS_H */
