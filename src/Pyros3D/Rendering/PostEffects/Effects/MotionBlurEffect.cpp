#include <cstdlib>
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
		// the scene's own depth (uTex3): what stood still is placed by it and the two cameras
		UseRTT(RTT::Depth);

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
		// PYROS_MB_DEBUG=1: red where the velocity pass drew something, green the streak length (40 px full)
		FragmentShaderString = std::string(std::getenv("PYROS_MB_DEBUG") ? "#define MB_DEBUG\n" : "") +
									"#define MAX_SAMPLES 48\n"
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
								"SAMPLER_BINDING(3) uniform sampler2D uTex3;\n"
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
								// Where a pixel moved over the shutter, in UV: its own velocity where the velocity
								// pass drew the surface that is seen there (its depth and the scene's agree), else
								// the point stood still in the world and the scene's depth and the two cameras say
								// where it was. (Taking everything the pass had not drawn for sky at infinity split
								// the picture in two along an edge - one half streaking, one sharp.) Bounded by a
								// share of the frame, so a hitch or a teleported matrix cannot paint the whole
								// picture; the cap is in pixels of this frame, so the shutter is the same at any rate.
								"vec2 texelSize;\n"
								"float maxPixels;\n"
								"vec2 VelocityAt(vec2 uv, out float sceneDepth) {\n"
								"	float drawnDepth = texture(uTex2, uv).r;\n"
								"	sceneDepth = texture(uTex3, uv).r;\n"
								"	vec2 velocity;\n"
								"	if (drawnDepth < 0.99999 && abs(drawnDepth - sceneDepth) <= 0.002) {\n"
								"		velocity = MeshVelocity(texture(uTex1, uv).rg);\n"
								"	} else {\n"
								"		vec4 prev = uReproject * vec4(UVToNDC(uv), min(sceneDepth, 1.0) * 2.0 - 1.0, 1.0);\n"
								"		vec2 prevUV = (prev.w > 0.0) ? NDCToUV(prev.xy / prev.w) : uv;\n"
								"		velocity = uv - prevUV;\n"
								"	}\n"
								"#if defined(VULKAN)\n"
								"	velocity *= uVelocityScale.x;\n"
								"#else\n"
								"	velocity *= uVelocityScale;\n"
								"#endif\n"
								"	float px = length(velocity / texelSize);\n"
								"	if (px > maxPixels) velocity *= maxPixels / px;\n"
								"	return velocity;\n"
								"}\n"
								// Distance from the eye, near enough, from a [0,1] depth: (1 - d) goes as near/z.
								"float EyeDepth(float d) { return 1.0 / max(1.0 - min(d, 0.999999), 1e-6); }\n"
								// How much of a pixel at distance x a streak of half-length r covers: a cone (thinner
								// toward its ends) and a cylinder (all of it, out to its end).
								"float Cone(float x, float r) { return clamp(1.0 - x / max(r, 1e-4), 0.0, 1.0); }\n"
								"float Cylinder(float x, float r) { return 1.0 - smoothstep(0.95 * r, 1.05 * r, x); }\n"
								// 1 where a is in front of b, by more than a few percent of their distance.
								"float Nearer(float a, float b) { return clamp((b - a) / (0.03 * b) , 0.0, 1.0); }\n"
								"void main() {\n"
								"texelSize = 1.0 / vec2(textureSize(uTex0, 0));\n"
								"maxPixels = 0.12 * float(max(textureSize(uTex0, 0).x, textureSize(uTex0, 0).y));\n"
								"vec2 uv = vTexcoord;\n"
								"float centreDepth;\n"
								"vec2 velocity = VelocityAt(uv, centreDepth);\n"
								"#ifdef MB_DEBUG\n"
								"FragColor = vec4(texture(uTex2, uv).r < 0.99999 ? 1.0 : 0.0, clamp(length(velocity / texelSize) / 40.0, 0.0, 1.0), 0.0, 1.0); return;\n"
								"#endif\n"
								"float speed = length(velocity / texelSize);\n"
								"vec4 centre = texture(uTex0, uv);\n"
								"if (speed < 0.5) { FragColor = centre; return; }\n"
								// Gathered along this pixel's streak, each sample weighed as a reconstruction filter
								// does (McGuire et al., 2012): a sample in front counts as far as its OWN streak
								// reaches this pixel - a gun or a car riding with the camera does not bleed into the
								// street streaking behind it; one behind counts as far as THIS pixel's streak reaches
								// - the street does not smear over the gun; two blurring together count where both
								// cover. Averaging everything along the streak alike drew a fast edge as a row of
								// copies, and a sharp foreground haloed with the background's smear.
								// The taps are offset per pixel by interleaved gradient noise - even, not grain.
								"int nSamples = int(clamp(speed * 0.75, 4.0, float(MAX_SAMPLES)));\n"
								"float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n"
								"float zc = EyeDepth(centreDepth);\n"
								"float rc = 0.5 * speed;\n"
								"float total = 1.0 / max(rc, 1.0);\n"
								"vec4 acc = centre * total;\n"
								"for (int i = 0; i < MAX_SAMPLES; ++i) {\n"
								"	if (i >= nSamples) break;\n"
								"	float t = ((float(i) + jitter) / float(nSamples)) * 2.0 - 1.0;\n"
								"	if (abs(t) < 1e-3) continue;\n"
								"	vec2 suv = uv + velocity * (0.5 * t);\n"
								"	float sd;\n"
								"	vec2 sv = VelocityAt(suv, sd);\n"
								"	float zs = EyeDepth(sd);\n"
								"	float x = abs(t) * rc;\n"
								"	float rs = 0.5 * length(sv / texelSize);\n"
								"	float f = Nearer(zs, zc);\n"
								"	float b = Nearer(zc, zs);\n"
								"	float w = f * Cone(x, rs) + b * Cone(x, rc) + Cylinder(x, rs) * Cylinder(x, rc) * 2.0;\n"
								"	acc += texture(uTex0, suv) * w;\n"
								"	total += w;\n"
								"}\n"
								"FragColor = acc / total;\n"
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
