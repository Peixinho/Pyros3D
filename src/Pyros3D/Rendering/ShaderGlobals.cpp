//============================================================================
// Name        : ShaderGlobals.cpp
// Author      : Duarte Peixinho
// Description : See ShaderGlobals.h
//============================================================================

#include <Pyros3D/Rendering/ShaderGlobals.h>

namespace p3d {
	namespace ShaderGlobals {
		Vec4* Values() { static Vec4 values[kCount]; return values; }
	}
	namespace RenderGlobals {
		f32 &AmbientScale() { static f32 scale = 1.f; return scale; }
		Vec4 &BackgroundOverride() { static Vec4 colour(0.f, 0.f, 0.f, 0.f); return colour; }
		f32 &CloudCover() { static f32 cover = -1.f; return cover; }
		f32 &Haze() { static f32 haze = -1.f; return haze; }
		f32 &Rain() { static f32 rain = 0.f; return rain; }
		void Reset() { AmbientScale() = 1.f; BackgroundOverride() = Vec4(0.f, 0.f, 0.f, 0.f); CloudCover() = -1.f; Haze() = -1.f; Rain() = 0.f; }
	}
}
