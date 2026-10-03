//============================================================================
// Name        : MotionBlur.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : MotionBlur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.h>

namespace p3d {

    MotionBlurEffect::MotionBlurEffect(const uint32 Tex1, Texture* VelocityMap, Texture* VelocityDepth, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
    {

		// Set RTT
		UseRTT(Tex1);
		UseCustomTexture(VelocityMap);
		UseCustomTexture(VelocityDepth);

		cfps = 60.0f;
		tfps = 60.0f;
		// Shutter length in target-rate frames. One frame is a soft edge
		// on anything but a whip pan, which reads as no blur at all; this
		// is long enough that the direction of the motion shows. The
		// samples below are centred on the pixel, so the length is a
		// streak rather than a sharp picture plus a ghost.
		strength = 3.25f;
		f32 vel = strength;
		velHandle = AddUniform(Uniform("uVelocityScale", Uniforms::DataType::Float, &vel));
		reprojectHandle = AddUniform(Uniform("uReproject", Uniforms::DataType::Matrix, &reproject));

		// Must stay in lockstep with the fragment shader's own `#if
		// defined(VULKAN)` below, and those two macros are NOT the same
		// thing: VULKAN_BACKEND/METAL_BACKEND are C++ build flags
		// (cmake/PyrosBackend.cmake), while VULKAN is predefined by
		// shaderc for *any* SPIR-V target - which includes the Metal
		// backend, since it compiles GLSL through shaderc before handing
		// the SPIR-V to SPIRV-Cross. Guarding this half on VULKAN_BACKEND
		// alone (as it did, from before the Metal backend existed) left
		// Metal taking the UBO branch in the shader and the loose-uniform
		// branch here: nothing ever created or filled MotionBlurParams, so
		// uVelocityScale.x read back as 0, velocity scaled to nothing and
		// nSamples collapsed to 1 - the effect ran every frame as an exact
		// pass-through. The velocity map itself was fine; only the scale
		// was missing (confirmed by rendering both to screen).
#if defined(VULKAN_BACKEND) || defined(METAL_BACKEND)
		// Vulkan/Metal reject loose floats - deliver scale via UBO.
		// mat4, then the float at std140 offset 64 (the vec4 it lands in).
		extraUniformsBinding = 26;
		extraUniformsBlockName = "MotionBlurParams";
		extraUniformsSize = 80;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uReproject"] = 0;
		extraUniformOffsets["uVelocityScale"] = 64;
#else
		// GL: plain uniforms + SendUniform. Avoids macOS driver std140
		// packing mismatches that left the scale at 0 (identity blur).
		extraUniformsBinding = 0;
#endif

		// Fragment: VULKAN (shaderc) uses a UBO; GL uses loose uniforms.
		FragmentShaderString =
									"#define MAX_SAMPLES 32\n"
									"#define varying_in in\n"
									"#define varying_out out\n"
									"#define attribute_in in\n"
									"#define texture_2D texture\n"
									"#define texture_cube texture\n"
									#if defined(GLES3)
										"precision highp float;\n"
									#endif
									"#if defined(VULKAN)\n"
									"#define UBO_BINDING(n) layout(std140, binding = n)\n"
									"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
									"#define IO_LOCATION(n) layout(location = n)\n"
									"#else\n"
									"#define UBO_BINDING(n) layout(std140)\n"
									"#define SAMPLER_BINDING(n)\n"
									"#define IO_LOCATION(n)\n"
									"#endif\n"
									"IO_LOCATION(0) out vec4 FragColor;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
								"SAMPLER_BINDING(2) uniform sampler2D uTex2;\n"
								"#if defined(VULKAN)\n"
								"UBO_BINDING(26) uniform MotionBlurParams {\n"
								"	mat4 uReproject;\n"
								"	vec4 uVelocityScale;\n"
								"};\n"
								"#else\n"
								"uniform mat4 uReproject;\n"
								"uniform float uVelocityScale;\n"
								"#endif\n"
								// Same UV <-> clip mapping as TAAResolveEffect. Vulkan (and
								// Metal, which compiles through shaderc and therefore
								// also defines VULKAN) stores the top row at v=0 while
								// the Y-flipped projection does not, so a straight
								// uv*2-1 sends the sky the opposite way to the meshes.
								"#if defined(VULKAN)\n"
								"vec2 UVToNDC(vec2 uv) { return vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); }\n"
								"vec2 NDCToUV(vec2 n) { return vec2(n.x * 0.5 + 0.5, 0.5 - n.y * 0.5); }\n"
								"#else\n"
								"vec2 UVToNDC(vec2 uv) { return uv * 2.0 - 1.0; }\n"
								"vec2 NDCToUV(vec2 n) { return n * 0.5 + 0.5; }\n"
								"#endif\n"
								// The velocity shader differences clip-space UVs. On
								// Metal those run the opposite way to this pass's
								// vTexcoord (see IEffect's vertex shader).
								"vec2 MeshVelocity(vec2 v) {\n"
								"#if defined(METAL)\n"
								"	v.y = -v.y;\n"
								"#endif\n"
								"	return v;\n"
								"}\n"
								"void main() {\n"
									"vec2 texelSize = 1.0 / vec2(textureSize(uTex0, 0));\n"
									"vec2 uv = vTexcoord;\n"
									// This pixel's own velocity when something was
									// drawn here. Taking the fastest neighbour instead
									// paints a rotating surface with its rim's
									// direction, so the whole object streaks one way.
									// Sky keeps the closest foreground velocity in the
									// 3x3, or the camera reprojection when there is
									// no foreground - otherwise the smear stops dead
									// on the silhouette and the background stays sharp
									// while the world moves.
									"float centerDepth = texture(uTex2, uv).r;\n"
									"vec2 velocity;\n"
									"if (centerDepth < 0.99999) {\n"
									"	velocity = MeshVelocity(texture(uTex1, uv).rg);\n"
									"} else {\n"
									"	float bestDepth = 2.0;\n"
									"	vec2 best = vec2(0.0);\n"
									"	bool found = false;\n"
									"	for (int y = -1; y <= 1; y++) {\n"
									"		for (int x = -1; x <= 1; x++) {\n"
									"			vec2 s = uv + vec2(float(x), float(y)) * texelSize;\n"
									"			float d = texture(uTex2, s).r;\n"
									"			if (d >= 0.99999 || d >= bestDepth) continue;\n"
									"			bestDepth = d;\n"
									"			best = texture(uTex1, s).rg;\n"
									"			found = true;\n"
									"		}\n"
									"	}\n"
									"	if (found) {\n"
									"		velocity = MeshVelocity(best);\n"
									"	} else {\n"
									"		vec4 prev = uReproject * vec4(UVToNDC(uv), 1.0, 1.0);\n"
									"		vec2 prevUV = (prev.w > 0.0) ? NDCToUV(prev.xy / prev.w) : uv;\n"
									"		velocity = uv - prevUV;\n"
									"	}\n"
									"}\n"
									"#if defined(VULKAN)\n"
									"velocity *= uVelocityScale.x;\n"
									"#else\n"
									"velocity *= uVelocityScale;\n"
									"#endif\n"
									// Bound a bad frame (a hitch, a teleported
									// matrix) so it cannot paint the whole picture.
									// The cap is in pixels of this frame, so a fast
									// editor and a 60fps one keep the same shutter.
									"float maxPixels = 0.12 * float(max(textureSize(uTex0, 0).x, textureSize(uTex0, 0).y));\n"
									"float speed = length(velocity / texelSize);\n"
									"if (speed > maxPixels) {\n"
									"	velocity *= maxPixels / speed;\n"
									"	speed = maxPixels;\n"
									"}\n"
									"int nSamples = int(clamp(speed, 1.0, float(MAX_SAMPLES)));\n"
									// Along the whole segment, centred. A fixed
									// grid of taps on a long streak shows up as
									// copies of the edge, so each pixel shifts
									// the grid by a fraction of one tap.
									"float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
									"vec4 acc = vec4(0.0);\n"
									"for (int i = 0; i < MAX_SAMPLES; ++i) {\n"
									"	if (i >= nSamples) break;\n"
									"	float t = (nSamples <= 1) ? 0.0 : ((float(i) + jitter) / float(nSamples) - 0.5);\n"
									"	acc += texture(uTex0, uv + velocity * t);\n"
									"}\n"
									"FragColor = acc / float(nSamples);\n"
								"}";

		CompileShaders();
    }

    MotionBlurEffect::~MotionBlurEffect() {
    }

    void MotionBlurEffect::UploadScale() {
	    // velocity in the map is how far the pixel moved over the last
	    // real frame. Multiply by current/target so the streak is
	    // `strength` target-frames long whatever the editor is hitting.
	    // Clamping this ratio at 2 made an uncapped view (the Vulkan
	    // present is immediate) smear by a fraction of a frame, which is
	    // a soft edge rather than a trail. A single hitch is limited by
	    // the pixel cap in the shader, not by shortening every fast frame.
	    f32 ratio = cfps / tfps;
	    if (ratio < 0.05f) ratio = 0.05f;
	    f32 v = strength * ratio;
	    velHandle->SetValue(&v);
    }

    void MotionBlurEffect::SetCurrentFPS(const f32 &currentfps) {
	    this->cfps = currentfps > 1.0f ? currentfps : 1.0f;
	    UploadScale();
    }

    void MotionBlurEffect::SetTargetFPS(const f32 &targetfps) {
	    this->tfps = targetfps > 1.0f ? targetfps : 1.0f;
	    UploadScale();
    }

    void MotionBlurEffect::SetCameraReproject(const Matrix &m) {
	    reproject = m;
	    reprojectHandle->SetValue((void*)&reproject);
    }

};
