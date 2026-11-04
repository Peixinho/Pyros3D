//============================================================================
// Name        : ShaderGlobals.cpp
// Author      : Duarte Peixinho
// Description : See ShaderGlobals.h
//============================================================================

#include <limits>
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
		Vec4 &SkySun() { static Vec4 sun(0.f, 0.f, 0.f, 0.f); return sun; }
		Vec4 &SkySunLight() { static Vec4 light(0.f, 0.f, 0.f, 0.f); return light; }
		f32 &Rain() { static f32 rain = 0.f; return rain; }
		f32 &MirrorLevel() { static f32 level = std::numeric_limits<f32>::quiet_NaN(); return level; }
		Vec4 &ScreenLens() { static Vec4 v(0.5f, 0.5f, 0.f, 0.f); return v; }
		Vec4 &ScreenLensLook() { static Vec4 v(0.f, 0.f, 0.f, 0.f); return v; }
		void Reset() { ScreenLens() = Vec4(0.5f, 0.5f, 0.f, 0.f); ScreenLensLook() = Vec4(0.f, 0.f, 0.f, 0.f); AmbientScale() = 1.f; BackgroundOverride() = Vec4(0.f, 0.f, 0.f, 0.f); CloudCover() = -1.f; Haze() = -1.f; SkySun() = Vec4(0.f, 0.f, 0.f, 0.f); SkySunLight() = Vec4(0.f, 0.f, 0.f, 0.f); Rain() = 0.f; MirrorLevel() = std::numeric_limits<f32>::quiet_NaN(); }
	}
}
