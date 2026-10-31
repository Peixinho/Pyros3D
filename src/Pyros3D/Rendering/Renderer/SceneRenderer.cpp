//============================================================================
// Name        : SceneRenderer.cpp
// Author      : Duarte Peixinho
// Description : The renderer - see SceneRenderer.h
//============================================================================

#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Line2D.h>
#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#include <Pyros3D/Rendering/GI/ReflectionProbes.h>
#include <Pyros3D/Rendering/GI/DDGIVolume.h>
#include <Pyros3D/Rendering/GI/SceneGI.h>
#include <Pyros3D/Rendering/GI/RayScene.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#if defined(PYROS_RENDERER_GLSLANG)
#include <Pyros3D/Rendering/Renderer/GlslToSpirv.h>
#endif
#if defined(PYROS_GFX_SPIRV_TO_WGSL)
#include <Pyros3D/Rendering/Device/SpirvToWgsl.h>
#endif
#include <cstdio>
#include <sstream>
#include <Pyros3D/Rendering/ColourSpace.h>
#include <Pyros3D/Rendering/ShaderGlobals.h>
#include <chrono>
#include <set>
#include <unordered_set>
#include <cmath>
#include <cstring>

// (a browser build: each shader's WGSL beside its SPIR-V, made at build time - the page turns no SPIR-V into WGSL)
#if defined(PYROS_RENDERER_WGSL)
#define PYROS_WGSL(desc, name) desc.wgsl = name##Wgsl
#else
#define PYROS_WGSL(desc, name)
#endif

namespace p3d {

	namespace {
		// resources/shaders, compiled to SPIR-V when the engine is built (cmake/PyrosRenderer.cmake)
		const uint32_t kSurfaceVertColor[] = {
#include "shaders/surface.vert.VERTEX_COLOR.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSurfaceVertColorWgsl =
#include "shaders/surface.vert.VERTEX_COLOR.wgsl.inc"
			;
#endif
		const uint32_t kSurfaceVert[] = {
#include "shaders/surface.vert.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSurfaceVertWgsl =
#include "shaders/surface.vert.wgsl.inc"
			;
#endif
		const uint32_t kSurfaceVertInstanced[] = {
#include "shaders/surface.vert.INSTANCED.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSurfaceVertInstancedWgsl =
#include "shaders/surface.vert.INSTANCED.wgsl.inc"
			;
#endif
		const uint32_t kDepthVertInstanced[] = {
#include "shaders/depth.vert.INSTANCED.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kDepthVertInstancedWgsl =
#include "shaders/depth.vert.INSTANCED.wgsl.inc"
			;
#endif
		const uint32_t kShadowVertInstanced[] = {
#include "shaders/shadow.vert.INSTANCED.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kShadowVertInstancedWgsl =
#include "shaders/shadow.vert.INSTANCED.wgsl.inc"
			;
#endif
		const uint32_t kShadowVertAlpha[] = {
#include "shaders/shadow.vert.ALPHA.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kShadowVertAlphaWgsl =
#include "shaders/shadow.vert.ALPHA.wgsl.inc"
			;
#endif
		const uint32_t kShadowVertInstancedAlpha[] = {
#include "shaders/shadow.vert.INSTANCED.ALPHA.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kShadowVertInstancedAlphaWgsl =
#include "shaders/shadow.vert.INSTANCED.ALPHA.wgsl.inc"
			;
#endif
		const uint32_t kDepthVertAlpha[] = {
#include "shaders/depth.vert.ALPHA.inc"
		};
		const uint32_t kDepthVertInstancedAlpha[] = {
#include "shaders/depth.vert.INSTANCED.ALPHA.inc"
		};
		const uint32_t kDepthAlphaFrag[] = {
#include "shaders/depthalpha.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kDepthVertAlphaWgsl =
#include "shaders/depth.vert.ALPHA.wgsl.inc"
			;
		const char* const kDepthVertInstancedAlphaWgsl =
#include "shaders/depth.vert.INSTANCED.ALPHA.wgsl.inc"
			;
		const char* const kDepthAlphaFragWgsl =
#include "shaders/depthalpha.frag.wgsl.inc"
			;
#endif
		const uint32_t kAlphaCutFrag[] = {
#include "shaders/alphacut.frag.inc"
		};
		const uint32_t kSurfaceFragNoCut[] = {
#include "shaders/surface.frag.NOCUT.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kAlphaCutFragWgsl =
#include "shaders/alphacut.frag.wgsl.inc"
			;
		const char* const kSurfaceFragNoCutWgsl =
#include "shaders/surface.frag.NOCUT.wgsl.inc"
			;
#endif
		const uint32_t kShadowAlphaFrag[] = {
#include "shaders/shadowalpha.frag.inc"
		};
		const uint32_t kSurfaceVertSkinned[] = {
#include "shaders/surface.vert.SKINNED.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSurfaceVertSkinnedWgsl =
#include "shaders/surface.vert.SKINNED.wgsl.inc"
			;
#endif
		const uint32_t kDepthVertSkinned[] = {
#include "shaders/depth.vert.SKINNED.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kDepthVertSkinnedWgsl =
#include "shaders/depth.vert.SKINNED.wgsl.inc"
			;
#endif
		const uint32_t kShadowVertSkinned[] = {
#include "shaders/shadow.vert.SKINNED.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kShadowVertSkinnedWgsl =
#include "shaders/shadow.vert.SKINNED.wgsl.inc"
			;
#endif
		const uint32_t kShadowVertSkinnedAlpha[] = {
#include "shaders/shadow.vert.SKINNED.ALPHA.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kShadowVertSkinnedAlphaWgsl =
#include "shaders/shadow.vert.SKINNED.ALPHA.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kShadowAlphaFragWgsl =
#include "shaders/shadowalpha.frag.wgsl.inc"
			;
#endif
		const uint32_t kParticleVert[] = {
#include "shaders/particle.vert.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kParticleVertWgsl =
#include "shaders/particle.vert.wgsl.inc"
			;
#endif
		const uint32_t kParticleFrag[] = {
#include "shaders/particle.frag.inc"
		};
		const uint32_t kParticleFragHalf[] = {
#include "shaders/particle.frag.HALF.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kParticleFragHalfWgsl =
#include "shaders/particle.frag.HALF.wgsl.inc"
			;
#endif
		const uint32_t kOverlayVert[] = {
#include "shaders/overlay.vert.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kOverlayVertWgsl =
#include "shaders/overlay.vert.wgsl.inc"
			;
#endif
		const uint32_t kOverlayFrag[] = {
#include "shaders/overlay.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kOverlayFragWgsl =
#include "shaders/overlay.frag.wgsl.inc"
			;
#endif
		const uint32_t kSkyboxTableFrag[] = {
#include "shaders/skyboxtable.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSkyboxTableFragWgsl =
#include "shaders/skyboxtable.frag.wgsl.inc"
			;
#endif
		const uint32_t kSkyboxFrag[] = {
#include "shaders/skybox.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSkyboxFragWgsl =
#include "shaders/skybox.frag.wgsl.inc"
			;
#endif
		const uint32_t kParticleCompositeFrag[] = {
#include "shaders/particlecomposite.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kParticleCompositeFragWgsl =
#include "shaders/particlecomposite.frag.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kParticleFragWgsl =
#include "shaders/particle.frag.wgsl.inc"
			;
#endif
		const uint32_t kUIVert[] = {
#include "shaders/ui.vert.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kUIVertWgsl =
#include "shaders/ui.vert.wgsl.inc"
			;
#endif
		const uint32_t kUIFrag[] = {
#include "shaders/ui.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kUIFragWgsl =
#include "shaders/ui.frag.wgsl.inc"
			;
#endif
		struct UIDrawData { f32 transform[16]; f32 colour[4]; f32 flags[4]; };
		// the renderer's shader sources as text, for the surfaces compiled at run time (cmake/EmbedShaderSources.cmake)
		struct EmbeddedSource { const char* name; const char* text; };
		const EmbeddedSource kSources[] = {
#include "shaders/sources.inc"
		};
		// A source with its #include "x" lines replaced by the embedded files (once each)
		std::string Resolve(const std::string &name, std::set<std::string> &done)
		{
			if (!done.insert(name).second) return std::string();
			std::string text;
			for (const EmbeddedSource &e : kSources) if (name == e.name) text = e.text;
			std::istringstream in(text);
			std::string line, out;
			while (std::getline(in, line))
			{
				const size_t inc = line.find("#include \"");
				if (inc != std::string::npos && line.find_first_not_of(" \t") == inc)
				{
					const size_t a = inc + 10, b = line.find('"', a);
					out += Resolve(line.substr(a, b - a), done);
				}
				else out += line + "\n";
			}
			return out;
		}
		const uint32 kMaterialParamsSlot = 256u << 10;     // (a frame's custom materials' parameters)
		const uint64 kParticleRingStart = 2u << 20;     // (a frame's particles to begin with: 65,536 of 32 bytes - grown to fit)
		const uint32_t kSurfaceFrag[] = {
#include "shaders/surface.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSurfaceFragWgsl =
#include "shaders/surface.frag.wgsl.inc"
			;
#endif
		const uint32_t kFullscreenVert[] = {
#include "shaders/fullscreen.vert.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kFullscreenVertWgsl =
#include "shaders/fullscreen.vert.wgsl.inc"
			;
#endif
		const uint32_t kTonemapFrag[] = {
#include "shaders/tonemap.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kTonemapFragWgsl =
#include "shaders/tonemap.frag.wgsl.inc"
			;
#endif
		const uint32_t kShadowVert[] = {
#include "shaders/shadow.vert.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kShadowVertWgsl =
#include "shaders/shadow.vert.wgsl.inc"
			;
#endif
		const uint32_t kDepthOnlyFrag[] = {
#include "shaders/depthonly.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kDepthOnlyFragWgsl =
#include "shaders/depthonly.frag.wgsl.inc"
			;
#endif
		const uint32_t kTransmittanceFrag[] = {
#include "shaders/transmittance.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kTransmittanceFragWgsl =
#include "shaders/transmittance.frag.wgsl.inc"
			;
#endif
		const uint32_t kSkyViewFrag[] = {
#include "shaders/skyview.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSkyViewFragWgsl =
#include "shaders/skyview.frag.wgsl.inc"
			;
#endif
		const uint32_t kSkyFrag[] = {
#include "shaders/sky.frag.inc"
		};
		const uint32_t kCloudsFrag[] = {
#include "shaders/clouds.frag.inc"
		};
		const uint32_t kCloudsDomeFrag[] = {
#include "shaders/clouds.frag.DOME.inc"
		};
		const uint32_t kCloudShadowFrag[] = {
#include "shaders/cloudshadow.frag.inc"
		};
		const uint32_t kParticleSimComp[] = {
#include "shaders/particle_sim.comp.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kParticleSimCompWgsl =
#include "shaders/particle_sim.comp.wgsl.inc"
			;
#endif
		const uint32_t kRtShadowFrag[] = {
#include "shaders/rtshadow.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kRtShadowFragWgsl =
#include "shaders/rtshadow.frag.wgsl.inc"
			;
#endif
		const uint32_t kRtReflectFrag[] = {
#include "shaders/rtreflect.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kRtReflectFragWgsl =
#include "shaders/rtreflect.frag.wgsl.inc"
			;
#endif
#if !defined(__EMSCRIPTEN__)
		const uint32_t kRtShadowRQFrag[] = {
#include "shaders/rtshadow.frag.RAYQUERY.inc"
		};
		const uint32_t kRtReflectRQFrag[] = {
#include "shaders/rtreflect.frag.RAYQUERY.inc"
		};
#endif
		const uint32_t kPostFrag[] = {
#include "shaders/post.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kPostFragWgsl =
#include "shaders/post.frag.wgsl.inc"
			;
#endif
		const uint32_t kTaaFrag[] = {
#include "shaders/taa.frag.inc"
		};
		const uint32_t kVelocityFrag[] = {
#include "shaders/velocity.frag.inc"
		};
		const uint32_t kNoVelocityFrag[] = {
#include "shaders/novelocity.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kNoVelocityFragWgsl =
#include "shaders/novelocity.frag.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kVelocityFragWgsl =
#include "shaders/velocity.frag.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kTaaFragWgsl =
#include "shaders/taa.frag.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kCloudShadowFragWgsl =
#include "shaders/cloudshadow.frag.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kCloudsDomeFragWgsl =
#include "shaders/clouds.frag.DOME.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kCloudsFragWgsl =
#include "shaders/clouds.frag.wgsl.inc"
			;
#endif
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSkyFragWgsl =
#include "shaders/sky.frag.wgsl.inc"
			;
#endif
		const uint32_t kSkyShFrag[] = {
#include "shaders/skysh.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSkyShFragWgsl =
#include "shaders/skysh.frag.wgsl.inc"
			;
#endif
		const uint32_t kDepthVert[] = {
#include "shaders/depth.vert.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kDepthVertWgsl =
#include "shaders/depth.vert.wgsl.inc"
			;
#endif
		const uint32_t kGtaoFrag[] = {
#include "shaders/gtao.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kGtaoFragWgsl =
#include "shaders/gtao.frag.wgsl.inc"
			;
#endif
		const uint32_t kAoBlurFrag[] = {
#include "shaders/aoblur.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kAoBlurFragWgsl =
#include "shaders/aoblur.frag.wgsl.inc"
			;
#endif
		const uint32_t kSkyShSumFrag[] = {
#include "shaders/skyshsum.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kSkyShSumFragWgsl =
#include "shaders/skyshsum.frag.wgsl.inc"
			;
#endif
		const uint32_t kProbeFilterFrag[] = {
#include "shaders/probefilter.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kProbeFilterFragWgsl =
#include "shaders/probefilter.frag.wgsl.inc"
			;
#endif
		const uint32_t kBloomDownFrag[] = {
#include "shaders/bloomdown.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kBloomDownFragWgsl =
#include "shaders/bloomdown.frag.wgsl.inc"
			;
#endif
		const uint32_t kBloomUpFrag[] = {
#include "shaders/bloomup.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kBloomUpFragWgsl =
#include "shaders/bloomup.frag.wgsl.inc"
			;
#endif
		const uint32_t kLuminanceFrag[] = {
#include "shaders/luminance.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kLuminanceFragWgsl =
#include "shaders/luminance.frag.wgsl.inc"
			;
#endif
		const uint32_t kReduce4Frag[] = {
#include "shaders/reduce4.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kReduce4FragWgsl =
#include "shaders/reduce4.frag.wgsl.inc"
			;
#endif
		const uint32_t kAdaptFrag[] = {
#include "shaders/adapt.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kAdaptFragWgsl =
#include "shaders/adapt.frag.wgsl.inc"
			;
#endif

		const uint32 kFramesInFlight = 3;
		const uint32 kFrameDataSlot = 1024;       // (a uniform buffer's dynamic offset: 256-byte aligned everywhere)
		const gfx::Format kHdrFormat = gfx::Format::RGBA16F, kDepthFormat = gfx::Format::D32F;
		const uint8 kSceneSamples = 4;            // (MSAA: geometric edges, the scene pass's own)

		// std140, as common.glsl's FrameData
		struct FrameData
		{
			f32 viewProjection[16];
			f32 cameraPosition[4];
			f32 sunDirection[4];
			f32 sunRadiance[4];
			f32 ambient[4];
			f32 frame[4];
			f32 sky[4];
			f32 projection[4];
			f32 viewport[4];
			f32 view[16];
			f32 globals[16][4];
			f32 ambientSky[4], ambientEquator[4], ambientGround[4];     // (w of the first: the mode - 1 gradient, 2 SH)
			f32 ambientSH[9][4];
			f32 weather[4];
			f32 previousViewProjection[16];     // (last frame's, without its jitter: where a point was in last frame's picture)
		};
		// std140, as surface_draw.glsl's DrawData
		struct DrawData
		{
			f32 model[16];
			f32 normalRows[12];
			f32 baseColor[4];
			f32 surface[4];
			f32 emission[4];
			f32 uvTransform[4];
			f32 skin[4];
			f32 wind[4];
			f32 wet[4];
			f32 subsurface[4];
			f32 shape[4];
		};
		// std430, as lighting.glsl's Light
		struct LightData
		{
			f32 positionRange[4];
			f32 radianceType[4];
			f32 directionCosOuter[4];
			f32 cosInner[4];
		};
		const uint32 kMaxLights = 1024;
		const uint32 kMaxBones = 8192;          // (a frame's skinned draws' bones, at most)
		const uint32 kLightDataSlot = (16 + kMaxLights * sizeof(LightData) + 255) / 256 * 256;     // (rounded up to 256: a dynamic offset)
		static_assert(kLightDataSlot % 256 == 0, "a light slot must be 256-byte aligned");

		// The sun's shadow: four cascades, a 2048 quadrant each of a 4096 atlas
		// (the shadow atlas: the sun's cascades in its left half, a quadrant each; the point and spot lights' tiles in its right)
		const uint32 kCascades = 4, kShadowAtlasW = 8192, kShadowAtlasH = 4096, kCascadeSize = kShadowAtlasH / 2;
		const uint32 kSoftwareRayTriangles = 1500000;     // (the most the shaders' own BVH walk is given: past it, the shadow maps and SSR)
		const uint32 kLocalTile = 512, kLocalTiles = 64, kLocalTilesAcross = (kShadowAtlasW / 2) / kLocalTile;
		const uint32 kShadowSlots = kCascades + kLocalTiles;     // (each a 256-byte slot of the shadow pass's matrices)
		const uint32 kShadowDataSlot = 6144;
		const gfx::Format kShadowFormat = gfx::Format::D16;     // (an orthographic depth: linear, 16 bits are plenty - half the bandwidth)
		struct ShadowData        // std140, as shadows.glsl's
		{
			f32 cascades[4][16];
			f32 splits[4];
			f32 params[4];
			f32 texelWorld[4];
			f32 cameraForward[4];
			f32 atlas[4];
			f32 localRect[64][4];
			f32 local[64][16];
			f32 cloudGrid[4];
			f32 reflections[4];
			f32 rain[4];
		};
		static_assert(sizeof(ShadowData) <= kShadowDataSlot, "ShadowData must fit its slot");
		struct ShadowDrawData { f32 model[16]; f32 uvTransform[4]; f32 alpha[4]; f32 skin[4]; f32 wind[4]; f32 shape[4]; };     // (as shadow.vert's)
		// (a half float's bits, of a float in its range: an instanced set's fades, two to a word - instancing.glsl)
		static uint32_t HalfBits(const f32 f)
		{
			uint32_t x; memcpy(&x, &f, 4);
			const uint32_t sign = (x >> 16) & 0x8000u;
			const int32_t e = (int32_t)((x >> 23) & 0xFF) - 127 + 15;
			if (e <= 0) return sign;
			if (e >= 31) return sign | 0x7BFFu;
			return sign | ((uint32_t)e << 10) | ((x >> 13) & 0x3FFu);
		}
		static f32 PackHalves(const f32 a, const f32 b) { const uint32_t bits = HalfBits(a) | (HalfBits(b) << 16); f32 f; memcpy(&f, &bits, 4); return f; }
		// (a plant's sway, as the material says: the same in every pass - and only what sways alike is one instanced draw)
		static Vec4 WindOf(const GenericShaderMaterial* m) { return m ? m->GetWind() : Vec4(0.f, 0.f, 0.f, 0.f); }
		static bool SameWind(const GenericShaderMaterial* a, const GenericShaderMaterial* b)
		{
			const Vec4 x = WindOf(a), y = WindOf(b);
			return x.x == y.x && x.y == y.y && x.z == y.z && x.w == y.w;
		}

		// A sphere in the world: a local one moved and scaled by a transform
		void WorldSphere(const Vec3 &localCenter, const f32 localRadius, const Matrix &model, Vec3 &center, f32 &radius)
		{
			center = model * localCenter;
			const f32 sx = Vec3(model.m[0], model.m[1], model.m[2]).magnitude(), sy = Vec3(model.m[4], model.m[5], model.m[6]).magnitude(), sz = Vec3(model.m[8], model.m[9], model.m[10]).magnitude();
			radius = localRadius * std::max(sx, std::max(sy, sz));
		}

		// sRGB <-> linear for 8-bit mips: averaged in light, not in the encoding
		f32 SrgbByteToLinear(const uchar c) { static f32 lut[256]; static bool made = false; if (!made) { for (int i = 0; i < 256; i++) lut[i] = SrgbToLinear(i / 255.f); made = true; } return lut[c]; }
		uchar LinearToSrgbByte(const f32 v) { const f32 c = v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.f / 2.4f) - 0.055f; return (uchar)std::max(0.f, std::min(255.f, c * 255.f + 0.5f)); }

		// The next mip of an RGBA8 image: each texel the mean of the 2x2 under it (edges clamped)
		std::vector<uchar> HalveRGBA8(const std::vector<uchar> &src, const uint32 w, const uint32 h, const bool srgb, uint32 &nw, uint32 &nh)
		{
			nw = std::max(1u, w / 2); nh = std::max(1u, h / 2);
			std::vector<uchar> dst((size_t)nw * nh * 4);
			for (uint32 y = 0; y < nh; y++)
				for (uint32 x = 0; x < nw; x++)
				{
					const uint32 x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1), y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
					const uchar* p[4] = { &src[((size_t)y0 * w + x0) * 4], &src[((size_t)y0 * w + x1) * 4], &src[((size_t)y1 * w + x0) * 4], &src[((size_t)y1 * w + x1) * 4] };
					uchar* o = &dst[((size_t)y * nw + x) * 4];
					for (int c = 0; c < 4; c++)
					{
						if (srgb && c < 3) o[c] = LinearToSrgbByte((SrgbByteToLinear(p[0][c]) + SrgbByteToLinear(p[1][c]) + SrgbByteToLinear(p[2][c]) + SrgbByteToLinear(p[3][c])) * 0.25f);
						else o[c] = (uchar)((p[0][c] + p[1][c] + p[2][c] + p[3][c] + 2) / 4);
					}
				}
			return dst;
		}
		static_assert(sizeof(FrameData) <= kFrameDataSlot, "FrameData must fit its slot");
		static_assert(sizeof(DrawData) <= 256, "DrawData is at most 256 bytes");

		// Interleaved as the surface pipeline reads it: position, normal, texcoord
		struct Vertex { f32 p[3], n[3], uv[2]; };
		// An instance, as the instanced pipelines read their second stream: the matrix's columns, then the tint
		struct Instance { f32 model[16]; f32 tint[4]; };
		// (the per-instance stream's layout, shared by every instanced pipeline)
		void AddInstanceStream(gfx::GraphicsPipelineDesc &p)
		{
			p.bindings.push_back({ 1, (uint16_t)sizeof(Instance), true });
			for (uint8 c = 0; c < 4; c++) p.attributes.push_back({ (uint8)(3 + c), 1, gfx::Format::RGBA32F, (uint16_t)(c * 16) });
			p.attributes.push_back({ 7, 1, gfx::Format::RGBA32F, 64 });
		}

		// Reversed Z, infinite far: depth 1 at the near plane, toward 0 far away; clip z in 0..1
		Matrix ReversedInfinitePerspective(const f32 fovYDegrees, const f32 aspect, const f32 nearZ)
		{
			const f32 f = 1.f / std::tan(fovYDegrees * (f32)PI / 360.f);
			Matrix m;
			for (int i = 0; i < 16; i++) m.m[i] = 0.f;
			m.m[0] = f / aspect;
			m.m[5] = f;
			m.m[11] = -1.f;
			m.m[14] = nearZ;
			return m;
		}

		// (reversed Z: depth 1 at nearZ, 0 at farZ; x and y by the half extents)
		Matrix ReversedOrthographic(const f32 halfWidth, const f32 halfHeight, const f32 nearZ, const f32 farZ)
		{
			Matrix m;
			for (int i = 0; i < 16; i++) m.m[i] = 0.f;
			m.m[0] = 1.f / halfWidth;
			m.m[5] = 1.f / halfHeight;
			m.m[10] = 1.f / (farZ - nearZ);
			m.m[14] = farZ / (farZ - nearZ);
			m.m[15] = 1.f;
			return m;
		}
		Matrix ProjectionFor(const View &view)
		{
			const f32 aspect = view.height ? (f32)view.width / (f32)view.height : 1.f;
			if (view.orthographic) return ReversedOrthographic(view.orthoHeight * 0.5f * aspect, view.orthoHeight * 0.5f, view.nearZ, std::max(view.farZ, view.nearZ + 0.01f));
			return ReversedInfinitePerspective(view.fovY, aspect, view.nearZ);
		}

		const VertexAttribute* FindAttribute(IGeometry* g, const char* name)
		{
			for (AttributeArray* a : g->Attributes)
				if (a)
					for (VertexAttribute* v : a->Attributes)
						if (v && v->Name == name) return v;
			return nullptr;
		}
	}

	namespace
	{
		// Drawn blended: transparent, or blending on with factors that blend (the old renderer's rule - IRenderer's
		// material state: a sprite with an alpha-cut texture is blending, not transparent)
		bool Blended(const IMaterial* m)
		{
			return m->IsTransparent() || (m->IsBlendingEnabled() && !(m->GetBlendingSFactor() == BlendFunc::Zero && m->GetBlendingDFactor() == BlendFunc::Zero));
		}
	}
	namespace { std::vector<SceneRenderer*>& LiveRenderers() { static std::vector<SceneRenderer*> live; return live; } }
	const std::vector<SceneRenderer*>& SceneRenderer::Live() { return LiveRenderers(); }

	std::vector<SceneRenderer::DebugTarget> SceneRenderer::DebugTargets() const
	{
		std::vector<DebugTarget> t;
		const uint32 w = targetWidth, h = targetHeight, hw = std::max(1u, w / 2), hh = std::max(1u, h / 2);
		auto add = [&](const char* n, const gfx::Texture x, uint32 tw, uint32 th) { if (x) t.push_back({ n, x, tw, th }); };
		add("HDR picture", hdr, w, h); add("Last frame (reflections)", hdrPrevious, w, h);
		add("Ambient occlusion", aoA, hw, hh); add("Motion (velocity)", velocityTex, w, h);
		const uint32 ow = outputWidth, oh = outputHeight;     // (the resolved picture's: more than the scene's with a render scale)
		add("Resolved picture", picture == hdr ? gfx::Texture() : picture, ow, oh);
		add("Bloom", bloom[0], std::max(1u, ow / 2), std::max(1u, oh / 2)); add("Half-size particles", particleTex, hw, hh);
		add("Temporal history A", taaTex[0], ow, oh); add("Temporal history B", taaTex[1], ow, oh);
		add("Sky view", skyView, 192, 108); add("Sky view with clouds", skyViewClouded, 192, 108); add("Transmittance", transmittance, 256, 64);
		add("Reflection probes", probeAtlas, kProbeColumns * kProbeBlockWidth, kProbeRows * kProbeBlockHeight);
		add("Clouds A", cloudTex[0], hw, hh); add("Clouds B", cloudTex[1], hw, hh); add("Clouds' shadow", cloudShadowTex, 256, 256);
		return t;
	}

	SceneRenderer::SceneRenderer(gfx::Device* d) : device(d)
	{
		LiveRenderers().push_back(this);
		CreateShaders();
		if (const char* rs = getenv("PYROS_RENDER_SCALE")) SetRenderScale((f32)atof(rs));     // (to try one without the settings)

		gfx::BufferDesc fd; fd.size = kFrameDataSlot * kFramesInFlight; fd.usage = gfx::BufUniform; fd.memory = gfx::Memory::Upload; fd.debugName = "Renderer.FrameData";
		frameData = device->CreateBuffer(fd);

		gfx::BufferDesc ld; ld.size = (uint64)kLightDataSlot * kFramesInFlight; ld.usage = gfx::BufStorage; ld.memory = gfx::Memory::Upload; ld.debugName = "Renderer.Lights";
		lightData = device->CreateBuffer(ld);

		gfx::SamplerDesc sd; sd.u = sd.v = sd.w = gfx::Address::Clamp; sd.anisotropy = 1.f;
		linearClamp = device->CreateSampler(sd);
		gfx::SamplerDesc md; md.anisotropy = 8.f;
		materialSampler = device->CreateSampler(md);
		gfx::SamplerDesc cd; cd.u = cd.v = cd.w = gfx::Address::Clamp; cd.anisotropy = 1.f; cd.mip = gfx::Filter::Nearest; cd.compare = true;
		shadowCompare = device->CreateSampler(cd);
		CreateDefaults();

		gfx::BufferDesc cb; cb.size = 256 * kShadowSlots * kFramesInFlight; cb.usage = gfx::BufUniform; cb.memory = gfx::Memory::Upload; cb.debugName = "Renderer.Cascades";
		cascadeData = device->CreateBuffer(cb);
		{
			gfx::BufferDesc bp; bp.size = (uint64)kMaxBones * 64 * kFramesInFlight; bp.usage = gfx::BufStorage; bp.memory = gfx::Memory::Upload; bp.debugName = "Renderer.Bones";
			bonePalette = device->CreateBuffer(bp);
		}
		gfx::BufferDesc sb; sb.size = kShadowDataSlot * kFramesInFlight; sb.usage = gfx::BufUniform; sb.memory = gfx::Memory::Upload; sb.debugName = "Renderer.ShadowData";
		shadowData = device->CreateBuffer(sb);
		gfx::TextureDesc at; at.width = kShadowAtlasW; at.height = kShadowAtlasH; at.format = kShadowFormat; at.usage = gfx::TexDepthTarget | gfx::TexSampled; at.debugName = "Renderer.SunShadow";
		shadowAtlas = device->CreateTexture(at);

		gfx::GraphicsPipelineDesc sp;
		sp.vertex = shadowVS; sp.fragment = depthOnlyFS;
		const gfx::BindingDesc shadowBones = { 3, gfx::BindingKind::StorageRead, true, false };     // (the skinned's bones; every shadow group has it)
		sp.material = { { 0, gfx::BindingKind::Uniform, true, false, false, true }, shadowBones };
		sp.drawDataBytes = sizeof(ShadowDrawData);
		sp.bindings = { { 0, (uint16_t)sizeof(Vertex), false } };
		sp.attributes = { { 0, 0, gfx::Format::RGB32F, 0 } };
		sp.cull = gfx::Cull::None;            // (both faces: thin and open meshes cast too; the bias is in the pipeline)
		sp.depthTest = sp.depthWrite = true; sp.depthCompare = gfx::Compare::GreaterEqual;
		// (reversed Z: the light's side is the larger depth - a caster is pushed the other way, away from it,
		// so a lit surface's own depth stays at or above what the map holds)
		sp.depthBiasConstant = -2.f; sp.depthBiasSlope = -2.5f;
		sp.colorCount = 0; sp.depthFormat = kShadowFormat;
		sp.debugName = "Renderer.SunShadow";
		shadowPipeline = device->CreatePipeline(sp);
		{
			// (skinned casters: their bones' stream too, at vertex binding 2)
			gfx::GraphicsPipelineDesc ss = sp;
			ss.vertex = shadowSkinVS; ss.bindings.push_back({ 2, 32, false });
			ss.attributes.push_back({ 8, 2, gfx::Format::RGBA32F, 0 }); ss.attributes.push_back({ 9, 2, gfx::Format::RGBA32F, 16 });
			ss.debugName = "Renderer.SunShadow (skinned)";
			shadowSkinPipeline = device->CreatePipeline(ss);
		}
		sp.vertex = shadowInstVS; AddInstanceStream(sp); sp.debugName = "Renderer.SunShadow (instanced)";
		shadowInstPipeline = device->CreatePipeline(sp);
		// (alpha-cut casters: the texcoord in too, and the base colour map and its sampler for the fragment stage)
		sp.vertex = shadowAlphaVS; sp.fragment = shadowAlphaFS;
		sp.material = { { 0, gfx::BindingKind::Uniform, true, false, false, true }, { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true }, shadowBones };
		sp.bindings = { { 0, (uint16_t)sizeof(Vertex), false } };
		sp.attributes = { { 0, 0, gfx::Format::RGB32F, 0 }, { 2, 0, gfx::Format::RG32F, 24 } };
		sp.debugName = "Renderer.SunShadow (alpha)";
		shadowAlphaPipeline = device->CreatePipeline(sp);
		{
			gfx::GraphicsPipelineDesc ss = sp;
			ss.vertex = shadowSkinAlphaVS; ss.bindings.push_back({ 2, 32, false });
			ss.attributes.push_back({ 8, 2, gfx::Format::RGBA32F, 0 }); ss.attributes.push_back({ 9, 2, gfx::Format::RGBA32F, 16 });
			ss.debugName = "Renderer.SunShadow (skinned, alpha)";
			shadowSkinAlphaPipeline = device->CreatePipeline(ss);
		}
		sp.vertex = shadowInstAlphaVS; AddInstanceStream(sp); sp.debugName = "Renderer.SunShadow (instanced, alpha)";
		shadowInstAlphaPipeline = device->CreatePipeline(sp);
		CreateSky();
		CreateClouds();
		CreateAO();
		CreatePost();
		CreateParticles();
		{
			gfx::GraphicsPipelineDesc u;
			u.vertex = uiVS; u.fragment = uiFS;
			u.material = { { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true } };
			u.drawDataBytes = sizeof(UIDrawData);
			u.bindings = { { 0, (uint16_t)sizeof(Vertex), false }, { 1, 16, false } };
			u.attributes = { { 0, 0, gfx::Format::RGB32F, 0 }, { 1, 0, gfx::Format::RGB32F, 12 }, { 2, 0, gfx::Format::RG32F, 24 }, { 3, 1, gfx::Format::RGBA32F, 0 } };
			u.cull = gfx::Cull::None; u.depthTest = u.depthWrite = false;
			u.colorCount = 1; u.colorFormats[0] = gfx::Format::Unknown; u.blend[0] = gfx::Blend::Alpha;     // (the backbuffer's own)
			u.debugName = "Renderer.UI";
			uiPipeline = device->CreatePipeline(u);
			u.colorFormats[0] = kHdrFormat; u.depthFormat = kDepthFormat; u.samples = kSceneSamples;
			u.depthTest = true; u.depthWrite = false; u.depthCompare = gfx::Compare::GreaterEqual;
			u.debugName = "Renderer.WorldText";
			worldTextPipeline = device->CreatePipeline(u);
		}
		gfx::BufferDesc mp; mp.size = (uint64)kMaterialParamsSlot * kFramesInFlight; mp.usage = gfx::BufUniform; mp.memory = gfx::Memory::Upload; mp.debugName = "Renderer.MaterialParams";
		materialParams = device->CreateBuffer(mp);

		gfx::GraphicsPipelineDesc tp;
		tp.vertex = fullscreenVS; tp.fragment = tonemapFS;
		tp.material = { { 0, gfx::BindingKind::Uniform, true, true, false, true }, { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true },
			{ 3, gfx::BindingKind::Texture2D, false, true }, { 4, gfx::BindingKind::Texture2D, false, true }, { 5, gfx::BindingKind::DepthTexture, false, true } };
		tp.cull = gfx::Cull::None; tp.depthTest = tp.depthWrite = false;
		tp.colorCount = 1; tp.colorFormats[0] = gfx::Format::Unknown;     // (the backbuffer's own)
		tp.debugName = "Renderer.Tonemap";
		tonemapPipeline = device->CreatePipeline(tp);
	}

	SceneRenderer::~SceneRenderer()
	{
		{ std::vector<SceneRenderer*> &live = LiveRenderers(); live.erase(std::remove(live.begin(), live.end(), this), live.end()); }
		probeRenderer.reset();
		for (gfx::Texture t : { giIrradiance, giVisibility, giProbes, giRadiance }) if (t) device->Destroy(t);
		if (probeFilterGroup) device->Destroy(probeFilterGroup);
		for (gfx::Texture t : { probeAtlas, probeFaceLdr, probeFaces[0], probeFaces[1], probeFaces[2], probeFaces[3], probeFaces[4], probeFaces[5] }) if (t) device->Destroy(t);
		for (std::unordered_map<IGeometry*, MeshGpu>::iterator i = meshes.begin(); i != meshes.end(); ++i)
		{
			device->Destroy(i->second.vertices);
			device->Destroy(i->second.indices);
			if (i->second.colors) device->Destroy(i->second.colors);
			if (i->second.skin) device->Destroy(i->second.skin);
		}
		for (std::map<std::array<uint32_t, 5>, gfx::BindGroup>::iterator i = surfaceGroups.begin(); i != surfaceGroups.end(); ++i) device->Destroy(i->second);
		for (std::map<std::pair<Texture*, bool>, TextureGpu>::iterator i = textures.begin(); i != textures.end(); ++i) if (i->second.texture) device->Destroy(i->second.texture);
		device->Destroy(white); device->Destroy(flatNormal);
		for (std::map<RenderingInstancedComponent*, InstancesGpu>::iterator i = instanceBuffers.begin(); i != instanceBuffers.end(); ++i) if (i->second.buffer) device->Destroy(i->second.buffer);
		device->Destroy(lightData);
		device->Destroy(cascadeData); device->Destroy(shadowData); device->Destroy(shadowAtlas);
		if (shadowGroup) device->Destroy(shadowGroup);
		for (std::map<uint32_t, gfx::BindGroup>::iterator i = shadowAlphaGroups.begin(); i != shadowAlphaGroups.end(); ++i) device->Destroy(i->second);
		for (std::map<uint32_t, gfx::BindGroup>::iterator i = depthAlphaGroups.begin(); i != depthAlphaGroups.end(); ++i) device->Destroy(i->second);
		for (std::map<uint32_t, gfx::BindGroup>::iterator i = particleGroups.begin(); i != particleGroups.end(); ++i) device->Destroy(i->second);
		for (std::map<uint32_t, gfx::BindGroup>::iterator i = uiGroups.begin(); i != uiGroups.end(); ++i) device->Destroy(i->second);
		if (batchRing) device->Destroy(batchRing);
		device->Destroy(particleRing);
		for (std::unordered_map<ParticleSystem*, GpuParticles>::iterator i = gpuParticles.begin(); i != gpuParticles.end(); ++i) ForgetGpuParticles(i->second);
		gpuParticles.clear();
		ForgetRayTracing();
		device->Destroy(materialParams);
		for (std::map<CustomShaderMaterial*, CustomGroup>::iterator i = customGroups.begin(); i != customGroups.end(); ++i) if (i->second.group) device->Destroy(i->second.group);
		for (gfx::BindGroup g : { transmittanceGroup, skyViewGroup, skyGroup, skyShGroup, skyShSumGroup }) if (g) device->Destroy(g);
		for (gfx::BindGroup g : { cloudsGroup[0], cloudsGroup[1], skyGroups[0], skyGroups[1] }) if (g) device->Destroy(g);
		for (gfx::Texture t : { cloudNoise, cloudTex[0], cloudTex[1] }) if (t) device->Destroy(t);
		if (cloudsDomeGroup) device->Destroy(cloudsDomeGroup);
		for (gfx::BindGroup g : taaGroups) if (g) device->Destroy(g);
		for (gfx::Texture t : taaTex) if (t) device->Destroy(t);
		if (skyboxGroup) device->Destroy(skyboxGroup);
		if (skyboxTableGroup) device->Destroy(skyboxTableGroup);
		if (overlayGroup) device->Destroy(overlayGroup);
		if (overlayRing) device->Destroy(overlayRing);
		if (skyboxTex) device->Destroy(skyboxTex);
		if (particleTex) device->Destroy(particleTex);
		if (particleCompositeGroup) device->Destroy(particleCompositeGroup);
		if (taaData) device->Destroy(taaData);
		if (bonePalette) device->Destroy(bonePalette);
		if (velocityTex) device->Destroy(velocityTex);
		if (skyViewClouded) device->Destroy(skyViewClouded);
		device->Destroy(transmittance); device->Destroy(skyView); device->Destroy(skySH); device->Destroy(skySHPartial); device->Destroy(skyData);
		for (gfx::BindGroup g : { depthGroup, gtaoGroup, aoBlurGroupH, aoBlurGroupV }) if (g) device->Destroy(g);
		for (gfx::Texture t : { prepassDepth, aoA, aoB }) if (t) device->Destroy(t);
		for (uint32 i = 0; i < kBloomLevels; i++) { if (bloom[i]) device->Destroy(bloom[i]); if (bloomDownGroup[i]) device->Destroy(bloomDownGroup[i]); if (bloomUpGroup[i]) device->Destroy(bloomUpGroup[i]); }
		for (gfx::Texture t : { lum64, lum16, lum4, lum1, exposureTex[0], exposureTex[1] }) if (t) device->Destroy(t);
		for (gfx::BindGroup g : { lumGroup, reduceGroup[0], reduceGroup[1], reduceGroup[2], adaptGroup[0], adaptGroup[1], tonemapGroups[0], tonemapGroups[1] }) if (g) device->Destroy(g);
		if (hdrPrevious) device->Destroy(hdrPrevious);
		if (hdr) device->Destroy(hdr);
		if (depth) device->Destroy(depth);
		if (msaaColor) device->Destroy(msaaColor);
		device->Destroy(frameData);
	}

	void SceneRenderer::CreateShaders()
	{
		gfx::ShaderDesc s;
		s.stage = gfx::Stage::Vertex; s.spirv = kSurfaceVert; s.spirvWords = sizeof(kSurfaceVert) / 4; PYROS_WGSL(s, kSurfaceVert); s.debugName = "surface.vert";
		surfaceVS = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment; s.spirv = kSurfaceFrag; s.spirvWords = sizeof(kSurfaceFrag) / 4; PYROS_WGSL(s, kSurfaceFrag); s.debugName = "surface.frag";
		surfaceFS = device->CreateShader(s);
		s.stage = gfx::Stage::Vertex; s.spirv = kFullscreenVert; s.spirvWords = sizeof(kFullscreenVert) / 4; PYROS_WGSL(s, kFullscreenVert); s.debugName = "fullscreen.vert";
		fullscreenVS = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment; s.spirv = kTonemapFrag; s.spirvWords = sizeof(kTonemapFrag) / 4; PYROS_WGSL(s, kTonemapFrag); s.debugName = "tonemap.frag";
		tonemapFS = device->CreateShader(s);
		s.stage = gfx::Stage::Vertex; s.spirv = kShadowVert; s.spirvWords = sizeof(kShadowVert) / 4; PYROS_WGSL(s, kShadowVert); s.debugName = "shadow.vert";
		shadowVS = device->CreateShader(s);
		s.spirv = kSurfaceVertInstanced; s.spirvWords = sizeof(kSurfaceVertInstanced) / 4; PYROS_WGSL(s, kSurfaceVertInstanced); s.debugName = "surface.vert (instanced)"; surfaceInstVS = device->CreateShader(s);
		s.spirv = kDepthVertInstanced; s.spirvWords = sizeof(kDepthVertInstanced) / 4; PYROS_WGSL(s, kDepthVertInstanced); s.debugName = "depth.vert (instanced)"; depthInstVS = device->CreateShader(s);
		s.spirv = kDepthVertAlpha; s.spirvWords = sizeof(kDepthVertAlpha) / 4; PYROS_WGSL(s, kDepthVertAlpha); s.debugName = "depth.vert (alpha)"; depthAlphaVS = device->CreateShader(s);
		s.spirv = kDepthVertInstancedAlpha; s.spirvWords = sizeof(kDepthVertInstancedAlpha) / 4; PYROS_WGSL(s, kDepthVertInstancedAlpha); s.debugName = "depth.vert (instanced, alpha)"; depthInstAlphaVS = device->CreateShader(s);
		{
			gfx::ShaderDesc f = s; f.stage = gfx::Stage::Fragment;
			f.spirv = kDepthAlphaFrag; f.spirvWords = sizeof(kDepthAlphaFrag) / 4; PYROS_WGSL(f, kDepthAlphaFrag); f.debugName = "depthalpha.frag"; depthAlphaFS = device->CreateShader(f);
			f.spirv = kAlphaCutFrag; f.spirvWords = sizeof(kAlphaCutFrag) / 4; PYROS_WGSL(f, kAlphaCutFrag); f.debugName = "alphacut.frag"; alphaCutFS = device->CreateShader(f);
			f.spirv = kSurfaceFragNoCut; f.spirvWords = sizeof(kSurfaceFragNoCut) / 4; PYROS_WGSL(f, kSurfaceFragNoCut); f.debugName = "surface.frag (no cut)"; surfaceNoCutFS = device->CreateShader(f);
		}
		s.spirv = kShadowVertInstanced; s.spirvWords = sizeof(kShadowVertInstanced) / 4; PYROS_WGSL(s, kShadowVertInstanced); s.debugName = "shadow.vert (instanced)"; shadowInstVS = device->CreateShader(s);
		s.spirv = kShadowVertAlpha; s.spirvWords = sizeof(kShadowVertAlpha) / 4; PYROS_WGSL(s, kShadowVertAlpha); s.debugName = "shadow.vert (alpha)"; shadowAlphaVS = device->CreateShader(s);
		s.spirv = kShadowVertInstancedAlpha; s.spirvWords = sizeof(kShadowVertInstancedAlpha) / 4; PYROS_WGSL(s, kShadowVertInstancedAlpha); s.debugName = "shadow.vert (instanced, alpha)"; shadowInstAlphaVS = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment; s.spirv = kShadowAlphaFrag; s.spirvWords = sizeof(kShadowAlphaFrag) / 4; PYROS_WGSL(s, kShadowAlphaFrag); s.debugName = "shadowalpha.frag"; shadowAlphaFS = device->CreateShader(s);
		s.spirv = kParticleFrag; s.spirvWords = sizeof(kParticleFrag) / 4; PYROS_WGSL(s, kParticleFrag); s.debugName = "particle.frag"; particleFS = device->CreateShader(s);
		s.stage = gfx::Stage::Vertex; s.spirv = kParticleVert; s.spirvWords = sizeof(kParticleVert) / 4; PYROS_WGSL(s, kParticleVert); s.debugName = "particle.vert"; particleVS = device->CreateShader(s);
		s.spirv = kUIVert; s.spirvWords = sizeof(kUIVert) / 4; PYROS_WGSL(s, kUIVert); s.debugName = "ui.vert"; uiVS = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment; s.spirv = kUIFrag; s.spirvWords = sizeof(kUIFrag) / 4; PYROS_WGSL(s, kUIFrag); s.debugName = "ui.frag"; uiFS = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment; s.spirv = kDepthOnlyFrag; s.spirvWords = sizeof(kDepthOnlyFrag) / 4; PYROS_WGSL(s, kDepthOnlyFrag); s.debugName = "depthonly.frag";
		depthOnlyFS = device->CreateShader(s);
		s.spirv = kTransmittanceFrag; s.spirvWords = sizeof(kTransmittanceFrag) / 4; PYROS_WGSL(s, kTransmittanceFrag); s.debugName = "transmittance.frag";
		transmittanceFS = device->CreateShader(s);
		s.spirv = kSkyViewFrag; s.spirvWords = sizeof(kSkyViewFrag) / 4; PYROS_WGSL(s, kSkyViewFrag); s.debugName = "skyview.frag";
		skyViewFS = device->CreateShader(s);
		s.spirv = kSkyFrag; s.spirvWords = sizeof(kSkyFrag) / 4; PYROS_WGSL(s, kSkyFrag); s.debugName = "sky.frag";
		skyFS = device->CreateShader(s);
		s.spirv = kCloudsFrag; s.spirvWords = sizeof(kCloudsFrag) / 4; PYROS_WGSL(s, kCloudsFrag); s.debugName = "clouds.frag"; cloudsFS = device->CreateShader(s);
		s.spirv = kCloudsDomeFrag; s.spirvWords = sizeof(kCloudsDomeFrag) / 4; PYROS_WGSL(s, kCloudsDomeFrag); s.debugName = "clouds.frag (dome)"; cloudsDomeFS = device->CreateShader(s);
		s.spirv = kCloudShadowFrag; s.spirvWords = sizeof(kCloudShadowFrag) / 4; PYROS_WGSL(s, kCloudShadowFrag); s.debugName = "cloudshadow.frag"; cloudShadowFS = device->CreateShader(s);
		s.spirv = kTaaFrag; s.spirvWords = sizeof(kTaaFrag) / 4; PYROS_WGSL(s, kTaaFrag); s.debugName = "taa.frag"; taaFS = device->CreateShader(s);
		s.spirv = kPostFrag; s.spirvWords = sizeof(kPostFrag) / 4; PYROS_WGSL(s, kPostFrag); s.debugName = "post.frag"; postFS = device->CreateShader(s);
		s.spirv = kRtShadowFrag; s.spirvWords = sizeof(kRtShadowFrag) / 4; PYROS_WGSL(s, kRtShadowFrag); s.debugName = "rtshadow.frag"; rtShadowFS = device->CreateShader(s);
		s.spirv = kRtReflectFrag; s.spirvWords = sizeof(kRtReflectFrag) / 4; PYROS_WGSL(s, kRtReflectFrag); s.debugName = "rtreflect.frag"; rtReflectFS = device->CreateShader(s);
#if !defined(__EMSCRIPTEN__)
		if (device->Caps().rayQuery)
		{
			gfx::ShaderDesc q = s; q.wgsl = nullptr;
			q.spirv = kRtShadowRQFrag; q.spirvWords = sizeof(kRtShadowRQFrag) / 4; q.debugName = "rtshadow.frag (ray queries)"; rtShadowRQFS = device->CreateShader(q);
			q.spirv = kRtReflectRQFrag; q.spirvWords = sizeof(kRtReflectRQFrag) / 4; q.debugName = "rtreflect.frag (ray queries)"; rtReflectRQFS = device->CreateShader(q);
		}
#endif
		{
			gfx::ShaderDesc c = s; c.stage = gfx::Stage::Compute; c.spirv = kParticleSimComp; c.spirvWords = sizeof(kParticleSimComp) / 4; PYROS_WGSL(c, kParticleSimComp);
			c.debugName = "particle_sim.comp"; particleSimCS = device->CreateShader(c);
		}
		s.spirv = kParticleFragHalf; s.spirvWords = sizeof(kParticleFragHalf) / 4; PYROS_WGSL(s, kParticleFragHalf); s.debugName = "particle.frag (half)"; particleHalfFS = device->CreateShader(s);
		s.spirv = kParticleCompositeFrag; s.spirvWords = sizeof(kParticleCompositeFrag) / 4; PYROS_WGSL(s, kParticleCompositeFrag); s.debugName = "particlecomposite.frag"; particleCompositeFS = device->CreateShader(s);
		s.spirv = kSkyboxFrag; s.spirvWords = sizeof(kSkyboxFrag) / 4; PYROS_WGSL(s, kSkyboxFrag); s.debugName = "skybox.frag"; skyboxFS = device->CreateShader(s);
		s.spirv = kSkyboxTableFrag; s.spirvWords = sizeof(kSkyboxTableFrag) / 4; PYROS_WGSL(s, kSkyboxTableFrag); s.debugName = "skyboxtable.frag"; skyboxTableFS = device->CreateShader(s);
		s.stage = gfx::Stage::Vertex; s.spirv = kOverlayVert; s.spirvWords = sizeof(kOverlayVert) / 4; PYROS_WGSL(s, kOverlayVert); s.debugName = "overlay.vert"; overlayVS = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment; s.spirv = kOverlayFrag; s.spirvWords = sizeof(kOverlayFrag) / 4; PYROS_WGSL(s, kOverlayFrag); s.debugName = "overlay.frag"; overlayFS = device->CreateShader(s);
		s.stage = gfx::Stage::Vertex;
		s.spirv = kSurfaceVertSkinned; s.spirvWords = sizeof(kSurfaceVertSkinned) / 4; PYROS_WGSL(s, kSurfaceVertSkinned); s.debugName = "surface.vert (skinned)"; surfaceSkinVS = device->CreateShader(s);
		s.spirv = kSurfaceVertColor; s.spirvWords = sizeof(kSurfaceVertColor) / 4; PYROS_WGSL(s, kSurfaceVertColor); s.debugName = "surface.vert (vertex colour)"; surfaceColorVS = device->CreateShader(s);
		s.spirv = kDepthVertSkinned; s.spirvWords = sizeof(kDepthVertSkinned) / 4; PYROS_WGSL(s, kDepthVertSkinned); s.debugName = "depth.vert (skinned)"; depthSkinVS = device->CreateShader(s);
		s.spirv = kShadowVertSkinned; s.spirvWords = sizeof(kShadowVertSkinned) / 4; PYROS_WGSL(s, kShadowVertSkinned); s.debugName = "shadow.vert (skinned)"; shadowSkinVS = device->CreateShader(s);
		s.spirv = kShadowVertSkinnedAlpha; s.spirvWords = sizeof(kShadowVertSkinnedAlpha) / 4; PYROS_WGSL(s, kShadowVertSkinnedAlpha); s.debugName = "shadow.vert (skinned, alpha)"; shadowSkinAlphaVS = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment;
		s.spirv = kVelocityFrag; s.spirvWords = sizeof(kVelocityFrag) / 4; PYROS_WGSL(s, kVelocityFrag); s.debugName = "velocity.frag"; velocityFS = device->CreateShader(s);
		s.spirv = kNoVelocityFrag; s.spirvWords = sizeof(kNoVelocityFrag) / 4; PYROS_WGSL(s, kNoVelocityFrag); s.debugName = "novelocity.frag"; noVelocityFS = device->CreateShader(s);
		{
			gfx::TextureDesc sd; sd.width = sd.height = 1; sd.format = kDepthFormat; sd.usage = gfx::TexDepthTarget | gfx::TexSampled; sd.debugName = "Renderer.StandInDepth";
			standInDepth = device->CreateTexture(sd);
		}
		s.spirv = kSkyShFrag; s.spirvWords = sizeof(kSkyShFrag) / 4; PYROS_WGSL(s, kSkyShFrag); s.debugName = "skysh.frag";
		skyShFS = device->CreateShader(s);
		s.spirv = kGtaoFrag; s.spirvWords = sizeof(kGtaoFrag) / 4; PYROS_WGSL(s, kGtaoFrag); s.debugName = "gtao.frag";
		gtaoFS = device->CreateShader(s);
		s.spirv = kAoBlurFrag; s.spirvWords = sizeof(kAoBlurFrag) / 4; PYROS_WGSL(s, kAoBlurFrag); s.debugName = "aoblur.frag";
		aoBlurFS = device->CreateShader(s);
		s.spirv = kSkyShSumFrag; s.spirvWords = sizeof(kSkyShSumFrag) / 4; PYROS_WGSL(s, kSkyShSumFrag); s.debugName = "skyshsum.frag";
		skyShSumFS = device->CreateShader(s);
		s.spirv = kBloomDownFrag; s.spirvWords = sizeof(kBloomDownFrag) / 4; PYROS_WGSL(s, kBloomDownFrag); s.debugName = "bloomdown.frag"; bloomDownFS = device->CreateShader(s);
		s.spirv = kProbeFilterFrag; s.spirvWords = sizeof(kProbeFilterFrag) / 4; PYROS_WGSL(s, kProbeFilterFrag); s.debugName = "probefilter.frag"; probeFilterFS = device->CreateShader(s);
		s.spirv = kBloomUpFrag; s.spirvWords = sizeof(kBloomUpFrag) / 4; PYROS_WGSL(s, kBloomUpFrag); s.debugName = "bloomup.frag"; bloomUpFS = device->CreateShader(s);
		s.spirv = kLuminanceFrag; s.spirvWords = sizeof(kLuminanceFrag) / 4; PYROS_WGSL(s, kLuminanceFrag); s.debugName = "luminance.frag"; luminanceFS = device->CreateShader(s);
		s.spirv = kReduce4Frag; s.spirvWords = sizeof(kReduce4Frag) / 4; PYROS_WGSL(s, kReduce4Frag); s.debugName = "reduce4.frag"; reduceFS = device->CreateShader(s);
		s.spirv = kAdaptFrag; s.spirvWords = sizeof(kAdaptFrag) / 4; PYROS_WGSL(s, kAdaptFrag); s.debugName = "adapt.frag"; adaptFS = device->CreateShader(s);
		s.stage = gfx::Stage::Vertex; s.spirv = kDepthVert; s.spirvWords = sizeof(kDepthVert) / 4; PYROS_WGSL(s, kDepthVert); s.debugName = "depth.vert";
		depthVS = device->CreateShader(s);
	}

	void SceneRenderer::CreateDefaults()
	{
		const uchar w[4] = { 255, 255, 255, 255 }, n[4] = { 128, 128, 255, 255 };
		gfx::TextureDesc t; t.format = gfx::Format::RGBA8; t.usage = gfx::TexSampled;
		gfx::TextureData d; d.size = 4;
		d.bytes = w; t.debugName = "Renderer.White"; white = device->CreateTexture(t, &d, 1);
		d.bytes = n; t.debugName = "Renderer.FlatNormal"; flatNormal = device->CreateTexture(t, &d, 1);
	}

	namespace { std::unordered_map<const Texture*, gfx::Texture>& ViewTextures() { static std::unordered_map<const Texture*, gfx::Texture> all; return all; } }

	void SceneRenderer::SetViewTexture(const Texture* texture, const gfx::Texture picture)
	{
		if (picture) ViewTextures()[texture] = picture; else ViewTextures().erase(texture);
	}

	gfx::Texture SceneRenderer::TextureFor(Texture* source, const bool srgb, const gfx::Texture fallback)
	{
		if (!source) return fallback;
		// (a SceneView's: its picture, as it was last drawn)
		if (!ViewTextures().empty())
		{
			std::unordered_map<const Texture*, gfx::Texture>::const_iterator v = ViewTextures().find(source);
			if (v != ViewTextures().end()) return v->second == drawingInto ? fallback : v->second;
		}
		// (by the asset's address and the revision of its pixels - until assets have handles of their own)
		TextureGpu &g = textures[std::make_pair(source, srgb)];
		// (another texture made where a freed one was: not its pixels)
		if (g.textureId != source->GetInstanceID()) { g.revision = 0xFFFFFFFFu; g.textureId = source->GetInstanceID(); }
		if (g.texture && g.revision == source->GetCpuRevision()) return g.texture;
		const std::vector<uchar>* pixels = source->GetCpuPixels(source->GetTextureType(), 0);
		const uint32 w = source->GetWidth(0), h = source->GetHeight(0);
		if (!pixels || w == 0 || h == 0) return g.texture ? g.texture : fallback;
		// As RGBA8: a one-channel picture (a font's coverage) into every channel, two as grey and alpha, three with alpha 1
		const size_t texels = (size_t)w * h, channels = pixels->size() / texels;
		if (channels < 1 || channels > 4 || pixels->size() != texels * channels) return g.texture ? g.texture : fallback;
		std::vector<uchar> rgba;
		if (channels == 4) rgba = *pixels;
		else
		{
			rgba.resize(texels * 4);
			const uchar* p = pixels->data();
			for (size_t t = 0; t < texels; t++)
			{
				uchar* o = &rgba[t * 4];
				if (channels == 1) o[0] = o[1] = o[2] = o[3] = p[t];
				else if (channels == 2) { o[0] = o[1] = o[2] = p[t * 2]; o[3] = p[t * 2 + 1]; }
				else { o[0] = p[t * 3]; o[1] = p[t * 3 + 1]; o[2] = p[t * 3 + 2]; o[3] = 255; }
			}
		}
		// The whole chain, down to 1x1
		std::vector<std::vector<uchar> > mips(1, std::move(rgba));
		std::vector<std::pair<uint32, uint32> > sizes(1, std::make_pair(w, h));
		while (sizes.back().first > 1 || sizes.back().second > 1)
		{
			uint32 nw, nh;
			mips.push_back(HalveRGBA8(mips.back(), sizes.back().first, sizes.back().second, srgb, nw, nh));
			sizes.push_back(std::make_pair(nw, nh));
		}
		std::vector<gfx::TextureData> data(mips.size());
		for (size_t i = 0; i < mips.size(); i++) { data[i].mip = (uint32_t)i; data[i].bytes = mips[i].data(); data[i].size = mips[i].size(); }
		gfx::TextureDesc t; t.width = w; t.height = h; t.mips = (uint32_t)mips.size();
		t.format = srgb ? gfx::Format::RGBA8_sRGB : gfx::Format::RGBA8; t.usage = gfx::TexSampled; t.debugName = "Renderer.Material";
		if (g.texture) device->Destroy(g.texture);
		g.texture = device->CreateTexture(t, data.data(), (uint32_t)data.size());
		g.revision = source->GetCpuRevision();
		return g.texture ? g.texture : fallback;
	}

	gfx::Texture SceneRenderer::SkyboxFor(Texture* source)
	{
		if (!source) return gfx::Texture();
		if (skyboxTex && source == skyboxMadeFrom && skyboxRevision == source->GetCpuRevision()) return skyboxTex;
		// Its six faces (+x -x +y -y +z -z, as a cube's layers are), RGBA8 sRGB - each kept on the CPU as it loaded
		const uint32 w = source->GetWidth(0), h = source->GetHeight(0);
		std::vector<std::vector<uchar> > faces(6);
		for (uint32 f = 0; f < 6; f++)
		{
			const std::vector<uchar>* px = source->GetCpuPixels(TextureType::CubemapPositive_X + f, 0);
			if (!px || w == 0 || h == 0) return skyboxTex;
			const size_t texels = (size_t)w * h, channels = px->size() / texels;
			if (channels == 4 && px->size() == texels * 4) faces[f] = *px;
			else if (channels == 3 && px->size() == texels * 3)
			{
				faces[f].resize(texels * 4);
				for (size_t t = 0; t < texels; t++) { faces[f][t * 4] = (*px)[t * 3]; faces[f][t * 4 + 1] = (*px)[t * 3 + 1]; faces[f][t * 4 + 2] = (*px)[t * 3 + 2]; faces[f][t * 4 + 3] = 255; }
			}
			else return skyboxTex;
		}
		gfx::TextureData data[6];
		for (uint32 f = 0; f < 6; f++) { data[f].layer = f; data[f].bytes = faces[f].data(); data[f].size = faces[f].size(); }
		gfx::TextureDesc t; t.kind = gfx::TextureKind::Cube; t.width = w; t.height = h; t.depthOrLayers = 6;
		t.format = gfx::Format::RGBA8_sRGB; t.usage = gfx::TexSampled; t.debugName = "Renderer.Skybox";
		if (skyboxTex) device->Destroy(skyboxTex);
		if (skyboxGroup) { device->Destroy(skyboxGroup); skyboxGroup = gfx::BindGroup(); }
		if (skyboxTableGroup) { device->Destroy(skyboxTableGroup); skyboxTableGroup = gfx::BindGroup(); }
		skyboxTex = device->CreateTexture(t, data, 6);
		skyboxMadeFrom = source; skyboxRevision = source->GetCpuRevision();
		return skyboxTex;
	}

	gfx::BindGroup SceneRenderer::SurfaceGroupFor(const gfx::Pipeline layoutOf, const std::array<gfx::Texture, 5> &maps)
	{
		const std::array<uint32_t, 5> key = { maps[0].id, maps[1].id, maps[2].id, maps[3].id, maps[4].id };
		std::map<std::array<uint32_t, 5>, gfx::BindGroup>::iterator i = surfaceGroups.find(key);
		if (i != surfaceGroups.end()) return i->second;
		gfx::Device::BindGroupEntry e[24];
		e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
		e[1].binding = 1; e[1].buffer = lightData; e[1].size = kLightDataSlot;
		for (int m = 0; m < 4; m++) { e[2 + m].binding = 2 + m; e[2 + m].texture = maps[m]; }
		e[6].binding = 6; e[6].sampler = materialSampler;
		e[7].binding = 7; e[7].buffer = shadowData; e[7].size = sizeof(ShadowData);
		e[8].binding = 8; e[8].texture = shadowAtlas;
		e[9].binding = 9; e[9].sampler = shadowCompare;
		e[10].binding = 10; e[10].texture = skySH;
		e[11].binding = 11; e[11].texture = AOForSurfaces();
		e[12].binding = 12; e[12].texture = skyViewClouded;     // (the sky with its clouds: what the surface mirrors)
		e[13].binding = 13; e[13].texture = prepassDepth;
		e[14].binding = 14; e[14].sampler = linearClamp;
		e[15].binding = 15; e[15].texture = cloudShadowTex;     // (the clouds' shadow on the sun)
		e[16].binding = 16; e[16].texture = PreviousForSurfaces();      // (last frame's picture: what the reflections find)
		e[17].binding = 17; e[17].buffer = bonePalette;
		e[18].binding = 18; e[18].texture = maps[4];     // (the height map: parallax)
		e[19].binding = 19; e[19].texture = probeAtlas ? probeAtlas : white;
		e[20].binding = 21; e[20].texture = giIrradiance ? giIrradiance : white;
		e[21].binding = 22; e[21].texture = giVisibility ? giVisibility : white;
		e[22].binding = 23; e[22].texture = giProbes ? giProbes : white;
		e[23].binding = 24; e[23].texture = giRadiance ? giRadiance : white;
		const gfx::BindGroup g = device->CreateBindGroup(layoutOf, e, 24);
		surfaceGroups[key] = g;
		return g;
	}

	uint32 SceneRenderer::WriteLights(SceneGraph* scene, const View &view, const uint32 slot)
	{
		// The lights that cast shadows, nearest the camera first, given tiles while there are tiles: a spot one, a point
		// light six
		localTiles.clear();
		rainTile = -1;
		std::map<IComponent*, int> firstTile;
		if (scene)
		{
			const Vec3 eye = view.cameraWorld.GetTranslation();
			std::vector<std::pair<f32, IComponent*> > wanted;
			for (IComponent* c : scene->GetLights())
			{
				if (!c || !c->GetOwner()) continue;
				const uint32 type = c->GetComponentType();
				if (type != ComponentType::PointLight && type != ComponentType::SpotLight) continue;
				ILightComponent* light = static_cast<ILightComponent*>(c);
				if (!light->IsCastingShadows()) continue;
				const f32 radius = type == ComponentType::PointLight ? static_cast<PointLight*>(c)->GetLightRadius() : static_cast<SpotLight*>(c)->GetLightRadius();
				wanted.push_back(std::make_pair(std::max(0.f, (c->GetOwner()->GetDrawWorld().GetTranslation() - eye).magnitude() - radius), c));
			}
			std::sort(wanted.begin(), wanted.end(), [](const std::pair<f32, IComponent*> &a, const std::pair<f32, IComponent*> &b) { return a.first < b.first; });
			for (const std::pair<f32, IComponent*> &w : wanted)
			{
				IComponent* c = w.second;
				const bool point = c->GetComponentType() == ComponentType::PointLight;
				const uint32 faces = point ? 6 : 1;
				if (localTiles.size() + faces > kLocalTiles) continue;
				const Matrix &world = c->GetOwner()->GetDrawWorld();
				const Vec3 p = world.GetTranslation();
				const f32 radius = point ? static_cast<PointLight*>(c)->GetLightRadius() : static_cast<SpotLight*>(c)->GetLightRadius();
				const f32 nearZ = std::max(0.05f, radius * 0.002f);
				firstTile[c] = (int)localTiles.size();
				for (uint32 f = 0; f < faces; f++)
				{
					LocalTile t;
					Vec3 forward, up;
					if (point)
					{
						static const Vec3 axes[6] = { Vec3(1, 0, 0), Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, -1, 0), Vec3(0, 0, 1), Vec3(0, 0, -1) };
						forward = axes[f]; up = f == 2 || f == 3 ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
						t.tanHalf = 1.f + 4.f / kLocalTile;                      // (90 degrees and a few texels: the faces meet without a seam)
					}
					else
					{
						SpotLight* s = static_cast<SpotLight*>(c);
						forward = (world * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();
						up = std::fabs(forward.y) > 0.99f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
						const f32 half = std::acos(std::min(std::max(s->GetLightCosOutterCone(), -1.f), 1.f));
						t.tanHalf = std::tan(std::min(half + 0.035f, 1.48f));
					}
					const Vec3 right = forward.cross(up).normalize(), u = right.cross(forward).normalize();
					Matrix v;
					v.m[0] = right.x; v.m[4] = right.y; v.m[8] = right.z;  v.m[12] = -right.dotProduct(p);
					v.m[1] = u.x;     v.m[5] = u.y;     v.m[9] = u.z;      v.m[13] = -u.dotProduct(p);
					v.m[2] = -forward.x; v.m[6] = -forward.y; v.m[10] = -forward.z; v.m[14] = forward.dotProduct(p);
					v.m[3] = 0.f; v.m[7] = 0.f; v.m[11] = 0.f; v.m[15] = 1.f;
					// Reversed Z perspective, near to the light's reach: depth 1 at the near plane, 0 at the radius
					Matrix pr;
					for (int k = 0; k < 16; k++) pr.m[k] = 0.f;
					const f32 n = nearZ, fz = std::max(radius, nearZ * 2.f);
					pr.m[0] = 1.f / t.tanHalf; pr.m[5] = 1.f / t.tanHalf;
					pr.m[10] = n / (fz - n); pr.m[14] = n * fz / (fz - n);
					pr.m[11] = -1.f;
					t.view = v; t.viewProjection = pr * v; t.position = p; t.nearZ = n; t.farZ = fz;
					localTiles.push_back(t);
				}
			}
			// Every directional light after the first (the sun, with its cascades): a light of its own in the list, and where it
			// casts shadows a tile of its own - orthographic along it, round the eye (a second sun's shadows, a rim light's)
			{
				bool first = true;
				for (IComponent* c : scene->GetLights())
				{
					if (!c || c->GetComponentType() != ComponentType::DirectionalLight) continue;
					if (first) { first = false; continue; }
					DirectionalLight* dl = static_cast<DirectionalLight*>(c);
					if (!dl->IsCastingShadows() || localTiles.size() >= kLocalTiles) continue;
					Vec3 toLight;
					dl->GetLightRadianceNow(&toLight);
					const Vec3 forward = (toLight * -1.f).normalize();
					// (the square over what is in view: ahead of the eye by its half size, as wide as half the shadow distance - at
					// least 40 m - so it holds what the camera looks at, not only the ground under it)
					const f32 half = std::min(std::max(dl->GetShadowFar() * 0.5f, 40.f), 250.f), back = 2.f * half + 100.f, depth = 2.f * back;
					const Vec3 look = (view.cameraWorld * Vec4(0.f, 0.f, -1.f, 0.f)).xyz().normalize();
					const Vec3 p = eye + look * half - forward * back;
					const Vec3 up = std::fabs(forward.y) > 0.99f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
					const Vec3 right = forward.cross(up).normalize(), u = right.cross(forward).normalize();
					LocalTile t;
					t.ortho = true; t.halfSize = half;
					Matrix v;
					v.m[0] = right.x; v.m[4] = right.y; v.m[8] = right.z;  v.m[12] = -right.dotProduct(p);
					v.m[1] = u.x;     v.m[5] = u.y;     v.m[9] = u.z;      v.m[13] = -u.dotProduct(p);
					v.m[2] = -forward.x; v.m[6] = -forward.y; v.m[10] = -forward.z; v.m[14] = forward.dotProduct(p);
					v.m[3] = 0.f; v.m[7] = 0.f; v.m[11] = 0.f; v.m[15] = 1.f;
					// (texel-snapped across it, so its shadows do not crawl as the eye moves)
					const f32 texel = 2.f * half / kLocalTile;
					v.m[12] = std::floor(v.m[12] / texel) * texel; v.m[13] = std::floor(v.m[13] / texel) * texel;
					// Reversed Z orthographic: depth 1 at the light's end, 0 `depth` metres along it
					Matrix pr;
					for (int k = 0; k < 16; k++) pr.m[k] = 0.f;
					pr.m[0] = 1.f / half; pr.m[5] = 1.f / half; pr.m[10] = 1.f / depth; pr.m[14] = 1.f; pr.m[15] = 1.f;
					t.view = v; t.viewProjection = pr * v; t.position = p; t.nearZ = 0.f; t.farZ = depth;
					firstTile[c] = (int)localTiles.size();
					localTiles.push_back(t);
				}
			}
			// While it rains: one more tile, looking straight down round the eye (orthographic, 120 m a side, its centre
			// snapped to its texels so it does not crawl) - what has a roof over it stays dry (RainExposure, shadows.glsl)
			if (RenderGlobals::Rain() > 0.f && localTiles.size() < kLocalTiles)
			{
				const f32 half = 60.f, texel = 2.f * half / kLocalTile, top = 150.f, depth = 400.f;
				const Vec3 p(std::floor(eye.x / texel) * texel, eye.y + top, std::floor(eye.z / texel) * texel);
				LocalTile t;
				t.ortho = true; t.halfSize = half;
				Matrix v;      // (looking down -y; x right, -z up on the map)
				v.m[0] = 1.f; v.m[4] = 0.f; v.m[8] = 0.f;  v.m[12] = -p.x;
				v.m[1] = 0.f; v.m[5] = 0.f; v.m[9] = -1.f; v.m[13] = p.z;
				v.m[2] = 0.f; v.m[6] = 1.f; v.m[10] = 0.f; v.m[14] = -p.y;
				v.m[3] = 0.f; v.m[7] = 0.f; v.m[11] = 0.f; v.m[15] = 1.f;
				// Reversed Z orthographic: depth 1 at the top, 0 `depth` metres down
				Matrix pr;
				for (int k = 0; k < 16; k++) pr.m[k] = 0.f;
				pr.m[0] = 1.f / half; pr.m[5] = 1.f / half; pr.m[10] = 1.f / depth; pr.m[14] = 1.f; pr.m[15] = 1.f;
				t.view = v; t.viewProjection = pr * v; t.position = p; t.nearZ = 0.f; t.farZ = depth;
				rainTile = (int)localTiles.size();
				localTiles.push_back(t);
			}
		}

		// (built here, then uploaded - the count and the lights in use, not the whole slot: every backend's way, WebGPU's
		// writeBuffer as Metal's copy)
		lightScratch.assign(16 + sizeof(LightData) * kMaxLights, 0);
		uint8* base = lightScratch.data();
		LightData* out = (LightData*)(base + 16);
		uint32 count = 0;
		if (scene)
			for (IComponent* c : scene->GetLights())
			{
				if (!c || count >= kMaxLights) continue;
				const uint32 type = c->GetComponentType();
				if (type != ComponentType::PointLight && type != ComponentType::SpotLight) continue;
				ILightComponent* light = static_cast<ILightComponent*>(c);
				GameObject* owner = c->GetOwner();
				if (!owner) continue;
				const Matrix &world = owner->GetDrawWorld();
				const Vec3 p = world.GetTranslation();
				const Vec4 radiance = light->GetLightRadiance();
				LightData &L = out[count];
				memset(&L, 0, sizeof(L));
				const f32 k = (f32)PI;     // (intensity as a white surface lit head-on at 1 unit gives it back - as the sun's)
				L.positionRange[0] = p.x; L.positionRange[1] = p.y; L.positionRange[2] = p.z;
				L.radianceType[0] = radiance.x * k; L.radianceType[1] = radiance.y * k; L.radianceType[2] = radiance.z * k;
				if (type == ComponentType::PointLight)
					L.positionRange[3] = static_cast<PointLight*>(c)->GetLightRadius();
				else
				{
					SpotLight* s = static_cast<SpotLight*>(c);
					L.positionRange[3] = s->GetLightRadius();
					L.radianceType[3] = 1.f;
					const Vec3 d = (world * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();
					L.directionCosOuter[0] = d.x; L.directionCosOuter[1] = d.y; L.directionCosOuter[2] = d.z; L.directionCosOuter[3] = s->GetLightCosOutterCone();
					L.cosInner[0] = s->GetLightCosInnerCone();
				}
				const std::map<IComponent*, int>::const_iterator tile = firstTile.find(c);
				L.cosInner[1] = tile != firstTile.end() ? (f32)tile->second : -1.f;
				count++;
			}
		// (the directional lights after the sun: type 2, from everywhere along its direction - the sun is the frame's own)
		if (scene)
		{
			bool first = true;
			for (IComponent* c : scene->GetLights())
			{
				if (!c || c->GetComponentType() != ComponentType::DirectionalLight) continue;
				if (first) { first = false; continue; }
				if (count >= kMaxLights) break;
				Vec3 toLight;
				const Vec4 radiance = static_cast<DirectionalLight*>(c)->GetLightRadianceNow(&toLight);
				LightData &L = out[count];
				memset(&L, 0, sizeof(L));
				const f32 k = (f32)PI;
				L.positionRange[3] = 1e30f;
				L.radianceType[0] = radiance.x * k; L.radianceType[1] = radiance.y * k; L.radianceType[2] = radiance.z * k; L.radianceType[3] = 2.f;
				const Vec3 l = toLight.normalize();
				L.directionCosOuter[0] = l.x; L.directionCosOuter[1] = l.y; L.directionCosOuter[2] = l.z;
				const std::map<IComponent*, int>::const_iterator tile = firstTile.find(c);
				L.cosInner[1] = tile != firstTile.end() ? (f32)tile->second : -1.f;
				L.cosInner[2] = tile != firstTile.end() ? localTiles[tile->second].halfSize : 40.f;
				count++;
			}
		}
		// After them, the scene's 2D shadow casters (Occluder2D::PublishedSegments): a segment an entry, (x0, y0, x1, y1)
		// in its first four floats - their count the header's second word (the 2D-lit sprites test their lights against them)
		uint32 occluders = 0;
		for (const Vec4 &seg : Occluder2D::PublishedSegments())
		{
			if (count + occluders >= kMaxLights) break;
			LightData &L = out[count + occluders];
			memset(&L, 0, sizeof(L));
			L.positionRange[0] = seg.x; L.positionRange[1] = seg.y; L.positionRange[2] = seg.z; L.positionRange[3] = seg.w;
			occluders++;
		}
		// Then the reflection probes seen, the smallest box first (a point takes the first that holds it): box min (w its
		// blend), box max (w its block in the atlas), where it was seen from - their count the header's third word
		uint32 probes = 0;
		if (scene && probesOn && probeAtlas)
		{
			std::vector<std::pair<f32, ReflectionProbe*> > seen;
			for (ReflectionProbe* p : ReflectionProbes::All())
			{
				if (!p || p->GetScene() != scene || !p->GetOwner()) continue;
				const std::map<ReflectionProbe*, ProbeState>::const_iterator st = probeStates.find(p);
				if (st == probeStates.end() || !st->second.seen || st->second.slot < 0) continue;
				const Vec3 &e = p->GetExtents();
				seen.push_back(std::make_pair(e.x * e.y * e.z, p));
			}
			std::stable_sort(seen.begin(), seen.end(), [](const std::pair<f32, ReflectionProbe*> &a, const std::pair<f32, ReflectionProbe*> &b) { return a.first < b.first; });
			for (const std::pair<f32, ReflectionProbe*> &sp : seen)
			{
				if (count + occluders + probes >= kMaxLights) break;
				ReflectionProbe* p = sp.second;
				const ProbeState &st = probeStates[p];
				const Vec3 c = p->GetOwner()->GetWorldPosition(), &e = p->GetExtents();
				LightData &L = out[count + occluders + probes];
				memset(&L, 0, sizeof(L));
				L.positionRange[0] = c.x - e.x; L.positionRange[1] = c.y - e.y; L.positionRange[2] = c.z - e.z; L.positionRange[3] = p->GetBlend();
				L.radianceType[0] = c.x + e.x; L.radianceType[1] = c.y + e.y; L.radianceType[2] = c.z + e.z; L.radianceType[3] = (f32)st.slot;
				L.directionCosOuter[0] = st.seenFrom.x; L.directionCosOuter[1] = st.seenFrom.y; L.directionCosOuter[2] = st.seenFrom.z;
				probes++;
			}
		}
		// Then the DDGI's grid, in two entries: origin (w: probes a row of its atlases), spacing (w: a 1 when the probes
		// have their offsets), counts (w: the probes' count), resolutions (x irradiance, y visibility); the header's
		// fourth word 1
		uint32 giEntries = 0;
		if (gi && giIrradiance && count + occluders + probes + 2 <= kMaxLights)
		{
			LightData* G = &out[count + occluders + probes];
			memset(G, 0, 2 * sizeof(LightData));
			const ProbeAtlas &irr = gi->GetIrradianceAtlas(), &vis = gi->GetVisibilityAtlas();
			G[0].positionRange[0] = gi->origin.x; G[0].positionRange[1] = gi->origin.y; G[0].positionRange[2] = gi->origin.z; G[0].positionRange[3] = (f32)irr.GetProbesPerRow();
			G[0].radianceType[0] = gi->spacing.x; G[0].radianceType[1] = gi->spacing.y; G[0].radianceType[2] = gi->spacing.z; G[0].radianceType[3] = gi->GetProbeData().empty() ? 0.f : 1.f;
			G[0].directionCosOuter[0] = (f32)gi->counts[0]; G[0].directionCosOuter[1] = (f32)gi->counts[1]; G[0].directionCosOuter[2] = (f32)gi->counts[2]; G[0].directionCosOuter[3] = (f32)gi->ProbeCount();
			G[0].cosInner[0] = (f32)irr.GetResolution(); G[0].cosInner[1] = (f32)vis.GetResolution();
			// (its reflections: resolution, tiles a row, levels - 0 none - and the roughness the first is filtered for)
			const ProbeAtlas &rad = gi->GetRadianceAtlas();
			const bool reflects = giRadiance && rad.GetResolution() > 0;
			G[1].positionRange[0] = (f32)rad.GetResolution(); G[1].positionRange[1] = (f32)rad.GetProbesPerRow();
			G[1].positionRange[2] = reflects ? (f32)gi->GetRadianceLevels() : 0.f; G[1].positionRange[3] = DDGIVolume::MinRoughness();
			giEntries = 2;
		}
		((uint32*)base)[0] = count;
		((uint32*)base)[1] = occluders;
		((uint32*)base)[2] = probes;
		((uint32*)base)[3] = giEntries ? 1u : 0u;
		device->Upload(lightData, (uint64)slot * kLightDataSlot, base, 16 + sizeof(LightData) * std::max(count + occluders + probes + giEntries, 1u));
		return count;
	}

	void SceneRenderer::CreateSky()
	{
		const gfx::Format f = gfx::Format::RGBA16F;
		gfx::TextureDesc t; t.format = f; t.usage = gfx::TexColorTarget | gfx::TexSampled;
		t.width = 256; t.height = 64; t.debugName = "Renderer.Transmittance"; transmittance = device->CreateTexture(t);
		t.width = 192; t.height = 108; t.debugName = "Renderer.SkyView"; skyView = device->CreateTexture(t);
		t.debugName = "Renderer.SkyViewClouded"; skyViewClouded = device->CreateTexture(t);
		t.width = 9; t.height = 1; t.debugName = "Renderer.SkySH"; skySH = device->CreateTexture(t);
		t.format = gfx::Format::RGBA32F; t.width = 9; t.height = 16; t.debugName = "Renderer.SkySHPartial"; skySHPartial = device->CreateTexture(t);
		gfx::BufferDesc b; b.size = 256 * kFramesInFlight; b.usage = gfx::BufUniform; b.memory = gfx::Memory::Upload; b.debugName = "Renderer.SkyData";
		skyData = device->CreateBuffer(b);

		const gfx::BindingDesc frame = { 0, gfx::BindingKind::Uniform, true, true, false, true };
		const gfx::BindingDesc trans = { 1, gfx::BindingKind::Texture2D, false, true }, view = { 2, gfx::BindingKind::Texture2D, false, true }, samp = { 3, gfx::BindingKind::Sampler, false, true };
		gfx::GraphicsPipelineDesc p;
		p.vertex = fullscreenVS; p.cull = gfx::Cull::None; p.depthTest = p.depthWrite = false;
		p.colorCount = 1; p.colorFormats[0] = f;
		p.fragment = transmittanceFS; p.material = { frame }; p.debugName = "Renderer.Transmittance";
		transmittancePipeline = device->CreatePipeline(p);
		p.fragment = skyViewFS; p.material = { frame, trans, samp }; p.debugName = "Renderer.SkyView";
		skyViewPipeline = device->CreatePipeline(p);
		p.fragment = skyShFS; p.material = { frame, trans, view, samp }; p.debugName = "Renderer.SkySH";
		p.colorFormats[0] = gfx::Format::RGBA32F;
		skyShPipeline = device->CreatePipeline(p);
		p.fragment = skyShSumFS; p.material = { { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true } }; p.debugName = "Renderer.SkySHSum";
		p.colorFormats[0] = f;
		skyShSumPipeline = device->CreatePipeline(p);
		// (behind everything: only where the depth is still the clear's far - reversed Z, 0)
		p.fragment = skyFS; p.material = { frame, trans, view, samp, { 4, gfx::BindingKind::Uniform, false, true, false, true }, { 5, gfx::BindingKind::Texture2D, false, true } };
		p.colorFormats[0] = kHdrFormat; p.depthFormat = kDepthFormat; p.depthTest = true; p.depthWrite = false; p.depthCompare = gfx::Compare::GreaterEqual;
		p.samples = kSceneSamples;
		p.debugName = "Renderer.Sky";
		skyPipeline = device->CreatePipeline(p);
	}

	void SceneRenderer::RecordSkyboxTables(gfx::CommandList* cl, const uint32 frameOffset)
	{
		if (!skyboxTex || !device->PipelineReady(skyboxTablePipeline) || !device->PipelineReady(skyShPipeline) || !device->PipelineReady(skyShSumPipeline)) return;
		gfx::Device::BindGroupEntry e[4];
		e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
		e[1].binding = 1; e[1].texture = transmittance;
		e[2].binding = 2; e[2].texture = skyViewClouded;
		e[3].binding = 3; e[3].sampler = linearClamp;
		if (!skyShGroup) skyShGroup = device->CreateBindGroup(skyShPipeline, e, 4);
		if (!skyShSumGroup)
		{
			gfx::Device::BindGroupEntry s2[2]; s2[0].binding = 1; s2[0].texture = skySHPartial; s2[1].binding = 2; s2[1].sampler = linearClamp;
			skyShSumGroup = device->CreateBindGroup(skyShSumPipeline, s2, 2);
		}
		if (!skyboxTableGroup)
		{
			gfx::Device::BindGroupEntry k[3];
			k[0].binding = 0; k[0].buffer = frameData; k[0].size = sizeof(FrameData);
			k[1].binding = 1; k[1].texture = skyboxTex;
			k[2].binding = 2; k[2].sampler = linearClamp;
			skyboxTableGroup = device->CreateBindGroup(skyboxTablePipeline, k, 3);
		}
		auto pass = [&](gfx::Texture target, uint32 w, uint32 h, gfx::Pipeline pipe, gfx::BindGroup group, const bool frame, const char* name)
		{
			graph.Use(cl, { { target, gfx::Access::ColorTarget } });
			gfx::PassDesc pd; pd.colorCount = 1; pd.colors[0].texture = target; pd.colors[0].load = gfx::Load::DontCare;
			pd.width = w; pd.height = h; pd.debugName = name;
			cl->BeginPass(pd);
			cl->SetViewport(0.f, 0.f, (f32)w, (f32)h);
			cl->SetPipeline(pipe);
			cl->SetBindGroup(group, frame ? &frameOffset : nullptr, frame ? 1 : 0);
			cl->Draw(3, 1, 0, 0);
			cl->EndPass();
			graph.Use(cl, { { target, gfx::Access::ShaderRead } });
		};
		pass(skyViewClouded, 192, 108, skyboxTablePipeline, skyboxTableGroup, true, "SkyboxTable");
		pass(skySHPartial, 9, 16, skyShPipeline, skyShGroup, true, "SkySH");
		pass(skySH, 9, 1, skyShSumPipeline, skyShSumGroup, false, "SkySHSum");
	}

	void SceneRenderer::RecordSkyTables(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot)
	{
		if (!device->PipelineReady(transmittancePipeline) || !device->PipelineReady(skyViewPipeline) || !device->PipelineReady(skyShPipeline) || !device->PipelineReady(skyShSumPipeline)
			|| !device->PipelineReady(cloudsDomePipeline)) return;
		gfx::Device::BindGroupEntry e[4];
		e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
		e[1].binding = 1; e[1].texture = transmittance;
		e[2].binding = 2; e[2].texture = skyViewClouded;     // (the sky's light summed with its clouds: overcast is dimmer, and whiter)
		e[3].binding = 3; e[3].sampler = linearClamp;
		if (!transmittanceGroup) transmittanceGroup = device->CreateBindGroup(transmittancePipeline, e, 1);
		if (!skyViewGroup) { gfx::Device::BindGroupEntry v[3] = { e[0], e[1], e[3] }; skyViewGroup = device->CreateBindGroup(skyViewPipeline, v, 3); }
		if (!skyShGroup) skyShGroup = device->CreateBindGroup(skyShPipeline, e, 4);
		if (!skyShSumGroup)
		{
			gfx::Device::BindGroupEntry s2[2]; s2[0].binding = 1; s2[0].texture = skySHPartial; s2[1].binding = 2; s2[1].sampler = linearClamp;
			skyShSumGroup = device->CreateBindGroup(skyShSumPipeline, s2, 2);
		}
		auto pass = [&](gfx::Texture target, uint32 w, uint32 h, gfx::Pipeline pipe, gfx::BindGroup group, const char* name)
		{
			graph.Use(cl, { { target, gfx::Access::ColorTarget } });
			gfx::PassDesc pd; pd.colorCount = 1; pd.colors[0].texture = target; pd.colors[0].load = gfx::Load::DontCare;
			pd.width = w; pd.height = h; pd.debugName = name;
			cl->BeginPass(pd);
			cl->SetViewport(0.f, 0.f, (f32)w, (f32)h);
			cl->SetPipeline(pipe);
			cl->SetBindGroup(group, group == skyShSumGroup ? nullptr : &frameOffset, group == skyShSumGroup ? 0 : 1);
			cl->Draw(3, 1, 0, 0);
			cl->EndPass();
			graph.Use(cl, { { target, gfx::Access::ShaderRead } });
		};
		// (the transmittance depends on the air alone: made once)
		if (!transmittanceMade) { pass(transmittance, 256, 64, transmittancePipeline, transmittanceGroup, "Transmittance"); transmittanceMade = true; }
		pass(skyView, 192, 108, skyViewPipeline, skyViewGroup, "SkyView");
		// The same directions with the clouds in them (with no cloud, the sky as it is)
		{
			if (!cloudsDomeGroup)
			{
				gfx::Device::BindGroupEntry c[7];
				c[0].binding = 0; c[0].buffer = frameData; c[0].size = sizeof(FrameData);
				c[1].binding = 1; c[1].texture = cloudNoise;
				c[2].binding = 2; c[2].sampler = repeatLinear;
				c[3].binding = 3; c[3].texture = skyView;
				c[4].binding = 4; c[4].sampler = linearClamp;
				c[5].binding = 5; c[5].buffer = skyData; c[5].size = 192;
				c[6].binding = 6; c[6].texture = transmittance;      // (its history is not read)
				cloudsDomeGroup = device->CreateBindGroup(cloudsDomePipeline, c, 7);
			}
			graph.Use(cl, { { skyViewClouded, gfx::Access::ColorTarget } });
			gfx::PassDesc pd; pd.colorCount = 1; pd.colors[0].texture = skyViewClouded; pd.colors[0].load = gfx::Load::DontCare;
			pd.width = 192; pd.height = 108; pd.debugName = "CloudsDome";
			cl->BeginPass(pd);
			cl->SetViewport(0.f, 0.f, 192.f, 108.f);
			cl->SetPipeline(cloudsDomePipeline);
			const uint32 offsets[2] = { frameOffset, slot * 256 };
			cl->SetBindGroup(cloudsDomeGroup, offsets, 2);
			cl->Draw(3, 1, 0, 0);
			cl->EndPass();
			graph.Use(cl, { { skyViewClouded, gfx::Access::ShaderRead } });
		}
		pass(skySHPartial, 9, 16, skyShPipeline, skyShGroup, "SkySH");
		pass(skySH, 9, 1, skyShSumPipeline, skyShSumGroup, "SkySHSum");
	}

	void SceneRenderer::CreateAO()
	{
		gfx::SamplerDesc pd; pd.min = pd.mag = pd.mip = gfx::Filter::Nearest; pd.u = pd.v = pd.w = gfx::Address::Clamp; pd.anisotropy = 1.f;
		pointClamp = device->CreateSampler(pd);
		const gfx::BindingDesc frame = { 0, gfx::BindingKind::Uniform, true, true, false, true };
		gfx::GraphicsPipelineDesc d;
		d.vertex = depthVS; d.fragment = velocityFS; d.material = { frame, { 1, gfx::BindingKind::Uniform, true, false, false, true }, { 2, gfx::BindingKind::StorageRead, true, false } };
		d.drawDataBytes = 208;
		d.bindings = { { 0, (uint16_t)sizeof(Vertex), false } };
		d.attributes = { { 0, 0, gfx::Format::RGB32F, 0 } };
		d.cull = gfx::Cull::None; d.depthTest = d.depthWrite = true; d.depthCompare = gfx::Compare::GreaterEqual;
		d.colorCount = 1; d.colorFormats[0] = gfx::Format::RG16F; d.depthFormat = kDepthFormat; d.debugName = "Renderer.DepthPrepass";     // (and each surface's move on screen)
		depthPipeline = device->CreatePipeline(d);
		{
			gfx::GraphicsPipelineDesc ds = d;
			ds.vertex = depthSkinVS; ds.bindings.push_back({ 2, 32, false });
			ds.attributes.push_back({ 8, 2, gfx::Format::RGBA32F, 0 }); ds.attributes.push_back({ 9, 2, gfx::Format::RGBA32F, 16 });
			ds.debugName = "Renderer.DepthPrepass (skinned)";
			depthSkinPipeline = device->CreatePipeline(ds);
		}
		{
			// (alpha-cut: the texcoord in, and the base colour map and its sampler for the cut)
			gfx::GraphicsPipelineDesc da = d;
			da.vertex = depthAlphaVS; da.fragment = depthAlphaFS;
			da.material.push_back({ 3, gfx::BindingKind::Texture2D, false, true }); da.material.push_back({ 4, gfx::BindingKind::Sampler, false, true });
			da.attributes.push_back({ 2, 0, gfx::Format::RG32F, 24 });
			da.debugName = "Renderer.DepthPrepass (alpha)";
			depthAlphaPipeline = device->CreatePipeline(da);
			da.vertex = depthInstAlphaVS; AddInstanceStream(da); da.debugName = "Renderer.DepthPrepass (instanced, alpha)";
			depthInstAlphaPipeline = device->CreatePipeline(da);
		}
		d.vertex = depthInstVS; AddInstanceStream(d); d.debugName = "Renderer.DepthPrepass (instanced)";
		depthInstPipeline = device->CreatePipeline(d);
		gfx::GraphicsPipelineDesc g;
		g.vertex = fullscreenVS; g.cull = gfx::Cull::None; g.depthTest = g.depthWrite = false;
		g.colorCount = 1; g.colorFormats[0] = gfx::Format::R8;
		g.fragment = gtaoFS; g.material = { frame, { 1, gfx::BindingKind::DepthTexture, false, true }, { 2, gfx::BindingKind::Sampler, false, true } };
		g.debugName = "Renderer.GTAO";
		gtaoPipeline = device->CreatePipeline(g);
		g.fragment = aoBlurFS; g.material.push_back({ 3, gfx::BindingKind::Texture2D, false, true });
		g.drawDataBytes = 16; g.debugName = "Renderer.AOBlur";
		aoBlurPipeline = device->CreatePipeline(g);
	}

	void SceneRenderer::RecordDepthAndAO(gfx::CommandList* cl, SceneGraph* scene, const uint32 frameOffset)
	{
		// The depth prepass
		graph.Use(cl, { { prepassDepth, gfx::Access::DepthTarget } });
		graph.Use(cl, { { velocityTex, gfx::Access::ColorTarget } });
		gfx::PassDesc pass; pass.colorCount = 1;
		// (where nothing is drawn, a move no surface has: the resolve then goes by the depth)
		pass.colors[0].texture = velocityTex; pass.colors[0].load = gfx::Load::Clear; pass.colors[0].store = gfx::StoreOp::Store;
		pass.colors[0].clear[0] = pass.colors[0].clear[1] = 10000.f; pass.colors[0].clear[2] = pass.colors[0].clear[3] = 0.f;
		pass.depth.texture = prepassDepth; pass.depth.clear = 0.f; pass.depth.store = gfx::StoreOp::Store;
		pass.width = targetWidth; pass.height = targetHeight; pass.debugName = "DepthPrepass";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (f32)targetWidth, (f32)targetHeight);
		if (scene && device->PipelineReady(depthPipeline))
		{
			if (!depthGroup)
			{
				gfx::Device::BindGroupEntry e[3]; e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
				e[1].binding = 1; e[1].buffer = taaData; e[1].size = 208;
				e[2].binding = 2; e[2].buffer = bonePalette;
				depthGroup = device->CreateBindGroup(depthPipeline, e, 3);
			}
			const uint32 depthOffsets[2] = { frameOffset, (uint32)(frameNumber % kFramesInFlight) * 256 };
			cl->SetPipeline(depthPipeline);
			cl->SetBindGroup(depthGroup, depthOffsets, 2);
			const bool instReady = device->PipelineReady(depthInstPipeline);
			int instBound = 0;
			// (what it draws: the opaque, not alpha-cut, not moved by a vertex stage of its own; in their order - by mesh
			// within a material - so the same mesh one after another is one instanced draw)
			std::vector<uint32> depthOrder, ownVertices;
			depthOrder.reserve(opaqueOrder.size());
			// (alpha-cut surfaces - foliage - too, cut by their base colour's alpha, where those pipelines are ready: the lit
			// pass then rejects a crown's hidden leaves before lighting them)
			const bool alphaReady = device->PipelineReady(depthAlphaPipeline) && device->PipelineReady(depthInstAlphaPipeline);
			for (uint32 i : opaqueOrder)
			{
				const DrawItem &it = items[i];
				if (it.material && (it.material->GetOptions() & ShaderUsage::AlphaTest) && !(alphaReady && it.alphaCut && it.baseMap && !it.skinned)) continue;
				if (it.movesItself) { ownVertices.push_back(i); continue; }     // (drawn by their own vertex stage, below)
				if (it.instances && !instReady) continue;
				depthOrder.push_back(i);
			}
			static const Matrix identity;
			for (size_t k = 0; k < depthOrder.size();)
			{
				const DrawItem &it = items[depthOrder[k]];
				size_t e = k + 1;
				if (instReady && !it.instances)
					while (e < depthOrder.size() && items[depthOrder[e]].gpu == it.gpu && !items[depthOrder[e]].instances && !items[depthOrder[e]].moved && !it.moved && !it.skinned && !items[depthOrder[e]].skinned
						&& SameWind(items[depthOrder[e]].material, it.material) && items[depthOrder[e]].alphaCut == it.alphaCut
						&& (!it.alphaCut || items[depthOrder[e]].material == it.material)) e++;     // (a batch carries no last-frame matrices: what moved, alone)
				uint64 batchAt = 0;
				const uint32 count = (uint32)(e - k);
				const bool batch = count > 1 && BatchWrite(depthOrder, (uint32)k, count, batchAt);
				const uint32 drawn = batch ? 1 : count;
				for (uint32 d = 0; d < drawn; d++)
				{
					const DrawItem &di = items[depthOrder[k + d]];
					const bool instanced = batch || di.instances;
					// (the pipeline: 0 plain, 1 instanced, 2 skinned; alpha-cut 3 plain, 4 instanced - each with its base colour map's group)
					const bool cut = di.alphaCut && di.baseMap;
					const int want = cut ? (instanced ? 4 : 3) : instanced ? 1 : di.skinned ? 2 : 0;
					if (want == 2 && !device->PipelineReady(depthSkinPipeline)) continue;
					if (cut)
					{
						gfx::BindGroup &g = depthAlphaGroups[di.baseMap.id];
						if (!g)
						{
							gfx::Device::BindGroupEntry e[5]; e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
							e[1].binding = 1; e[1].buffer = taaData; e[1].size = 208;
							e[2].binding = 2; e[2].buffer = bonePalette;
							e[3].binding = 3; e[3].texture = di.baseMap;
							e[4].binding = 4; e[4].sampler = materialSampler;
							g = device->CreateBindGroup(depthAlphaPipeline, e, 5);
						}
						cl->SetPipeline(want == 4 ? depthInstAlphaPipeline : depthAlphaPipeline); cl->SetBindGroup(g, depthOffsets, 2); instBound = -1;
					}
					else if (want != instBound) { cl->SetPipeline(want == 1 ? depthInstPipeline : want == 2 ? depthSkinPipeline : depthPipeline); cl->SetBindGroup(depthGroup, depthOffsets, 2); instBound = want; }
					f32 models[52] = {};
					if (cut)
					{
						const Vec2 uvScale = di.material->GetUVScale(), uvOffset = di.material->GetUVOffset();
						models[40] = uvScale.x; models[41] = uvScale.y; models[42] = uvOffset.x; models[43] = uvOffset.y;
						models[44] = di.material->GetAlphaCutoff();
					}
					memcpy(models, batch ? identity.m : di.model.m, 64); memcpy(models + 16, batch ? identity.m : di.previousModel.m, 64);
					models[32] = (f32)di.boneBase; models[33] = drawSeconds; models[34] = drawPreviousSeconds;
					const Vec4 wind = WindOf(di.material);
					models[36] = wind.x; models[37] = wind.y; models[38] = wind.z; models[39] = wind.w;
					if (!batch) memcpy(models + 48, di.shape, sizeof(di.shape));
					cl->DrawData(models, sizeof(models));
					if (!batch) { DrawItemGeometry(cl, di); continue; }
					cl->SetVertexBuffer(0, di.gpu->vertices, 0);
					cl->SetVertexBuffer(1, batchRing, batchAt);
					cl->SetIndexBuffer(di.gpu->indices, 0, true);
					cl->DrawIndexed(di.gpu->indexCount, count, 0, 0, 0);
				}
				k = e;
			}
			// Surfaces that move their own vertices (the sea's sheet): through their own vertex stage, where they are
			const uint32 slot = (uint32)(frameNumber % kFramesInFlight);
			for (uint32 i : ownVertices)
			{
				const DrawItem &it = items[i];
				uint64 hash = 0;
				CustomSurface* cs = it.custom ? SurfaceFor(it.custom, hash) : nullptr;
				if (!cs) continue;
				CustomKey key; key.surface = hash; key.cull = (uint8)it.custom->GetCullFace(); key.blend = kPrepassBlend; key.instanced = it.instances ? 1 : 0;
				key.skinned = it.skinned ? 1 : 0;
				const gfx::Pipeline pipe = CustomPipeline(key, cs);
				if (!device->PipelineReady(pipe)) continue;
				const gfx::BindGroup g = CustomGroupFor(it.custom, cs, pipe, true);
				cl->SetPipeline(pipe);
				const uint32 offsets[4] = { frameOffset, slot * kLightDataSlot, slot * kShadowDataSlot, WriteCustomParams(it.custom, cs) };
				cl->SetBindGroup(g, offsets, 4);
				DrawData dd;
				FillDrawData(it, false, &dd);
				cl->DrawData(&dd, sizeof(dd));
				DrawItemGeometry(cl, it);
			}
		}
		cl->EndPass();
		graph.Use(cl, { { velocityTex, gfx::Access::ShaderRead } });
		graph.Use(cl, { { prepassDepth, gfx::Access::ShaderRead } });

		// GTAO, then the blur both ways: aoA -> aoB -> aoA
		const uint32 w = std::max(1u, targetWidth / 2), h = std::max(1u, targetHeight / 2);
		if (!device->PipelineReady(gtaoPipeline) || !device->PipelineReady(aoBlurPipeline)) return;
		gfx::Device::BindGroupEntry e[4];
		e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
		e[1].binding = 1; e[1].texture = prepassDepth;
		e[2].binding = 2; e[2].sampler = pointClamp;
		if (!gtaoGroup) gtaoGroup = device->CreateBindGroup(gtaoPipeline, e, 3);
		e[3].binding = 3; e[3].texture = aoA;
		if (!aoBlurGroupH) aoBlurGroupH = device->CreateBindGroup(aoBlurPipeline, e, 4);
		e[3].texture = aoB;
		if (!aoBlurGroupV) aoBlurGroupV = device->CreateBindGroup(aoBlurPipeline, e, 4);
		auto pass2 = [&](gfx::Texture target, gfx::Pipeline pipe, gfx::BindGroup group, const f32* dir, const char* name)
		{
			graph.Use(cl, { { target, gfx::Access::ColorTarget } });
			gfx::PassDesc pd; pd.colorCount = 1; pd.colors[0].texture = target; pd.colors[0].load = gfx::Load::DontCare;
			pd.width = w; pd.height = h; pd.debugName = name;
			cl->BeginPass(pd);
			cl->SetViewport(0.f, 0.f, (f32)w, (f32)h);
			cl->SetPipeline(pipe);
			if (dir) cl->DrawData(dir, 16);
			cl->SetBindGroup(group, &frameOffset, 1);
			cl->Draw(3, 1, 0, 0);
			cl->EndPass();
			graph.Use(cl, { { target, gfx::Access::ShaderRead } });
		};
		const f32 horizontal[4] = { 1.f / w, 0.f, 0.f, 0.f }, vertical[4] = { 0.f, 1.f / h, 0.f, 0.f };
		pass2(aoA, gtaoPipeline, gtaoGroup, nullptr, "GTAO");
		pass2(aoB, aoBlurPipeline, aoBlurGroupH, horizontal, "AOBlurH");
		pass2(aoA, aoBlurPipeline, aoBlurGroupV, vertical, "AOBlurV");
	}

	void SceneRenderer::CreatePost()
	{
		const gfx::BindingDesc src = { 1, gfx::BindingKind::Texture2D, false, true }, samp = { 2, gfx::BindingKind::Sampler, false, true };
		gfx::GraphicsPipelineDesc p;
		p.vertex = fullscreenVS; p.cull = gfx::Cull::None; p.depthTest = p.depthWrite = false; p.colorCount = 1;
		p.material = { src, samp }; p.drawDataBytes = 32;
		p.colorFormats[0] = BloomFormat();
		p.fragment = bloomDownFS; p.debugName = "Renderer.BloomDown"; bloomDownPipeline = device->CreatePipeline(p);
		{
			// (a reflection probe's six views into its block of the probe atlas, a level of roughness a draw)
			gfx::GraphicsPipelineDesc f = p;
			f.material.clear();
			for (uint32_t b = 1; b <= 6; b++) f.material.push_back({ b, gfx::BindingKind::Texture2D, false, true });
			f.material.push_back({ 7, gfx::BindingKind::Sampler, false, true });
			f.colorFormats[0] = kHdrFormat; f.fragment = probeFilterFS; f.debugName = "Renderer.ProbeFilter";
			probeFilterPipeline = device->CreatePipeline(f);
		}
		p.fragment = bloomUpFS; p.blend[0] = gfx::Blend::Additive; p.debugName = "Renderer.BloomUp"; bloomUpPipeline = device->CreatePipeline(p);
		p.blend[0] = gfx::Blend::Opaque; p.drawDataBytes = 0;
		p.colorFormats[0] = gfx::Format::RG32F;
		{
			// (with the prepass's depth: metered on what was drawn - SceneRenderer::RecordBloomAndExposure)
			gfx::GraphicsPipelineDesc l = p;
			l.material.push_back({ 3, gfx::BindingKind::DepthTexture, false, true }); l.drawDataBytes = 16;
			l.fragment = luminanceFS; l.debugName = "Renderer.Luminance"; luminancePipeline = device->CreatePipeline(l);
		}
		p.fragment = reduceFS; p.debugName = "Renderer.Reduce"; reducePipeline = device->CreatePipeline(p);
		p.colorFormats[0] = gfx::Format::R32F; p.drawDataBytes = 16;
		p.material = { { 0, gfx::BindingKind::Uniform, true, true, false, true }, { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Texture2D, false, true }, { 3, gfx::BindingKind::Sampler, false, true } };
		p.fragment = adaptFS; p.debugName = "Renderer.Adapt"; adaptPipeline = device->CreatePipeline(p);
		gfx::TextureDesc t; t.usage = gfx::TexColorTarget | gfx::TexSampled; t.format = gfx::Format::RG32F;
		t.width = t.height = 64; t.debugName = "Renderer.Lum64"; lum64 = device->CreateTexture(t);
		t.width = t.height = 16; t.debugName = "Renderer.Lum16"; lum16 = device->CreateTexture(t);
		t.width = t.height = 4; t.debugName = "Renderer.Lum4"; lum4 = device->CreateTexture(t);
		t.width = t.height = 1; t.debugName = "Renderer.Lum1"; lum1 = device->CreateTexture(t);
		t.format = gfx::Format::R32F; t.debugName = "Renderer.Exposure";
		const f32 one = 1.f; gfx::TextureData d; d.bytes = &one; d.size = 4;
		exposureTex[0] = device->CreateTexture(t, &d, 1); exposureTex[1] = device->CreateTexture(t, &d, 1);
	}

	void SceneRenderer::RecordBloomAndExposure(gfx::CommandList* cl, const uint32 frameOffset)
	{
		for (gfx::Pipeline p : { bloomDownPipeline, bloomUpPipeline, luminancePipeline, reducePipeline, adaptPipeline }) if (!device->PipelineReady(p)) return;
		auto group = [&](gfx::BindGroup &g, gfx::Pipeline layout, gfx::Texture source, gfx::Sampler sampler)
		{
			if (g) return g;
			gfx::Device::BindGroupEntry e[2]; e[0].binding = 1; e[0].texture = source; e[1].binding = 2; e[1].sampler = sampler;
			g = device->CreateBindGroup(layout, e, 2);
			return g;
		};
		auto pass = [&](gfx::Texture target, uint32 w, uint32 h, gfx::Pipeline pipe, gfx::BindGroup g, const void* dd, uint32 ddBytes, bool keep, const char* name, const uint32* offsets = nullptr, uint32 offsetCount = 0)
		{
			graph.Use(cl, { { target, gfx::Access::ColorTarget } });
			gfx::PassDesc pd; pd.colorCount = 1; pd.colors[0].texture = target; pd.colors[0].load = keep ? gfx::Load::Keep : gfx::Load::DontCare;
			pd.width = w; pd.height = h; pd.debugName = name;
			cl->BeginPass(pd);
			cl->SetViewport(0.f, 0.f, (f32)w, (f32)h);
			cl->SetPipeline(pipe);
			if (dd) cl->DrawData(dd, ddBytes);
			cl->SetBindGroup(g, offsets, offsetCount);
			cl->Draw(3, 1, 0, 0);
			cl->EndPass();
			graph.Use(cl, { { target, gfx::Access::ShaderRead } });
		};
		// Bloom: down the chain, then back up it, each level adding the one below
		for (uint32 i = 0; i < kBloomLevels; i++)
		{
			const gfx::Texture source = i == 0 ? picture : bloom[i - 1];
			const uint32 sw = i == 0 ? outputWidth : bloomW[i - 1], sh = i == 0 ? outputHeight : bloomH[i - 1];
			const f32 dd[8] = { 1.f / sw, 1.f / sh, 0.f, 0.f, i == 0 ? 1.f : 0.f, 0.f, 0.f, 0.f };
			pass(bloom[i], bloomW[i], bloomH[i], bloomDownPipeline, group(bloomDownGroup[i], bloomDownPipeline, source, linearClamp), dd, 32, false, "BloomDown");
		}
		for (int i = (int)kBloomLevels - 2; i >= 0; i--)
		{
			const f32 dd[8] = { 1.f / bloomW[i + 1], 1.f / bloomH[i + 1], 0.f, 0.f, 1.f, 0.f, 0.f, 0.f };
			pass(bloom[i], bloomW[i], bloomH[i], bloomUpPipeline, group(bloomUpGroup[i], bloomUpPipeline, bloom[i + 1], linearClamp), dd, 32, true, "BloomUp");
		}
		// Exposure: the scene's log luminance at 64x64, summed down to 1x1, then adapted from the last frame's
		if (!lumGroup)
		{
			gfx::Device::BindGroupEntry e[3]; e[0].binding = 1; e[0].texture = hdr; e[1].binding = 2; e[1].sampler = linearClamp; e[2].binding = 3; e[2].texture = prepassDepth;
			lumGroup = device->CreateBindGroup(luminancePipeline, e, 3);
		}
		const f32 meter[4] = { meterBackground ? 1.f : 0.f, 0.f, 0.f, 0.f };
		pass(lum64, 64, 64, luminancePipeline, lumGroup, meter, 16, false, "Luminance");
		pass(lum16, 16, 16, reducePipeline, group(reduceGroup[0], reducePipeline, lum64, pointClamp), nullptr, 0, false, "Reduce");
		pass(lum4, 4, 4, reducePipeline, group(reduceGroup[1], reducePipeline, lum16, pointClamp), nullptr, 0, false, "Reduce");
		pass(lum1, 1, 1, reducePipeline, group(reduceGroup[2], reducePipeline, lum4, pointClamp), nullptr, 0, false, "Reduce");
		const uint32 cur = (uint32)(frameNumber & 1), prev = cur ^ 1;
		if (!adaptGroup[cur])
		{
			gfx::Device::BindGroupEntry e[4];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = lum1; e[2].binding = 2; e[2].texture = exposureTex[prev]; e[3].binding = 3; e[3].sampler = pointClamp;
			adaptGroup[cur] = device->CreateBindGroup(adaptPipeline, e, 4);
		}
		const f32 ap[4] = { exposureEV, autoExposure ? 1.f : 0.f, std::pow(2.f, exposureEV), exposureRestart ? 1.f : 0.f };
		exposureRestart = false;
		pass(exposureTex[cur], 1, 1, adaptPipeline, adaptGroup[cur], ap, 16, false, "Adapt", &frameOffset, 1);
	}

	bool SceneRenderer::PrepareSunShadow(SceneGraph* scene, const View &view, const uint32 slot)
	{
		ShadowData sd = {};
		sunShadowOn = false;
		// The clouds' shadow map: where it lies and how dark (none until it is made)
		sd.cloudGrid[0] = cloudGrid[0]; sd.cloudGrid[1] = cloudGrid[1]; sd.cloudGrid[2] = 1.f / cloudGrid[2]; sd.cloudGrid[3] = cloudShadowOn ? cloudShadow : 0.f;
		// (the reflections: only once there is a last frame's picture to find things in)
		sd.rain[0] = (f32)rainTile;
		// (a deck of overcast all but puts the sun out, wherever its gaps are: the sun on surfaces dimmed by the cover - the
		// clouds' shadow map shapes it, this is the deck's whole weight - and only where there are clouds and a sky)
		{
			const f32 deck = std::min(std::max((cloudCover - 0.6f) / 0.35f, 0.f), 1.f);
			sd.rain[1] = skyOn ? 1.f - 0.85f * deck * deck * (3.f - 2.f * deck) : 1.f;
		}
		// (2: the rays' - uPreviousColor is theirs; 1: the screen's - also where traced ones were asked for and refused)
		sd.reflections[0] = RtReflections() && rtReflectTex ? 2.f : ScreenReflections() && hdrPreviousMade ? 1.f : 0.f;
		sd.reflections[1] = reflectionReach; sd.reflections[2] = (f32)reflectionSteps; sd.reflections[3] = (f32)(frameNumber % 1024);
		// The atlas, and the point and spot lights' tiles (WriteLights chose them)
		sd.atlas[0] = 1.f / kShadowAtlasW; sd.atlas[1] = 1.f / kShadowAtlasH; sd.atlas[2] = (f32)kLocalTile / kShadowAtlasW; sd.atlas[3] = (f32)kLocalTile / kShadowAtlasH;
		for (uint32 t = 0; t < localTiles.size(); t++)
		{
			sd.localRect[t][0] = (f32)(kShadowAtlasW / 2 + (t % kLocalTilesAcross) * kLocalTile) / kShadowAtlasW;
			sd.localRect[t][1] = (f32)((t / kLocalTilesAcross) * kLocalTile) / kShadowAtlasH;
			sd.localRect[t][2] = localTiles[t].tanHalf;
			memcpy(sd.local[t], localTiles[t].viewProjection.m, sizeof(sd.local[t]));
			device->Upload(cascadeData, (slot * kShadowSlots + kCascades + t) * 256, localTiles[t].viewProjection.m, sizeof(localTiles[t].viewProjection.m));
		}
		DirectionalLight* sun = nullptr;
		if (scene)
			for (IComponent* c : scene->GetLights())
				if (c && c->GetComponentType() == ComponentType::DirectionalLight) { sun = static_cast<DirectionalLight*>(c); break; }
		const Vec3 forward = (view.cameraWorld * Vec4(0.f, 0.f, -1.f, 0.f)).xyz().normalize();
		sd.cameraForward[0] = forward.x; sd.cameraForward[1] = forward.y; sd.cameraForward[2] = forward.z;
		if (sun && sun->IsCastingShadows() && sun->GetOwner())
		{
			Vec3 toLight;
			sun->GetLightRadianceNow(&toLight);
			const Vec3 l = toLight.normalize() * -1.f;                       // (the way the light goes)
			const f32 shadowDistance = std::min(std::max(sun->GetShadowFar(), 10.f), 1000.f);
			// (a shadow falls at most this many of its caster's sizes beyond it: the sun's slant - long when it is low)
			shadowStretch = std::sqrt(std::max(0.f, 1.f - l.y * l.y)) / std::max(std::fabs(l.y), 0.15f);
			shadowEye = view.cameraWorld.GetTranslation(); shadowForward = forward;
			const f32 n = std::max(view.nearZ, 0.05f);
			const f32 aspect = view.height ? (f32)view.width / (f32)view.height : 1.f;
			const f32 tanY = std::tan(view.fovY * (f32)PI / 360.f), tanX = tanY * aspect;
			const Vec3 eye = view.cameraWorld.GetTranslation();
			const Vec3 right = (view.cameraWorld * Vec4(1.f, 0.f, 0.f, 0.f)).xyz().normalize(), up = (view.cameraWorld * Vec4(0.f, 1.f, 0.f, 0.f)).xyz().normalize();

			// The light's basis (a fixed one for a sun: its cascades only move by whole texels)
			const Vec3 helper = std::fabs(l.y) > 0.99f ? Vec3(1.f, 0.f, 0.f) : Vec3(0.f, 1.f, 0.f);
			const Vec3 lx = helper.cross(l).normalize(), ly = l.cross(lx).normalize();

			// How far toward the light the casters reach: every caster's sphere along the light's way
			f32 casterMin = 1e30f, casterMax = -1e30f;
			for (const DrawItem &it : items)
			{
				if (!it.caster) continue;
				const f32 d = it.center.dotProduct(l);
				casterMin = std::min(casterMin, d - it.radius); casterMax = std::max(casterMax, d + it.radius);
			}
			if (casterMin > casterMax) { device->Upload(shadowData, slot * kShadowDataSlot, &sd, sizeof(sd)); return false; }

			f32 prev = n;
			for (uint32 c = 0; c < kCascades; c++)
			{
				// Practical split: between logarithmic and uniform
				const f32 i = (f32)(c + 1) / kCascades;
				const f32 split = 0.8f * n * std::pow(shadowDistance / n, i) + 0.2f * (n + (shadowDistance - n) * i);
				// The slice's bounding sphere - the same size whatever way the camera turns (no shimmer)
				const f32 mid = (prev + split) * 0.5f;
				const Vec3 center = eye + forward * mid;
				f32 radius = 0.f;
				const f32 ds[2] = { prev, split };
				for (int k = 0; k < 2; k++)
					for (int sxi = -1; sxi <= 1; sxi += 2)
						for (int syi = -1; syi <= 1; syi += 2)
						{
							// (orthographic: the same width at every distance)
							const f32 sx = view.orthographic ? view.orthoHeight * 0.5f * aspect : tanX * ds[k], sy = view.orthographic ? view.orthoHeight * 0.5f : tanY * ds[k];
							const Vec3 corner = eye + forward * ds[k] + right * (sxi * sx) + up * (syi * sy);
							radius = std::max(radius, (corner - center).magnitude());
						}
				radius = std::ceil(radius * 16.f) / 16.f;
				// Its centre in the light's plane, snapped to whole texels
				const f32 texel = 2.f * radius / kCascadeSize;
				f32 cx = center.dotProduct(lx), cy = center.dotProduct(ly);
				cx = std::floor(cx / texel) * texel; cy = std::floor(cy / texel) * texel;
				// Light view: x along lx, y along ly, looking down l; depth from the nearest caster to the farthest
				const f32 nearD = casterMin - 1.f, farD = std::max(casterMax, center.dotProduct(l) + radius) + 1.f;
				Matrix v;
				v.m[0] = lx.x; v.m[4] = lx.y; v.m[8] = lx.z;  v.m[12] = -cx;
				v.m[1] = ly.x; v.m[5] = ly.y; v.m[9] = ly.z;  v.m[13] = -cy;
				v.m[2] = l.x;  v.m[6] = l.y;  v.m[10] = l.z;  v.m[14] = 0.f;      // (z: distance along the light)
				v.m[3] = 0.f;  v.m[7] = 0.f;  v.m[11] = 0.f;  v.m[15] = 1.f;
				// Reversed Z ortho: depth 1 nearest the light, 0 farthest
				Matrix p;
				for (int k = 0; k < 16; k++) p.m[k] = 0.f;
				p.m[0] = 1.f / radius; p.m[5] = 1.f / radius;
				p.m[10] = -1.f / (farD - nearD); p.m[14] = farD / (farD - nearD);
				p.m[15] = 1.f;
				cascadeMatrix[c] = p * v;
				cascadeRadius[c] = radius;
				memcpy(sd.cascades[c], cascadeMatrix[c].m, sizeof(sd.cascades[c]));
				sd.splits[c] = split;
				sd.texelWorld[c] = texel;
				cascadeNear[c] = prev; cascadeFar[c] = split;
				prev = split;
			}
			sd.params[0] = 1.f / kShadowAtlasW; sd.params[1] = 1.f; sd.params[2] = shadowDistance; sd.params[3] = 1.5f;
			sunShadowOn = true;
			// Each cascade's matrix for the shadow pass, in the frame's slots
			for (uint32 c = 0; c < kCascades; c++) device->Upload(cascadeData, (slot * kShadowSlots + c) * 256, cascadeMatrix[c].m, sizeof(cascadeMatrix[c].m));
		}
		device->Upload(shadowData, slot * kShadowDataSlot, &sd, sizeof(sd));
		return sunShadowOn;
	}

	void SceneRenderer::RecordSunShadow(gfx::CommandList* cl, SceneGraph* scene, const uint32 slot)
	{
		graph.Use(cl, { { shadowAtlas, gfx::Access::DepthTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 0;
		pass.depth.texture = shadowAtlas; pass.depth.clear = 0.f; pass.depth.store = gfx::StoreOp::Store;
		pass.width = kShadowAtlasW; pass.height = kShadowAtlasH; pass.debugName = "Shadows";
		cl->BeginPass(pass);
		if ((sunShadowOn || !localTiles.empty()) && device->PipelineReady(shadowPipeline))
		{
			if (!shadowGroup)
			{
				gfx::Device::BindGroupEntry e[2]; e[0].binding = 0; e[0].buffer = cascadeData; e[0].size = 64;
				e[1].binding = 3; e[1].buffer = bonePalette;
				shadowGroup = device->CreateBindGroup(shadowPipeline, e, 2);
			}
			// (kinds: 1 instanced, 2 alpha-cut, 4 skinned - instanced and skinned never both)
			const bool ready[8] = { true, device->PipelineReady(shadowInstPipeline), device->PipelineReady(shadowAlphaPipeline), device->PipelineReady(shadowInstAlphaPipeline),
				device->PipelineReady(shadowSkinPipeline), false, device->PipelineReady(shadowSkinAlphaPipeline), false };
			const gfx::Pipeline pipes[8] = { shadowPipeline, shadowInstPipeline, shadowAlphaPipeline, shadowInstAlphaPipeline, shadowSkinPipeline, gfx::Pipeline(), shadowSkinAlphaPipeline, gfx::Pipeline() };
			// (the sun's cascades, then each local tile: one pass, a viewport each)
			const uint32 firstPass = sunShadowOn ? 0 : kCascades;
			for (uint32 c = firstPass; c < kCascades + (uint32)localTiles.size(); c++)
			{
				const bool local = c >= kCascades;
				const uint32 t = c - kCascades;
				if (!local)
				{
					cl->SetViewport((f32)((c & 1) * kCascadeSize), (f32)((c >> 1) * kCascadeSize), (f32)kCascadeSize, (f32)kCascadeSize);
					cl->SetScissor((int32_t)((c & 1) * kCascadeSize), (int32_t)((c >> 1) * kCascadeSize), kCascadeSize, kCascadeSize);
				}
				else
				{
					const uint32 x = kShadowAtlasW / 2 + (t % kLocalTilesAcross) * kLocalTile, y = (t / kLocalTilesAcross) * kLocalTile;
					cl->SetViewport((f32)x, (f32)y, (f32)kLocalTile, (f32)kLocalTile);
					cl->SetScissor((int32_t)x, (int32_t)y, kLocalTile, kLocalTile);
				}
				const uint32 offset = (slot * kShadowSlots + c) * 256;
				int boundKind = -1; gfx::BindGroup boundGroup;
				// (the casters this cascade or tile holds, in the casters' order)
				std::vector<uint32> visible;
				visible.reserve(casterOrder.size());
				for (uint32 ci : casterOrder)
				{
					const DrawItem &it = items[ci];
					const int kind = (it.instances ? 1 : 0) + (it.alphaCut ? 2 : 0) + (it.skinned ? 4 : 0);
					if (!ready[kind]) continue;
					if (local)
					{
						// (only what this tile's frustum can hold: within the light's reach, inside its four sides)
						const LocalTile &lt = localTiles[t];
						const Vec4 vc = lt.view * Vec4(it.center, 1.f);
						const f32 depth = -vc.z, r = it.radius, slant = r * std::sqrt(1.f + lt.tanHalf * lt.tanHalf);
						if (depth + r < lt.nearZ || depth - r > lt.farZ) continue;
						if (lt.ortho) { if (std::fabs(vc.x) - r > lt.halfSize || std::fabs(vc.y) - r > lt.halfSize) continue; }
						else if (std::fabs(vc.x) - slant > depth * lt.tanHalf || std::fabs(vc.y) - slant > depth * lt.tanHalf) continue;
					}
					else
					{
						// (only into the cascades whose slice its shadow can reach - a blade of grass into the one it stands in)
						const f32 along = (it.center - shadowEye).dotProduct(shadowForward), reach = it.radius * (1.f + shadowStretch);
						if (along + reach < cascadeNear[c] || along - reach > cascadeFar[c]) continue;
						// (and only what this cascade's square can hold)
						const Vec4 lc = cascadeMatrix[c] * Vec4(it.center, 1.f);
						const f32 rn = it.radius / cascadeRadius[c];
						if (lc.x + rn < -1.f || lc.x - rn > 1.f || lc.y + rn < -1.f || lc.y - rn > 1.f) continue;
					}
					visible.push_back(ci);
				}
				for (size_t k = 0; k < visible.size();)
				{
					const DrawItem &it = items[visible[k]];
					const int kind = (it.instances ? 1 : 0) + (it.alphaCut ? 2 : 0) + (it.skinned ? 4 : 0);
					// (the same mesh - and alpha map - one after another: one instanced draw, where its pipeline is ready; a
					// skinned one alone - its pose is its own)
					size_t e = k + 1;
					if (!it.instances && !it.skinned && ready[kind | 1])
						while (e < visible.size())
						{
							const DrawItem &n = items[visible[e]];
							if (n.gpu != it.gpu || n.instances || n.alphaCut != it.alphaCut || (it.alphaCut && !(n.baseMap == it.baseMap)) || !SameWind(n.material, it.material)) break;
							e++;
						}
					const uint32 count = (uint32)(e - k);
					uint64 batchAt = 0;
					const bool batch = count > 1 && BatchWrite(visible, (uint32)k, count, batchAt);
					const uint32 drawn = batch ? 1 : count;
					for (uint32 d = 0; d < drawn; d++)
					{
						const DrawItem &di = items[visible[k + d]];
						const int drawKind = batch ? (kind | 1) : (di.instances ? 1 : 0) + (di.alphaCut ? 2 : 0) + (di.skinned ? 4 : 0);
						gfx::BindGroup g = shadowGroup;
						if (di.alphaCut)
						{
							gfx::BindGroup &a = shadowAlphaGroups[di.baseMap.id];
							if (!a)
							{
								gfx::Device::BindGroupEntry en[4];
								en[0].binding = 0; en[0].buffer = cascadeData; en[0].size = 64;
								en[1].binding = 1; en[1].texture = di.baseMap;
								en[2].binding = 2; en[2].sampler = materialSampler;
								en[3].binding = 3; en[3].buffer = bonePalette;
								a = device->CreateBindGroup(shadowAlphaPipeline, en, 4);
							}
							g = a;
						}
						if (drawKind != boundKind) { cl->SetPipeline(pipes[drawKind]); boundKind = drawKind; boundGroup = gfx::BindGroup(); }
						if (!(g == boundGroup)) { cl->SetBindGroup(g, &offset, 1); boundGroup = g; }
						ShadowDrawData dd = {};
						const Matrix model = batch ? Matrix() : di.model;
						memcpy(dd.model, model.m, sizeof(dd.model));
						if (di.alphaCut)
						{
							const Vec2 uvScale = di.material->GetUVScale(), uvOffset = di.material->GetUVOffset();
							dd.uvTransform[0] = uvScale.x; dd.uvTransform[1] = uvScale.y; dd.uvTransform[2] = uvOffset.x; dd.uvTransform[3] = uvOffset.y;
							dd.alpha[0] = di.material->GetAlphaCutoff();
						}
						dd.skin[0] = (f32)di.boneBase; dd.skin[1] = drawSeconds;
						const Vec4 wind = WindOf(di.material);
						dd.wind[0] = wind.x; dd.wind[1] = wind.y; dd.wind[2] = wind.z; dd.wind[3] = wind.w;
						if (!batch) { dd.shape[0] = di.shape[0]; dd.shape[1] = di.shape[1]; }
						cl->DrawData(&dd, sizeof(dd));
						if (!batch) { DrawItemGeometry(cl, di); continue; }
						cl->SetVertexBuffer(0, di.gpu->vertices, 0);
						cl->SetVertexBuffer(1, batchRing, batchAt);
						cl->SetIndexBuffer(di.gpu->indices, 0, true);
						cl->DrawIndexed(di.gpu->indexCount, count, 0, 0, 0);
					}
					k = e;
				}
			}
		}
		cl->EndPass();
		graph.Use(cl, { { shadowAtlas, gfx::Access::ShaderRead } });
	}

	SceneRenderer::InstancesGpu* SceneRenderer::InstancesFor(RenderingInstancedComponent* rc, MeshGpu* mesh)
	{
		InstancesGpu &g = instanceBuffers[rc];
		if (g.buffer && g.revision == rc->instanceRevision && g.count == rc->transform.size()) return &g;
		if (rc->transform.empty()) return nullptr;     // (instances the GPU fills - not read back here)
		const bool tinted = rc->instanceColor.size() == rc->transform.size();
		std::vector<Instance> data(rc->transform.size());
		Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
		f32 scale = 0.f;
		for (size_t i = 0; i < data.size(); i++)
		{
			const Matrix &m = rc->transform[i];
			memcpy(data[i].model, m.m, sizeof(data[i].model));
			const Vec4 t = tinted ? rc->instanceColor[i] : Vec4(1.f, 1.f, 1.f, 1.f);
			data[i].tint[0] = t.x; data[i].tint[1] = t.y; data[i].tint[2] = t.z; data[i].tint[3] = t.w;
			const Vec3 p = m.GetTranslation();
			lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)); hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
			scale = std::max(scale, std::max(Vec3(m.m[0], m.m[1], m.m[2]).magnitude(), std::max(Vec3(m.m[4], m.m[5], m.m[6]).magnitude(), Vec3(m.m[8], m.m[9], m.m[10]).magnitude())));
		}
		// (the sphere round every instance: their positions' box, grown by the mesh's own sphere at the largest scale)
		g.center = (lo + hi) * 0.5f;
		g.radius = (hi - g.center).magnitude() + (mesh->center.magnitude() + mesh->radius) * scale;
		if (g.buffer) device->Destroy(g.buffer);
		gfx::BufferDesc b; b.size = data.size() * sizeof(Instance); b.usage = gfx::BufVertex; b.debugName = "Renderer.Instances";
		g.buffer = device->CreateBuffer(b, data.data());
		g.revision = rc->instanceRevision; g.count = (uint32)data.size();
		return &g;
	}

	void SceneRenderer::ForgetScene()
	{
		for (std::unordered_map<IGeometry*, MeshGpu>::iterator i = meshes.begin(); i != meshes.end(); ++i)
			for (gfx::Buffer b : { i->second.vertices, i->second.indices, i->second.colors, i->second.skin }) if (b) device->Destroy(b);
		meshes.clear();
		for (std::map<std::pair<Texture*, bool>, TextureGpu>::iterator i = textures.begin(); i != textures.end(); ++i) if (i->second.texture) device->Destroy(i->second.texture);
		textures.clear();
		for (std::map<RenderingInstancedComponent*, InstancesGpu>::iterator i = instanceBuffers.begin(); i != instanceBuffers.end(); ++i) if (i->second.buffer) device->Destroy(i->second.buffer);
		instanceBuffers.clear();
		for (std::map<CustomShaderMaterial*, CustomGroup>* m : { &customGroups, &customPrepassGroups })
		{
			for (std::map<CustomShaderMaterial*, CustomGroup>::iterator i = m->begin(); i != m->end(); ++i) if (i->second.group) device->Destroy(i->second.group);
			m->clear();
		}
		for (std::map<uint32_t, gfx::BindGroup>* m : { &uiGroups, &particleGroups, &shadowAlphaGroups, &depthAlphaGroups })
		{
			for (std::map<uint32_t, gfx::BindGroup>::iterator i = m->begin(); i != m->end(); ++i) if (i->second) device->Destroy(i->second);
			m->clear();
		}
		for (std::unordered_map<ParticleSystem*, GpuParticles>::iterator i = gpuParticles.begin(); i != gpuParticles.end(); ++i) ForgetGpuParticles(i->second);
		gpuParticles.clear();
		ForgetRayTracing();
		for (std::map<std::array<uint32_t, 5>, gfx::BindGroup>::iterator i = surfaceGroups.begin(); i != surfaceGroups.end(); ++i) if (i->second) device->Destroy(i->second);
		surfaceGroups.clear();
		if (skyboxGroup) { device->Destroy(skyboxGroup); skyboxGroup = gfx::BindGroup(); }
		if (skyboxTableGroup) { device->Destroy(skyboxTableGroup); skyboxTableGroup = gfx::BindGroup(); }
		if (skyboxTex) { device->Destroy(skyboxTex); skyboxTex = gfx::Texture(); }
		skyboxMadeFrom = nullptr; skyboxSource = nullptr;
		meshDraws.clear(); surfaceRefs.clear();
		items.clear(); opaqueOrder.clear(); transparentOrder.clear(); worldTextOrder.clear(); casterOrder.clear();
		// (a new place: nothing of the last picture blended into the first ones of the new)
		taaHistory = false; cloudHistory = false; hdrPreviousMade = false;
	}

	void SceneRenderer::BatchFrameStart()
	{
		// (the ring as big as the last frame wanted, and some: made anew at a frame's start, never in its middle)
		if (batchWanted > batchRingBytes)
		{
			batchRingBytes = std::max<uint64>(batchWanted + batchWanted / 2, 1u << 20);
			if (batchRing) device->Destroy(batchRing);
			gfx::BufferDesc b; b.size = batchRingBytes * kFramesInFlight; b.usage = gfx::BufVertex; b.memory = gfx::Memory::Upload; b.debugName = "Renderer.Batches";
			batchRing = device->CreateBuffer(b);
		}
		batchCursor = 0; batchWanted = 0;
	}

	bool SceneRenderer::BatchWrite(const std::vector<uint32> &order, const uint32 first, const uint32 count, uint64 &offset)
	{
		const uint64 bytes = (uint64)count * sizeof(Instance);
		batchWanted += bytes;
		if (!batchRing || batchCursor + bytes > batchRingBytes) return false;
		batchScratch.resize((size_t)bytes);
		Instance* out = (Instance*)batchScratch.data();
		for (uint32 k = 0; k < count; k++, out++)
		{
			memcpy(out->model, items[order[first + k]].model.m, sizeof(out->model));
			out->tint[0] = out->tint[1] = out->tint[2] = out->tint[3] = 1.f;
		}
		offset = (uint64)batchSlot * batchRingBytes + batchCursor;
		device->Upload(batchRing, offset, batchScratch.data(), bytes);
		batchCursor += bytes;
		return true;
	}

	void SceneRenderer::DrawItemGeometry(gfx::CommandList* cl, const DrawItem &it)
	{
		cl->SetVertexBuffer(0, it.gpu->vertices, 0);
		if (it.instances) cl->SetVertexBuffer(1, it.instances, 0);
		if (it.skinned) cl->SetVertexBuffer(2, it.gpu->skin, 0);
		if (it.vertexColor && it.mesh) cl->SetVertexBuffer(3, ColorsFor(it.mesh->Geometry, it.gpu), 0);
		cl->SetIndexBuffer(it.gpu->indices, 0, true);
		cl->DrawIndexed(it.gpu->indexCount, it.instances ? it.instanceCount : 1, 0, 0, 0);
	}

	gfx::Buffer SceneRenderer::ColorsFor(IGeometry* g, MeshGpu* mesh)
	{
		if (mesh->colors) return mesh->colors;
		std::vector<f32> c((size_t)mesh->vertexCount * 4, 1.f);
		const VertexAttribute* a = FindAttribute(g, "aColor");
		if (a && a->DataLength == mesh->vertexCount && a->Data.size() >= (size_t)mesh->vertexCount * sizeof(Vec4)) memcpy(c.data(), a->Data.data(), c.size() * 4);
		else if (a)
			for (AttributeArray* aa : g->Attributes)
			{
				AttributeBuffer* ab = dynamic_cast<AttributeBuffer*>(aa);
				if (!ab || !ab->Buffer || ab->BufferOffset == 0 || ab->Buffer->CpuData.size() < (size_t)ab->BufferOffset * mesh->vertexCount) continue;
				bool here = false; for (VertexAttribute* va : ab->Attributes) if (va == a) here = true;
				if (!here) continue;
				for (uint32 v = 0; v < mesh->vertexCount; v++) memcpy(&c[v * 4], &ab->Buffer->CpuData[(size_t)v * ab->BufferOffset + a->Offset], sizeof(Vec4));
			}
		gfx::BufferDesc b; b.size = c.size() * 4; b.usage = gfx::BufVertex; b.debugName = "Renderer.VertexColours";
		mesh->colors = device->CreateBuffer(b, c.data());
		return mesh->colors;
	}

	void SceneRenderer::DrawUIMesh(gfx::CommandList* cl, RenderingMesh* m, const Matrix &transform, const bool inWorld)
	{
		GenericShaderMaterial* gm = static_cast<GenericShaderMaterial*>(m->Material.get());
		MeshGpu* mesh = MeshFor(m->Geometry);
		if (!mesh) return;
		const uint64 options = gm->GetOptions();
		const bool text = (options & ShaderUsage::TextRendering) != 0;
		Texture* source = text ? gm->GetFontMap() : gm->GetColorMap();
		const gfx::Texture tex = TextureFor(source, !text, white);     // (a font's atlas is coverage, not colour)
		gfx::BindGroup &g = uiGroups[tex.id];
		if (!g)
		{
			gfx::Device::BindGroupEntry e[2]; e[0].binding = 1; e[0].texture = tex; e[1].binding = 2; e[1].sampler = linearClamp;
			g = device->CreateBindGroup(uiPipeline, e, 2);
		}
		cl->SetBindGroup(g);
		UIDrawData dd;
		memcpy(dd.transform, transform.m, sizeof(dd.transform));
		// (the material's colour where it has one - not a text's, whose characters carry theirs; the vertices' where
		// a batched quad carries it)
		const bool vertexColours = (options & ShaderUsage::VertexColor) != 0;
		const Vec4 c = (options & ShaderUsage::Color) && !text ? SrgbToLinear(gm->GetColor()) : Vec4(1.f, 1.f, 1.f, 1.f);
		dd.colour[0] = c.x; dd.colour[1] = c.y; dd.colour[2] = c.z; dd.colour[3] = c.w * gm->GetOpacity();
		dd.flags[0] = text ? ((options & ShaderUsage::TextSDF) ? 2.f : 1.f) : 0.f;
		dd.flags[1] = source ? 1.f : 0.f; dd.flags[2] = vertexColours ? 1.f : 0.f; dd.flags[3] = inWorld ? 1.f : 0.f;
		cl->DrawData(&dd, sizeof(dd));
		cl->SetVertexBuffer(0, mesh->vertices, 0);
		cl->SetVertexBuffer(1, ColorsFor(m->Geometry, mesh), 0);
		cl->SetIndexBuffer(mesh->indices, 0, true);
		cl->DrawIndexed(mesh->indexCount, 1, 0, 0, 0);
	}

	void SceneRenderer::RecordUI(gfx::CommandList* cl, SceneGraph* scene, const uint32 width, const uint32 height)
	{
		if (!scene || !device->PipelineReady(uiPipeline)) return;
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
		if (canvases.empty()) return;
		cl->SetPipeline(uiPipeline);
		for (UICanvas* canvas : canvases)
		{
			canvas->Solve(width, height);
			const UIRectValue &rect = canvas->GetCanvasRect();
			if (rect.width <= 0.f || rect.height <= 0.f) continue;
			// (the canvas's units over the whole window: x right, y up from its top edge's -height)
			Projection ortho;
			ortho.Ortho(0.f, rect.width, -rect.height, 0.f, -1000.f, 1000.f);
			const Matrix projection = ortho.m;
			const std::vector<RenderingMesh*> &list = canvas->GetBatchedDrawList();
			const std::vector<UIRectValue> &clips = canvas->GetBatchedDrawClips();
			const f32 ppu = canvas->GetPixelsPerUnit();
			for (size_t i = 0; i < list.size(); i++)
			{
				RenderingMesh* m = list[i];
				if (!m || !m->renderingComponent || !m->renderingComponent->IsActive() || !m->Active || !m->renderingComponent->GetOwner()) continue;
				GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(m->Material.get());
				if (!gm || !m->Geometry) continue;
				// (clipped to its scroll view or mask, in pixels from the window's top left)
				if (i < clips.size())
				{
					const UIRectValue &c = clips[i];
					if (c.width <= 0.f || c.height <= 0.f) continue;
					const int32_t x = (int32_t)std::max(0.f, c.x * ppu), y = (int32_t)std::max(0.f, c.y * ppu);
					cl->SetScissor(x, y, (uint32_t)std::max(0.f, std::min(c.width * ppu, (f32)width - x)), (uint32_t)std::max(0.f, std::min(c.height * ppu, (f32)height - y)));
				}
				else cl->SetScissor(0, 0, width, height);
				DrawUIMesh(cl, m, projection * m->renderingComponent->GetOwner()->GetDrawWorld() * m->Pivot, false);
			}
		}
		cl->SetScissor(0, 0, width, height);
	}

	namespace
	{
		// Tileable noise for the clouds: periodic Perlin and Worley over a lattice that wraps
		f32 Fade(f32 t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }
		uint32 Hash3(int32 x, int32 y, int32 z) { uint32 h = (uint32)x * 73856093u ^ (uint32)y * 19349663u ^ (uint32)z * 83492791u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h; }
		f32 Grad(uint32 h, f32 x, f32 y, f32 z)
		{
			switch (h & 15) { case 0: return x + y; case 1: return -x + y; case 2: return x - y; case 3: return -x - y; case 4: return x + z; case 5: return -x + z;
			case 6: return x - z; case 7: return -x - z; case 8: return y + z; case 9: return -y + z; case 10: return y - z; case 11: return -y - z;
			case 12: return x + y; case 13: return -y + z; case 14: return -x + y; default: return -y - z; }
		}
		// (in 0..1; `period` lattice cells across the tile)
		f32 PerlinTiled(f32 x, f32 y, f32 z, int32 period)
		{
			const int32 xi = (int32)std::floor(x), yi = (int32)std::floor(y), zi = (int32)std::floor(z);
			const f32 xf = x - xi, yf = y - yi, zf = z - zi, u = Fade(xf), v = Fade(yf), w = Fade(zf);
			auto g = [&](int32 a, int32 b, int32 c) { return Grad(Hash3(((xi + a) % period + period) % period, ((yi + b) % period + period) % period, ((zi + c) % period + period) % period), xf - a, yf - b, zf - c); };
			const f32 x00 = g(0, 0, 0) + u * (g(1, 0, 0) - g(0, 0, 0)), x10 = g(0, 1, 0) + u * (g(1, 1, 0) - g(0, 1, 0));
			const f32 x01 = g(0, 0, 1) + u * (g(1, 0, 1) - g(0, 0, 1)), x11 = g(0, 1, 1) + u * (g(1, 1, 1) - g(0, 1, 1));
			const f32 y0 = x00 + v * (x10 - x00), y1 = x01 + v * (x11 - x01);
			return std::min(std::max((y0 + w * (y1 - y0)) * 0.5f + 0.5f, 0.f), 1.f);
		}
		// (1 at a feature point, 0 far from all: inverted distance to the nearest of one point a cell)
		f32 WorleyTiled(f32 x, f32 y, f32 z, int32 cells)
		{
			const int32 xi = (int32)std::floor(x), yi = (int32)std::floor(y), zi = (int32)std::floor(z);
			f32 best = 1e9f;
			for (int32 a = -1; a <= 1; a++) for (int32 b = -1; b <= 1; b++) for (int32 c = -1; c <= 1; c++)
			{
				const int32 cx = xi + a, cy = yi + b, cz = zi + c;
				const uint32 h = Hash3((cx % cells + cells) % cells, (cy % cells + cells) % cells, (cz % cells + cells) % cells);
				const f32 px = cx + (h & 1023) / 1023.f, py = cy + ((h >> 10) & 1023) / 1023.f, pz = cz + ((h >> 20) & 1023) / 1023.f;
				const f32 dx = px - x, dy = py - y, dz = pz - z;
				best = std::min(best, dx * dx + dy * dy + dz * dz);
			}
			return 1.f - std::min(std::sqrt(best), 1.f);
		}
	}

	void SceneRenderer::CreateClouds()
	{
		// The noise: 64^3, tiling. r: Perlin fbm cut by Worley (the clouds' shapes), gba: Worley at 4, 8 and 16 cells (wearing them away)
		const uint32 n = 64;
		std::vector<uint8> texels((size_t)n * n * n * 4);
		for (uint32 z = 0; z < n; z++)
			for (uint32 y = 0; y < n; y++)
				for (uint32 x = 0; x < n; x++)
				{
					const f32 fx = (x + 0.5f) / n, fy = (y + 0.5f) / n, fz = (z + 0.5f) / n;
					f32 perlin = 0.f, amp = 1.f, sum = 0.f;
					for (int32 o = 0, p = 4; o < 4; o++, p *= 2) { perlin += PerlinTiled(fx * p, fy * p, fz * p, p) * amp; sum += amp; amp *= 0.5f; }
					perlin /= sum;
					const f32 w4 = WorleyTiled(fx * 4, fy * 4, fz * 4, 4), w8 = WorleyTiled(fx * 8, fy * 8, fz * 8, 8), w16 = WorleyTiled(fx * 16, fy * 16, fz * 16, 16);
					const f32 worley = w4 * 0.625f + w8 * 0.25f + w16 * 0.125f;
					// (the Perlin's billows lifted by the Worley's cells: from the Worley's floor up)
					const f32 pw = worley + perlin * (1.f - worley);
					uint8* t = &texels[(((size_t)z * n + y) * n + x) * 4];
					t[0] = (uint8)std::min(pw * 255.f + 0.5f, 255.f); t[1] = (uint8)(w4 * 255.f + 0.5f); t[2] = (uint8)(w8 * 255.f + 0.5f); t[3] = (uint8)(w16 * 255.f + 0.5f);
				}
		// (the shapes' channel made even - each value as common as any other: a cover of c leaves about c of the slab's
		// middle in cloud)
		{
			std::vector<std::pair<uint8, uint32> > ranked;
			ranked.reserve((size_t)n * n * n);
			for (uint32 k = 0; k < n * n * n; k++) ranked.push_back(std::make_pair(texels[(size_t)k * 4], k));
			std::sort(ranked.begin(), ranked.end());
			for (size_t r = 0; r < ranked.size(); r++) texels[(size_t)ranked[r].second * 4] = (uint8)(r * 255 / (ranked.size() - 1));
		}
		gfx::TextureDesc d; d.kind = gfx::TextureKind::Tex3D; d.width = d.height = n; d.depthOrLayers = n; d.format = gfx::Format::RGBA8;
		d.usage = gfx::TexSampled; d.debugName = "Renderer.CloudNoise";
		std::vector<gfx::TextureData> slices(n);
		for (uint32 z = 0; z < n; z++) { slices[z].layer = z; slices[z].bytes = &texels[(size_t)z * n * n * 4]; slices[z].size = (uint64)n * n * 4; }
		cloudNoise = device->CreateTexture(d, slices.data(), n);
		gfx::SamplerDesc rs; rs.u = rs.v = rs.w = gfx::Address::Repeat; rs.mip = gfx::Filter::Nearest; rs.anisotropy = 1.f;
		repeatLinear = device->CreateSampler(rs);
		gfx::GraphicsPipelineDesc p;
		p.vertex = fullscreenVS; p.fragment = cloudsFS; p.cull = gfx::Cull::None; p.depthTest = p.depthWrite = false;
		p.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::Texture3D, false, true }, { 2, gfx::BindingKind::Sampler, false, true },
			{ 3, gfx::BindingKind::Texture2D, false, true }, { 4, gfx::BindingKind::Sampler, false, true }, { 5, gfx::BindingKind::Uniform, false, true, false, true },
			{ 6, gfx::BindingKind::Texture2D, false, true } };
		p.colorCount = 1; p.colorFormats[0] = gfx::Format::RGBA16F;
		p.debugName = "Renderer.Clouds";
		cloudsPipeline = device->CreatePipeline(p);
		p.fragment = cloudsDomeFS; p.debugName = "Renderer.CloudsDome";
		cloudsDomePipeline = device->CreatePipeline(p);
		p.fragment = cloudShadowFS; p.colorFormats[0] = gfx::Format::R16F; p.debugName = "Renderer.CloudShadow";
		p.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::Texture3D, false, true }, { 2, gfx::BindingKind::Sampler, false, true },
			{ 5, gfx::BindingKind::Uniform, false, true, false, true } };
		cloudShadowPipeline = device->CreatePipeline(p);
		gfx::TextureDesc st; st.width = st.height = 256; st.format = gfx::Format::R16F; st.usage = gfx::TexColorTarget | gfx::TexSampled; st.debugName = "Renderer.CloudShadow";
		cloudShadowTex = device->CreateTexture(st);
		// (the temporal anti-aliasing's resolve, with these: a full screen, into the HDR format)
		{
			gfx::GraphicsPipelineDesc t;
			t.vertex = fullscreenVS; t.fragment = taaFS; t.cull = gfx::Cull::None; t.depthTest = t.depthWrite = false;
			t.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true },
				{ 3, gfx::BindingKind::Texture2D, false, true }, { 4, gfx::BindingKind::DepthTexture, false, true }, { 5, gfx::BindingKind::Uniform, false, true, false, true },
				{ 6, gfx::BindingKind::Texture2D, false, true } };
			t.colorCount = 1; t.colorFormats[0] = kHdrFormat; t.debugName = "Renderer.TemporalAA";
			taaPipeline = device->CreatePipeline(t);
			// (the lens and shutter: the picture, its moves on screen, its depth - post.frag)
			gfx::GraphicsPipelineDesc po;
			po.vertex = fullscreenVS; po.fragment = postFS; po.cull = gfx::Cull::None; po.depthTest = po.depthWrite = false;
			po.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true },
				{ 3, gfx::BindingKind::Texture2D, false, true }, { 4, gfx::BindingKind::DepthTexture, false, true } };
			po.drawDataBytes = 64; po.colorCount = 1; po.colorFormats[0] = kHdrFormat; po.debugName = "Renderer.Post";
			postPipeline = device->CreatePipeline(po);
			// (the ray-traced shadows: the depth, the AO, the scene's triangles - rtshadow.frag)
			gfx::GraphicsPipelineDesc rt;
			rt.vertex = fullscreenVS; rt.fragment = rtShadowFS; rt.cull = gfx::Cull::None; rt.depthTest = rt.depthWrite = false;
			rt.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::DepthTexture, false, true }, { 2, gfx::BindingKind::Texture2D, false, true },
				{ 3, gfx::BindingKind::Sampler, false, true }, { 4, gfx::BindingKind::StorageRead, false, true }, { 5, gfx::BindingKind::StorageRead, false, true }, { 6, gfx::BindingKind::StorageRead, false, true } };
			rt.drawDataBytes = 16; rt.colorCount = 1; rt.colorFormats[0] = gfx::Format::RG8; rt.debugName = "Renderer.RTShadows";
			rtShadowPipeline = device->CreatePipeline(rt);
			gfx::GraphicsPipelineDesc rr = rt;
			rr.fragment = rtReflectFS;
			rr.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::DepthTexture, false, true }, { 3, gfx::BindingKind::Sampler, false, true },
				{ 4, gfx::BindingKind::StorageRead, false, true }, { 5, gfx::BindingKind::StorageRead, false, true }, { 6, gfx::BindingKind::StorageRead, false, true },
				{ 7, gfx::BindingKind::StorageRead, false, true }, { 9, gfx::BindingKind::Texture2D, false, true }, { 10, gfx::BindingKind::Texture2D, false, true } };
			rr.colorFormats[0] = kHdrFormat; rr.debugName = "Renderer.RTReflections";
			rtReflectPipeline = device->CreatePipeline(rr);
			// (the GPU's own ray tracing: the same, with the frame's scene where the BVH's buffers were - binding 8)
			if (rtShadowRQFS && rtReflectRQFS)
			{
				gfx::GraphicsPipelineDesc q = rt;
				q.fragment = rtShadowRQFS; q.debugName = "Renderer.RTShadows (ray queries)";
				q.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::DepthTexture, false, true }, { 2, gfx::BindingKind::Texture2D, false, true },
					{ 3, gfx::BindingKind::Sampler, false, true }, { 8, gfx::BindingKind::RayScene, false, true } };
				rtShadowRQPipeline = device->CreatePipeline(q);
				gfx::GraphicsPipelineDesc qr = rr;
				qr.fragment = rtReflectRQFS; qr.debugName = "Renderer.RTReflections (ray queries)";
				qr.material = { { 0, gfx::BindingKind::Uniform, false, true, false, true }, { 1, gfx::BindingKind::DepthTexture, false, true }, { 3, gfx::BindingKind::Sampler, false, true },
					{ 7, gfx::BindingKind::StorageRead, false, true }, { 8, gfx::BindingKind::RayScene, false, true }, { 9, gfx::BindingKind::Texture2D, false, true }, { 10, gfx::BindingKind::Texture2D, false, true } };
				rtReflectRQPipeline = device->CreatePipeline(qr);
			}
			gfx::BufferDesc b; b.size = 256 * kFramesInFlight; b.usage = gfx::BufUniform; b.memory = gfx::Memory::Upload; b.debugName = "Renderer.TemporalAA";
			taaData = device->CreateBuffer(b);
			if (const char* e = getenv("PYROS_RENDERER_TAA")) taaOn = e[0] != '0';
		}
	}

	void SceneRenderer::RecordClouds(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot)
	{
		const uint32 cur = cloudFrame & 1;
		if (!device->PipelineReady(cloudsPipeline) || !cloudTex[cur]) return;
		gfx::BindGroup &group = cloudsGroup[cur];
		if (!group)
		{
			gfx::Device::BindGroupEntry e[7];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = cloudNoise;
			e[2].binding = 2; e[2].sampler = repeatLinear;
			e[3].binding = 3; e[3].texture = skyView;
			e[4].binding = 4; e[4].sampler = linearClamp;
			e[5].binding = 5; e[5].buffer = skyData; e[5].size = 192;
			e[6].binding = 6; e[6].texture = cloudTex[cur ^ 1];      // (last frame's)
			group = device->CreateBindGroup(cloudsPipeline, e, 7);
		}
		graph.Use(cl, { { cloudTex[cur], gfx::Access::ColorTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = cloudTex[cur]; pass.colors[0].load = gfx::Load::DontCare;
		pass.width = std::max(1u, targetWidth / 2); pass.height = std::max(1u, targetHeight / 2); pass.debugName = "Clouds";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (f32)pass.width, (f32)pass.height);
		cl->SetPipeline(cloudsPipeline);
		const uint32 offsets[2] = { frameOffset, slot * 256 };
		cl->SetBindGroup(group, offsets, 2);
		cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		cloudHistory = true;
		graph.Use(cl, { { cloudTex[cur], gfx::Access::ShaderRead } });
	}

	void SceneRenderer::RecordCloudShadow(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot)
	{
		if (!device->PipelineReady(cloudShadowPipeline)) return;
		if (!cloudShadowGroup)
		{
			gfx::Device::BindGroupEntry e[4];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = cloudNoise;
			e[2].binding = 2; e[2].sampler = repeatLinear;
			e[3].binding = 5; e[3].buffer = skyData; e[3].size = 192;
			cloudShadowGroup = device->CreateBindGroup(cloudShadowPipeline, e, 4);
		}
		graph.Use(cl, { { cloudShadowTex, gfx::Access::ColorTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = cloudShadowTex; pass.colors[0].load = gfx::Load::DontCare;
		pass.width = pass.height = 256; pass.debugName = "CloudShadow";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, 256.f, 256.f);
		cl->SetPipeline(cloudShadowPipeline);
		const uint32 offsets[2] = { frameOffset, slot * 256 };
		cl->SetBindGroup(cloudShadowGroup, offsets, 2);
		cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		graph.Use(cl, { { cloudShadowTex, gfx::Access::ShaderRead } });
		cloudShadowOn = true;
	}

	void SceneRenderer::FillDrawData(const DrawItem &it, const bool batch, void* out)
	{
		DrawData &dd = *(DrawData*)out;
		dd = DrawData();
		const Matrix model = batch ? Matrix() : it.model;
		memcpy(dd.model, model.m, sizeof(dd.model));
		if (!batch) memcpy(dd.shape, it.shape, sizeof(dd.shape));
		dd.skin[0] = (f32)it.boneBase;
		const Matrix normal = model.Inverse().Transpose();      // (rows of its 3x3: column-major m[col * 4 + row])
		for (int r = 0; r < 3; r++) { dd.normalRows[r * 4 + 0] = normal.m[0 * 4 + r]; dd.normalRows[r * 4 + 1] = normal.m[1 * 4 + r]; dd.normalRows[r * 4 + 2] = normal.m[2 * 4 + r]; }
		if (it.custom)
		{
			dd.baseColor[0] = dd.baseColor[1] = dd.baseColor[2] = 1.f; dd.baseColor[3] = it.custom->GetOpacity();
			dd.surface[0] = 0.5f; dd.surface[2] = 0.5f;
			dd.uvTransform[0] = dd.uvTransform[1] = 1.f;
			return;
		}
		GenericShaderMaterial* gm = it.material;
		// (the material's colour where it says it has one - ShaderUsage::Color; a textured one without it is the map alone)
		const Vec4 albedo = (gm->GetOptions() & ShaderUsage::Color) ? SrgbToLinear(gm->GetColor()) : Vec4(1.f, 1.f, 1.f, 1.f);
		dd.baseColor[0] = albedo.x; dd.baseColor[1] = albedo.y; dd.baseColor[2] = albedo.z; dd.baseColor[3] = albedo.w * gm->GetOpacity();
		if (gm->GetOptions() & ShaderUsage::PBR)
		{
			dd.surface[0] = gm->GetRoughness(); dd.surface[1] = gm->GetMetallic();
		}
		else
		{
			// (a classic material's shininess as a roughness: the Blinn-Phong exponent s ~ 2 / a^2 - 2, a = roughness^2)
			dd.surface[0] = std::sqrt(std::sqrt(2.f / (std::max(gm->GetShininess(), 0.f) + 2.f))); dd.surface[1] = 0.f;
			// (and no glossier than its own roughness says - a leaf's 0.8: the default shininess made every classic surface
			// a 0.44, and a crown of two-sided leaf cards seen edge on was white with the sky's reflection)
			dd.surface[0] = std::max(dd.surface[0], gm->GetRoughness());
		}
		dd.surface[2] = 0.5f;
		dd.surface[3] = gm->GetNormalMap() ? 1.f : 0.f;
		// (its own light, as its flags say - surface.frag: lit by none without Diffuse, PBR or cel shading; by distance
		// alone with Lighting2D) and an additive one's colour by its cover
		{
			const uint32 o = gm->GetOptions();
			const bool lit = (o & (ShaderUsage::Diffuse | ShaderUsage::PBR | ShaderUsage::CellShading)) != 0;
			dd.skin[2] = !lit ? 1.f : (o & ShaderUsage::Lighting2D) && !(o & ShaderUsage::PBR) ? 2.f : 0.f;
			dd.skin[3] = Blended(gm) && gm->GetBlendingDFactor() == BlendFunc::One ? 1.f : 0.f;
		}
		dd.skin[1] = gm->GetDisplacementMap() ? std::max(gm->GetDisplacementHeight(), 0.f) : 0.f;     // (parallax: how deep its height map goes, in texcoord units)
		const Vec3 em = gm->GetEmission();
		dd.emission[0] = em.x; dd.emission[1] = em.y; dd.emission[2] = em.z;
		dd.emission[3] = (gm->GetOptions() & ShaderUsage::AlphaTest) ? gm->GetAlphaCutoff() : 0.f;
		const Vec2 uvScale = gm->GetUVScale(), uvOffset = gm->GetUVOffset();
		dd.uvTransform[0] = uvScale.x; dd.uvTransform[1] = uvScale.y; dd.uvTransform[2] = uvOffset.x; dd.uvTransform[3] = uvOffset.y;
		// (a plant: its sway, and how much sunlight it lets through - surface.vert, surface.frag)
		const Vec4 wind = gm->GetWind();
		dd.wind[0] = wind.x; dd.wind[1] = wind.y; dd.wind[2] = wind.z; dd.wind[3] = wind.w;
		dd.normalRows[3] = std::max(gm->GetLightThrough(), 0.f);
		dd.normalRows[7] = gm->GetSSRStrength();     // (its mirror floor's strength - surface.frag's MirrorFloor)
		// (wet - and its puddles - and the light under the skin: surface.frag)
		dd.wet[0] = gm->GetWetness(); dd.wet[1] = gm->GetPuddles(); dd.wet[2] = gm->GetPuddleSize();
		const Vec3 sss = SrgbToLinear(gm->GetSubsurfaceColor());
		dd.subsurface[0] = sss.x; dd.subsurface[1] = sss.y; dd.subsurface[2] = sss.z; dd.subsurface[3] = gm->GetSubsurfaceStrength();
		}

	void SceneRenderer::AddOverlay(const std::vector<OverlayVertex> &vertices, const bool lines, const bool depthTested)
	{
		const uint32 per = lines ? 2u : 3u;
		const uint32 count = (uint32)vertices.size() / per * per;
		if (!count) return;
		OverlayBatch b; b.first = (uint32)overlayVertices.size(); b.count = count; b.lines = lines; b.depthTested = depthTested;
		overlayVertices.insert(overlayVertices.end(), vertices.begin(), vertices.begin() + count);
		overlayBatches.push_back(b);
	}

	void SceneRenderer::RecordOverlay(gfx::CommandList* cl, const Matrix &viewProjection, const uint32 slot)
	{
		if (overlayBatches.empty() || !device->PipelineReady(overlayLinePipeline) || !device->PipelineReady(overlayTrianglePipeline)) return;
		// (this frame's vertices in its slot of the ring, grown to hold them - the old one freed when its frames are done)
		const uint64 bytes = (uint64)overlayVertices.size() * sizeof(OverlayVertex);
		if (bytes > overlayRingBytes)
		{
			if (overlayRing) device->Destroy(overlayRing);
			overlayRingBytes = std::max<uint64>(bytes * 2, 1 << 16);
			gfx::BufferDesc b; b.size = overlayRingBytes * kFramesInFlight; b.usage = gfx::BufVertex; b.memory = gfx::Memory::Upload; b.debugName = "Renderer.Overlay";
			overlayRing = device->CreateBuffer(b);
		}
		const uint64 base = overlayRingBytes * slot;
		device->Upload(overlayRing, base, overlayVertices.data(), bytes);
		if (!overlayGroup)
		{
			gfx::Device::BindGroupEntry e[2]; e[0].binding = 1; e[0].texture = prepassDepth; e[1].binding = 2; e[1].sampler = pointClamp;
			overlayGroup = device->CreateBindGroup(overlayLinePipeline, e, 2);
		}
		f32 dd[20];
		memcpy(dd, viewProjection.m, 64);
		for (const OverlayBatch &b : overlayBatches)
		{
			cl->SetPipeline(b.lines ? overlayLinePipeline : overlayTrianglePipeline);
			cl->SetBindGroup(overlayGroup, nullptr, 0);
			dd[16] = b.depthTested ? 1.f : 0.f; dd[17] = dd[18] = dd[19] = 0.f;
			cl->DrawData(dd, sizeof(dd));
			cl->SetVertexBuffer(0, overlayRing, base);
			cl->Draw(b.count, 1, b.first, 0);
		}
	}

	void SceneRenderer::RecordHalfParticles(gfx::CommandList* cl, const uint32 frameOffset)
	{
		if (particleSystems.empty() || !particleTex || !device->PipelineReady(particleCompositePipeline)) return;
		const uint32 w = std::max(1u, targetWidth / 2), h = std::max(1u, targetHeight / 2);
		graph.Use(cl, { { particleTex, gfx::Access::ColorTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = particleTex; pass.colors[0].load = gfx::Load::Clear;
		pass.colors[0].clear[0] = pass.colors[0].clear[1] = pass.colors[0].clear[2] = pass.colors[0].clear[3] = 0.f;
		pass.width = w; pass.height = h; pass.debugName = "Particles";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (f32)w, (f32)h);
		RecordParticles(cl, frameOffset, true);
		cl->EndPass();
		graph.Use(cl, { { particleTex, gfx::Access::ShaderRead } });
		// Over the picture: their light, and what is behind them dimmed by their cover
		if (!particleCompositeGroup)
		{
			gfx::Device::BindGroupEntry e[2]; e[0].binding = 1; e[0].texture = particleTex; e[1].binding = 2; e[1].sampler = linearClamp;
			particleCompositeGroup = device->CreateBindGroup(particleCompositePipeline, e, 2);
		}
		graph.Use(cl, { { hdr, gfx::Access::ColorTarget } });
		gfx::PassDesc cp;
		cp.colorCount = 1; cp.colors[0].texture = hdr; cp.colors[0].load = gfx::Load::Keep;
		cp.width = targetWidth; cp.height = targetHeight; cp.debugName = "ParticleComposite";
		cl->BeginPass(cp);
		cl->SetViewport(0.f, 0.f, (f32)targetWidth, (f32)targetHeight);
		cl->SetPipeline(particleCompositePipeline);
		cl->SetBindGroup(particleCompositeGroup, nullptr, 0);
		cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		graph.Use(cl, { { hdr, gfx::Access::ShaderRead } });
	}

	void SceneRenderer::RecordTemporalAA(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot)
	{
		const uint32 cur = taaFrame & 1;
		if (!device->PipelineReady(taaPipeline) || !taaTex[cur]) return;
		gfx::BindGroup &g = taaGroups[cur];
		if (!g)
		{
		gfx::Device::BindGroupEntry e[6];
		e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
		e[1].binding = 1; e[1].texture = hdr;
		e[2].binding = 2; e[2].sampler = linearClamp;
		e[3].binding = 3; e[3].texture = taaTex[cur ^ 1];      // (last frame's result)
		e[4].binding = 4; e[4].texture = prepassDepth;
		e[5].binding = 5; e[5].buffer = taaData; e[5].size = 208;
		gfx::Device::BindGroupEntry all[7] = { e[0], e[1], e[2], e[3], e[4], e[5], gfx::Device::BindGroupEntry() };
		all[6].binding = 6; all[6].texture = velocityTex;
		g = device->CreateBindGroup(taaPipeline, all, 7);
		}
		graph.Use(cl, { { taaTex[cur], gfx::Access::ColorTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = taaTex[cur]; pass.colors[0].load = gfx::Load::DontCare;
		pass.width = outputWidth; pass.height = outputHeight; pass.debugName = "TemporalAA";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (f32)outputWidth, (f32)outputHeight);
		cl->SetPipeline(taaPipeline);
		const uint32 offsets[2] = { frameOffset, slot * 256 };
		cl->SetBindGroup(g, offsets, 2);
		cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		// (the result back where the rest reads the picture from: bloom, exposure, the tonemap, the reflections' copy)
		graph.Use(cl, { { taaTex[cur], gfx::Access::CopySrc }, { picture, gfx::Access::CopyDst } });
		cl->CopyTexture(taaTex[cur], picture);
		graph.Use(cl, { { taaTex[cur], gfx::Access::ShaderRead }, { picture, gfx::Access::ShaderRead } });
		taaHistory = true;
		taaFrame++;
	}

	void SceneRenderer::RecordPost(gfx::CommandList* cl, const View &view, const uint32 frameOffset)
	{
		if (!device->PipelineReady(postPipeline) || !postTex) return;
		if (!postGroup)
		{
			gfx::Device::BindGroupEntry e[5];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = hdr;
			e[2].binding = 2; e[2].sampler = linearClamp;
			e[3].binding = 3; e[3].texture = velocityTex;
			e[4].binding = 4; e[4].texture = prepassDepth;
			postGroup = device->CreateBindGroup(postPipeline, e, 5);
		}
		// (where the sun is on the screen: through this frame's view)
		f32 dd[16] = {};
		if (shaftsStrength > 0.f && skyOn && lastSunDirection.w > 0.f)
		{
			const Vec3 sunDir(lastSunDirection.x, lastSunDirection.y, lastSunDirection.z);
			const Vec4 c = lastViewProjection * Vec4(view.cameraWorld.GetTranslation() + sunDir * 10000.f, 1.f);
			if (c.w > 0.f)
			{
				dd[0] = shaftsStrength; dd[1] = 0.965f; dd[2] = 0.85f;
				dd[4] = c.x / c.w * 0.5f + 0.5f; dd[5] = 0.5f - c.y / c.w * 0.5f; dd[6] = 1.f; dd[7] = sunDir.y;
			}
		}
		dd[8] = dofFocus; dd[9] = dofRange; dd[10] = dofBlur;
		dd[12] = motionShutter; dd[13] = 24.f;
		graph.Use(cl, { { postTex, gfx::Access::ColorTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = postTex; pass.colors[0].load = gfx::Load::DontCare;
		pass.width = targetWidth; pass.height = targetHeight; pass.debugName = "Post";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (f32)targetWidth, (f32)targetHeight);
		cl->SetPipeline(postPipeline);
		cl->SetBindGroup(postGroup, &frameOffset, 1);
		cl->DrawData(dd, sizeof(dd));
		cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		// (back where the rest reads the picture from)
		graph.Use(cl, { { postTex, gfx::Access::CopySrc }, { hdr, gfx::Access::CopyDst } });
		cl->CopyTexture(postTex, hdr);
		graph.Use(cl, { { postTex, gfx::Access::ShaderRead }, { hdr, gfx::Access::ShaderRead } });
	}

	void SceneRenderer::CreateParticles()
	{
		particleRingBytes = kParticleRingStart;
		gfx::BufferDesc b; b.size = particleRingBytes * kFramesInFlight; b.usage = gfx::BufVertex; b.memory = gfx::Memory::Upload; b.debugName = "Renderer.Particles";
		particleRing = device->CreateBuffer(b);
		gfx::GraphicsPipelineDesc p;
		p.vertex = particleVS; p.fragment = particleFS;
		p.material = { { 0, gfx::BindingKind::Uniform, true, true, false, true }, { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true },
			{ 3, gfx::BindingKind::DepthTexture, false, true }, { 4, gfx::BindingKind::Texture2D, false, true } };
		p.drawDataBytes = 80;
		p.bindings = { { 0, 32, true } };
		p.attributes = { { 0, 0, gfx::Format::RGBA32F, 0 }, { 1, 0, gfx::Format::RGBA32F, 16 } };
		p.cull = gfx::Cull::None; p.depthTest = true; p.depthWrite = false; p.depthCompare = gfx::Compare::GreaterEqual;
		p.colorCount = 1; p.colorFormats[0] = kHdrFormat; p.depthFormat = kDepthFormat; p.samples = kSceneSamples;
		p.blend[0] = gfx::Blend::PremultipliedAlpha; p.debugName = "Renderer.Particles";
		particleAlphaPipeline = device->CreatePipeline(p);
		p.blend[0] = gfx::Blend::Additive; p.debugName = "Renderer.Particles (additive)";
		particleAddPipeline = device->CreatePipeline(p);
		// (half resolution: no depth target, no multisampling - the prepass's depth hides them; colour and cover kept)
		p.fragment = particleHalfFS; p.depthTest = false; p.depthFormat = gfx::Format::Unknown; p.samples = 1; p.colorFormats[0] = gfx::Format::RGBA16F;
		p.blend[0] = gfx::Blend::PremultipliedAlpha; p.debugName = "Renderer.Particles (half)";
		particleHalfAlphaPipeline = device->CreatePipeline(p);
		p.blend[0] = gfx::Blend::Additive; p.debugName = "Renderer.Particles (half, additive)";
		particleHalfAddPipeline = device->CreatePipeline(p);
		gfx::GraphicsPipelineDesc c;
		c.vertex = fullscreenVS; c.fragment = particleCompositeFS; c.cull = gfx::Cull::None; c.depthTest = c.depthWrite = false;
		c.material = { { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true } };
		c.colorCount = 1; c.colorFormats[0] = kHdrFormat; c.blend[0] = gfx::Blend::PremultipliedAlpha; c.debugName = "Renderer.ParticleComposite";
		particleCompositePipeline = device->CreatePipeline(c);
		// (the systems simulated on the GPU: their step, a thread a particle - particle_sim.comp)
		{
			gfx::ComputePipelineDesc cp;
			cp.compute = particleSimCS;
			gfx::BindingDesc state = { 1, gfx::BindingKind::StorageReadWrite, false, false, true }, stream = { 2, gfx::BindingKind::StorageReadWrite, false, false, true };
			cp.material = { state, stream };
			cp.drawDataBytes = 96; cp.debugName = "Renderer.ParticleSim";
			particleSimPipeline = device->CreatePipeline(cp);
		}
		// (the skybox: behind everything - only where the depth is still the clear's far, reversed Z 0)
		gfx::GraphicsPipelineDesc k;
		k.vertex = fullscreenVS; k.fragment = skyboxFS; k.cull = gfx::Cull::None;
		k.material = { { 0, gfx::BindingKind::Uniform, true, true, false, true }, { 1, gfx::BindingKind::TextureCube, false, true }, { 2, gfx::BindingKind::Sampler, false, true } };
		k.colorCount = 1; k.colorFormats[0] = kHdrFormat; k.depthFormat = kDepthFormat; k.depthTest = true; k.depthWrite = false; k.depthCompare = gfx::Compare::GreaterEqual;
		k.samples = kSceneSamples; k.debugName = "Renderer.Skybox";
		skyboxPipeline = device->CreatePipeline(k);
		k.fragment = skyboxTableFS; k.depthFormat = gfx::Format::Unknown; k.depthTest = false; k.samples = 1; k.colorFormats[0] = gfx::Format::RGBA16F;
		k.debugName = "Renderer.SkyboxTable";
		skyboxTablePipeline = device->CreatePipeline(k);
		// (the overlay: over the tonemapped picture - the backbuffer's format, or a texture made with it)
		gfx::GraphicsPipelineDesc o;
		o.vertex = overlayVS; o.fragment = overlayFS;
		o.material = { { 1, gfx::BindingKind::DepthTexture, false, true }, { 2, gfx::BindingKind::Sampler, false, true } };
		o.drawDataBytes = 80;
		o.bindings = { { 0, (uint16_t)sizeof(OverlayVertex), false } };
		o.attributes = { { 0, 0, gfx::Format::RGB32F, 0 }, { 1, 0, gfx::Format::RGBA32F, 12 } };
		o.cull = gfx::Cull::None; o.depthTest = false; o.depthWrite = false;
		o.colorCount = 1; o.colorFormats[0] = gfx::Format::Unknown; o.blend[0] = gfx::Blend::Alpha;
		o.topology = gfx::Topology::Lines; o.debugName = "Renderer.Overlay (lines)";
		overlayLinePipeline = device->CreatePipeline(o);
		o.topology = gfx::Topology::Triangles; o.debugName = "Renderer.Overlay (triangles)";
		overlayTrianglePipeline = device->CreatePipeline(o);
		if (const char* e = getenv("PYROS_RENDERER_HALF_PARTICLES")) halfParticles = e[0] != '0';
	}

	void SceneRenderer::ForgetRayTracing()
	{
		for (std::unordered_map<IGeometry*, RtGeometry>::iterator i = rtGeometries.begin(); i != rtGeometries.end(); ++i) device->Destroy(i->second.geometry);
		rtGeometries.clear();
		rtScene.reset(); rtSceneOf = nullptr; rtRefused = false;
	}

	void SceneRenderer::BuildRayInstances()
	{
		// The frame's scene for the GPU's ray tracing: every opaque thing gathered - in view or not, what casts a shadow
		// into it or shows in a mirror may be anywhere - its mesh built once (again where it changed), and its colour
		// and glow by its place in the list (what a reflected ray found, rtreflect.frag)
		std::vector<gfx::Device::RayInstance> instances;
		std::vector<f32> mats;
		instances.reserve(items.size());
		uint32 instancedBudget = 50000;     // (an instanced set's members, at most this many in all)
		auto rows = [](const Matrix &m, f32* t) { for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) t[r * 4 + c] = m.m[c * 4 + r]; };
		for (const DrawItem &it : items)
		{
			if (it.transparent || !it.mesh || !it.mesh->Geometry) continue;
			IGeometry* g = it.mesh->Geometry;
			RtGeometry &rg = rtGeometries[g];
			const uint64 content = ContentRevision(g);
			if (!rg.geometry || rg.revision != content || rg.geometryId != g->GetInstanceID())
			{
				if (rg.geometry) device->Destroy(rg.geometry);
				rg = RtGeometry(); rg.revision = content; rg.geometryId = g->GetInstanceID();
				AttributeSource pos = SourceOf(g, "aPosition"), nor = SourceOf(g, "aNormal"), uv = SourceOf(g, "aTexcoord");
				PrimitiveArrays(g, pos, nor, uv);
				const uint32* indexData = nullptr; size_t indexCount = 0;
				IndicesOf(g, indexData, indexCount);
				if (pos.base && indexData && indexCount >= 3 && pos.stride % 4 == 0)
					rg.geometry = device->CreateRayGeometry((const f32*)pos.base, pos.count, (uint32)(pos.stride / 4), indexData, (uint32)indexCount);
			}
			if (!rg.geometry) continue;
			Vec4 albedo(0.7f, 0.7f, 0.7f, 1.f); Vec3 glow;
			if (GenericShaderMaterial* gm = it.material)
			{
				if (gm->GetOptions() & ShaderUsage::Color) albedo = SrgbToLinear(gm->GetColor());
				glow = gm->GetEmission();
			}
			const f32 mat[8] = { albedo.x, albedo.y, albedo.z, 1.f, glow.x, glow.y, glow.z, 0.f };
			auto add = [&](const Matrix &world) { gfx::Device::RayInstance ri; ri.geometry = rg.geometry; rows(world, ri.transform); instances.push_back(ri); mats.insert(mats.end(), mat, mat + 8); };
			if (it.instances)
			{
				RenderingInstancedComponent* ic = it.mesh && it.mesh->renderingComponent && it.mesh->renderingComponent->GetComponentType() == ComponentType::RenderingInstancedComponent
					? static_cast<RenderingInstancedComponent*>(it.mesh->renderingComponent) : nullptr;
				if (!ic) continue;
				const uint32 n = std::min((uint32)ic->transform.size(), it.instanceCount);
				if (n > instancedBudget) continue;
				instancedBudget -= n;
				for (uint32 i = 0; i < n; i++) add(it.model * ic->transform[i]);
			}
			else add(it.model);
		}
		device->SetRayScene(instances.data(), (uint32)instances.size());
		if (mats.empty()) mats.assign(8, 0.7f);
		rtInstanceCount = (uint32)(mats.size() / 8);
		const uint64 bytes = mats.size() * 4;
		if (bytes > rtInstanceMaterialsBytes || !rtInstanceMaterials)
		{
			if (rtInstanceMaterials) device->Destroy(rtInstanceMaterials);
			gfx::BufferDesc d; d.size = std::max<uint64>(bytes + bytes / 2, 4096); d.usage = gfx::BufStorage | gfx::BufCopyDst; d.debugName = "Renderer.RTInstanceMaterials";
			rtInstanceMaterials = device->CreateBuffer(d); rtInstanceMaterialsBytes = d.size;
			if (rtReflectRQGroup) { device->Destroy(rtReflectRQGroup); rtReflectRQGroup = gfx::BindGroup(); }
		}
		device->Upload(rtInstanceMaterials, 0, mats.data(), bytes);
	}

	void SceneRenderer::PrepareRayTracing(SceneGraph* scene)
	{
		// The scene's triangles as rays see them: made where the scene is another, made again where something came or
		// went, refitted where things only moved - and what changed uploaded
		bool upload = false;
		if (!rtScene || rtSceneOf != scene)
		{
			rtScene.reset(new RayScene());
			rtSceneOf = scene;
			rtRefused = false;
			if (scene && rtScene->BuildFromScene(scene))
			{
				// (more than the shaders' own walk does in a frame: neither tracer - the shadow maps and the screen's reflections)
				// (PYROS_RT_SOFTWARE_LIMIT=<triangles>: another limit - to see the fallback on a small scene)
				static const uint32 limit = getenv("PYROS_RT_SOFTWARE_LIMIT") ? (uint32)atol(getenv("PYROS_RT_SOFTWARE_LIMIT")) : kSoftwareRayTriangles;
				if (rtScene->TriangleCount() > limit)
				{
					fprintf(stderr, "[renderer] %u triangles: more than the shaders' own ray tracer takes (%u) and no ray-tracing hardware - the shadow maps and the screen's reflections\n", rtScene->TriangleCount(), limit);
					rtScene->Clear(); rtRefused = true; targetWidth = 0; rtSceneOf = scene;
					return;
				}
				rtScene->Build(4);
			}
			upload = true;
		}
		else
		{
			const RaySceneChange::Enum change = rtScene->RefreshTransforms();
			if (change == RaySceneChange::NeedsRebuild) { rtScene->Clear(); if (rtScene->BuildFromScene(scene)) rtScene->Build(4); upload = true; }
			else if (change == RaySceneChange::Moved) { rtScene->RefitBVH(); upload = true; }
		}
		if (!upload) return;
		std::vector<f32> tris, nodes; std::vector<uint32> indices;
		rtScene->PackForGPU(tris, nodes, indices);
		if (tris.empty()) { tris.assign(RayScene::kTriangleStrideFloats, 0.f); }
		if (nodes.empty()) { nodes.assign(RayScene::kNodeStrideFloats, 0.f); }
		if (indices.empty()) indices.push_back(0);
		auto put = [&](gfx::Buffer &b, uint64 &have, const void* data, uint64 bytes, const char* name)
		{
			if (bytes > have || !b)
			{
				if (b) device->Destroy(b);
				gfx::BufferDesc d; d.size = bytes; d.usage = gfx::BufStorage | gfx::BufCopyDst; d.memory = gfx::Memory::GpuOnly; d.debugName = name;
				b = device->CreateBuffer(d); have = bytes;
				if (rtShadowGroup) { device->Destroy(rtShadowGroup); rtShadowGroup = gfx::BindGroup(); }
		if (rtShadowRQGroup) { device->Destroy(rtShadowRQGroup); rtShadowRQGroup = gfx::BindGroup(); }
		if (rtReflectRQGroup) { device->Destroy(rtReflectRQGroup); rtReflectRQGroup = gfx::BindGroup(); }
			}
			device->Upload(b, 0, data, bytes);
		};
		put(rtTriangles, rtTrianglesBytes, tris.data(), tris.size() * 4, "Renderer.RTTriangles");
		put(rtNodes, rtNodesBytes, nodes.data(), nodes.size() * 4, "Renderer.RTNodes");
		put(rtIndices, rtIndicesBytes, indices.data(), indices.size() * 4, "Renderer.RTIndices");
		// (each material's colour and glow - what a reflected ray finds)
		std::vector<f32> mats;
		for (const RayMaterial &m : rtScene->materials) { const f32 a[8] = { m.albedo.x, m.albedo.y, m.albedo.z, 1.f, m.emissive.x, m.emissive.y, m.emissive.z, 0.f }; mats.insert(mats.end(), a, a + 8); }
		if (mats.empty()) mats.assign(8, 0.8f);
		rtMaterialCount = (uint32)(mats.size() / 8);
		const uint64 before = rtMaterialsBytes;
		put(rtMaterials, rtMaterialsBytes, mats.data(), mats.size() * 4, "Renderer.RTMaterials");
		if (rtMaterialsBytes != before && rtReflectGroup) { device->Destroy(rtReflectGroup); rtReflectGroup = gfx::BindGroup(); }
	}

	void SceneRenderer::RecordRTReflections(gfx::CommandList* cl, const uint32 frameOffset)
	{
		const bool hw = RtHardware();
		const gfx::Pipeline pipe = hw ? rtReflectRQPipeline : rtReflectPipeline;
		if (!device->PipelineReady(pipe) || !hdrPrevious) return;
		if (hw ? !rtInstanceMaterials : (!rtTriangles || !rtMaterials)) return;
		if (hw && !rtReflectRQGroup)
		{
			gfx::Device::BindGroupEntry e[6];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = prepassDepth;
			e[2].binding = 3; e[2].sampler = pointClamp;
			e[3].binding = 7; e[3].buffer = rtInstanceMaterials; e[3].size = rtInstanceMaterialsBytes;
			e[4].binding = 9; e[4].texture = hdrPrevious;
			e[5].binding = 10; e[5].texture = skySH;
			rtReflectRQGroup = device->CreateBindGroup(rtReflectRQPipeline, e, 6);
		}
		if (!hw && !rtReflectGroup)
		{
			gfx::Device::BindGroupEntry e[9];
			e[8].binding = 9; e[8].texture = hdrPrevious;
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = prepassDepth;
			e[2].binding = 3; e[2].sampler = pointClamp;
			e[3].binding = 4; e[3].buffer = rtTriangles; e[3].size = rtTrianglesBytes;
			e[4].binding = 5; e[4].buffer = rtNodes; e[4].size = rtNodesBytes;
			e[5].binding = 6; e[5].buffer = rtIndices; e[5].size = rtIndicesBytes;
			e[6].binding = 7; e[6].buffer = rtMaterials; e[6].size = rtMaterialsBytes;
			e[7].binding = 10; e[7].texture = skySH;
			rtReflectGroup = device->CreateBindGroup(rtReflectPipeline, e, 9);
		}
		graph.Use(cl, { { rtReflectTex, gfx::Access::ColorTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = rtReflectTex; pass.colors[0].load = gfx::Load::DontCare;
		pass.width = targetWidth; pass.height = targetHeight; pass.debugName = "RTReflections";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (f32)targetWidth, (f32)targetHeight);
		cl->SetPipeline(pipe);
		cl->SetBindGroup(hw ? rtReflectRQGroup : rtReflectGroup, &frameOffset, 1);
		{ static int said = -1; if (said != (int)hw) { said = (int)hw; fprintf(stderr, "[renderer] ray-traced %s: %s\n", "reflections", hw ? "the GPU's ray tracing (ray queries)" : "the shaders' own BVH walk"); } }
		const f32 dd[4] = { 500.f, hw || (rtScene && rtScene->TriangleCount() > 0) ? 1.f : 0.f, (f32)(hw ? rtInstanceCount : rtMaterialCount), (f32)(frameNumber % 1024) };
		cl->DrawData(dd, sizeof(dd));
		cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		graph.Use(cl, { { rtReflectTex, gfx::Access::ShaderRead } });
	}

	void SceneRenderer::RecordRTShadows(gfx::CommandList* cl, const uint32 frameOffset)
	{
		// (the GPU's own ray tracing where it has it: the frame's scene in place of the BVH's buffers)
		const bool hw = RtHardware();
		const gfx::Pipeline pipe = hw ? rtShadowRQPipeline : rtShadowPipeline;
		if (!device->PipelineReady(pipe) || (!hw && !rtTriangles)) return;
		if (hw && !rtShadowRQGroup)
		{
			gfx::Device::BindGroupEntry e[4];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = prepassDepth;
			e[2].binding = 2; e[2].texture = aoA;
			e[3].binding = 3; e[3].sampler = linearClamp;
			rtShadowRQGroup = device->CreateBindGroup(rtShadowRQPipeline, e, 4);
		}
		if (!hw && !rtShadowGroup)
		{
			gfx::Device::BindGroupEntry e[7];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = prepassDepth;
			e[2].binding = 2; e[2].texture = aoA;
			e[3].binding = 3; e[3].sampler = linearClamp;
			e[4].binding = 4; e[4].buffer = rtTriangles; e[4].size = rtTrianglesBytes;
			e[5].binding = 5; e[5].buffer = rtNodes; e[5].size = rtNodesBytes;
			e[6].binding = 6; e[6].buffer = rtIndices; e[6].size = rtIndicesBytes;
			rtShadowGroup = device->CreateBindGroup(rtShadowPipeline, e, 7);
		}
		graph.Use(cl, { { aoRT, gfx::Access::ColorTarget } });
		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = aoRT; pass.colors[0].load = gfx::Load::DontCare;
		pass.width = targetWidth; pass.height = targetHeight; pass.debugName = "RTShadows";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (f32)targetWidth, (f32)targetHeight);
		cl->SetPipeline(pipe);
		cl->SetBindGroup(hw ? rtShadowRQGroup : rtShadowGroup, &frameOffset, 1);
		{ static int said = -1; if (said != (int)hw) { said = (int)hw; fprintf(stderr, "[renderer] ray-traced %s: %s\n", "shadows", hw ? "the GPU's ray tracing (ray queries)" : "the shaders' own BVH walk"); } }
		const f32 dd[4] = { rtShadowsOn ? rtSunAngle : rtSunSoftness, hw || (rtScene && rtScene->TriangleCount() > 0) ? 1.f : 0.f, (f32)(frameNumber % 1024), 2000.f };
		cl->DrawData(dd, sizeof(dd));
		cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		graph.Use(cl, { { aoRT, gfx::Access::ShaderRead } });
	}

	void SceneRenderer::ForgetGpuParticles(GpuParticles &g)
	{
		if (g.group) device->Destroy(g.group);
		if (g.state) device->Destroy(g.state);
		if (g.stream) device->Destroy(g.stream);
		g = GpuParticles();
	}

	void SceneRenderer::RecordParticleSim(gfx::CommandList* cl)
	{
		if (!device->PipelineReady(particleSimPipeline)) return;
		for (const std::pair<f32, ParticleSystem*> &e : particleSystems)
		{
			ParticleSystem* ps = e.second;
			if (!ps->SimulatedOnGPU()) continue;
			const ParticleSystem::GpuStep &st = ps->GetGpuStep();
			if (st.capacity == 0) continue;
			GpuParticles &g = gpuParticles[ps];
			g.seen = frameNumber;
			// (its ring made - again where it grew or shrank: 48 bytes a particle, and the 32 of the stream drawn)
			bool fresh = false;
			if (g.capacity != st.capacity || !g.state)
			{
				ForgetGpuParticles(g);
				gfx::BufferDesc b; b.usage = gfx::BufStorage; b.memory = gfx::Memory::GpuOnly;
				b.size = (uint64)st.capacity * 48; b.debugName = "Renderer.ParticleState"; g.state = device->CreateBuffer(b);
				b.usage = gfx::BufStorage | gfx::BufVertex; b.size = (uint64)st.capacity * 32; b.debugName = "Renderer.ParticleStream"; g.stream = device->CreateBuffer(b);
				gfx::Device::BindGroupEntry en[2];
				en[0].binding = 1; en[0].buffer = g.state; en[0].size = (uint64)st.capacity * 48;
				en[1].binding = 2; en[1].buffer = g.stream; en[1].size = (uint64)st.capacity * 32;
				g.group = device->CreateBindGroup(particleSimPipeline, en, 2);
				g.capacity = st.capacity;
				fresh = true;
			}
			if (g.step == st.id && !fresh) continue;     // (stepped already: another view of the same frame)
			g.step = st.id;
			const ParticleSystemDesc &d = ps->GetDesc();
			const Vec3 axis = d.direction.magnitude() > 1e-6f ? d.direction.normalize() : Vec3(0.f, 1.f, 0.f);
			const f32 dd[24] = {
				st.origin.x, st.origin.y, st.origin.z, st.time,
				axis.x, axis.y, axis.z, cosf(d.spreadAngle),
				d.minSpeed, d.maxSpeed, d.minLifetime, d.maxLifetime,
				d.gravity.x, d.gravity.y, d.gravity.z, d.damping,
				(f32)st.start, (f32)st.count, (f32)st.capacity, (st.reset || fresh) ? 1.f : 0.f,
				st.dt, d.minRotationSpeed, d.maxRotationSpeed, 0.f };
			gfx::Barrier before; before.buffer = g.stream; before.from = fresh ? gfx::Access::Undefined : gfx::Access::VertexIndex; before.to = gfx::Access::ShaderWrite;
			cl->Barriers(&before, 1);
			cl->SetPipeline(particleSimPipeline);
			cl->SetBindGroup(g.group, nullptr, 0);
			cl->DrawData(dd, sizeof(dd));
			cl->Dispatch((st.capacity + 63) / 64, 1, 1);
			gfx::Barrier after; after.buffer = g.stream; after.from = gfx::Access::ShaderWrite; after.to = gfx::Access::VertexIndex;
			cl->Barriers(&after, 1);
		}
		// (the rings of systems no longer drawn: let go a few frames on)
		for (std::unordered_map<ParticleSystem*, GpuParticles>::iterator i = gpuParticles.begin(); i != gpuParticles.end();)
			if (i->second.seen + 8 < frameNumber) { ForgetGpuParticles(i->second); i = gpuParticles.erase(i); }
			else ++i;
	}

	void SceneRenderer::RecordParticles(gfx::CommandList* cl, const uint32 frameOffset, const bool half)
	{
		const gfx::Pipeline alphaPipe = half ? particleHalfAlphaPipeline : particleAlphaPipeline, addPipe = half ? particleHalfAddPipeline : particleAddPipeline;
		if (particleSystems.empty() || !device->PipelineReady(alphaPipe) || !device->PipelineReady(addPipe)) return;
		// (the ring grown to hold the frame's particles - every frame slot's share at once; the old one is freed when the
		// frames still reading it are done)
		uint64 needed = 0;
		for (const std::pair<f32, ParticleSystem*> &e : particleSystems) if (!e.second->SimulatedOnGPU()) needed += (uint64)e.second->GetLiveParticleCount() * 32;
		if (needed > particleRingBytes)
		{
			while (particleRingBytes < needed) particleRingBytes *= 2;
			device->Destroy(particleRing);
			gfx::BufferDesc b; b.size = particleRingBytes * kFramesInFlight; b.usage = gfx::BufVertex; b.memory = gfx::Memory::Upload; b.debugName = "Renderer.Particles";
			particleRing = device->CreateBuffer(b);
		}
		const uint64 slotStart = (uint64)particleSlot * particleRingBytes;
		uint64 used = 0;
		gfx::Pipeline bound;
		for (const std::pair<f32, ParticleSystem*> &e : particleSystems)
		{
			ParticleSystem* ps = e.second;
			const ParticleSystemDesc &d = ps->GetDesc();
			// (on the GPU: its own stream, as the compute pass left it - every slot of its ring, the dead with no size)
			const std::unordered_map<ParticleSystem*, GpuParticles>::const_iterator onGpu = ps->SimulatedOnGPU() ? gpuParticles.find(ps) : gpuParticles.end();
			if (ps->SimulatedOnGPU() && (onGpu == gpuParticles.end() || !onGpu->second.stream)) continue;
			const uint32 n = onGpu != gpuParticles.end() ? onGpu->second.capacity : ps->GetLiveParticleCount();
			const uint64 bytes = onGpu != gpuParticles.end() ? 0 : (uint64)n * 32;
			if (bytes) device->Upload(particleRing, slotStart + used, ps->GetParticleData(), bytes);
			const gfx::Texture tex = TextureFor(d.texture.get(), true, white);
			gfx::BindGroup &g = particleGroups[tex.id];
			if (!g)
			{
				gfx::Device::BindGroupEntry en[5];
				en[0].binding = 0; en[0].buffer = frameData; en[0].size = sizeof(FrameData);
				en[1].binding = 1; en[1].texture = tex;
				en[2].binding = 2; en[2].sampler = linearClamp;
				en[3].binding = 3; en[3].texture = prepassDepth;
				en[4].binding = 4; en[4].texture = skySH;
				g = device->CreateBindGroup(alphaPipe, en, 5);
			}
			const bool additive = d.blendMode == ParticleBlendMode::Additive;
			const gfx::Pipeline pipe = additive ? addPipe : alphaPipe;
			if (!(pipe == bound)) { cl->SetPipeline(pipe); bound = pipe; }
			cl->SetBindGroup(g, &frameOffset, 1);
			const Vec4 c0 = SrgbToLinear(d.startColor), c1 = SrgbToLinear(d.endColor);
			const f32 dd[20] = { c0.x, c0.y, c0.z, c0.w, c1.x, c1.y, c1.z, c1.w,
				d.startSize, d.endSize, d.sizeRandomJitter, (f32)d.sizeEase,
				(f32)d.colorEase, d.fadeInFraction, d.fadeOutFraction, d.softness,
				ps->IsLit() ? 1.f : 0.f, ps->GetForwardScatter(), additive ? 1.f : 0.f, 0.f };
			cl->DrawData(dd, sizeof(dd));
			if (onGpu != gpuParticles.end()) cl->SetVertexBuffer(0, onGpu->second.stream, 0);
			else cl->SetVertexBuffer(0, particleRing, slotStart + used);
			cl->Draw(6, n, 0, 0);
			used += bytes;
		}
	}

	namespace
	{
		// A compiled text's name in a surface cache: by the whole text it is compiled from (the engine's surface.frag/.vert
		// changed is another key) and its stage
		std::string SurfaceCacheName(const std::string &text, const gfx::Stage stage)
		{
			uint64 key = 14695981039346656037ull;
			for (unsigned char c : text) { key ^= c; key *= 1099511628211ull; }
			char name[48]; snprintf(name, sizeof(name), "/%016llx.%s", (unsigned long long)key, stage == gfx::Stage::Vertex ? "vert" : "frag");
			return name;
		}
	}

	bool SceneRenderer::WriteSurfaceCache(const std::string &source, const std::string &directory, std::string &error)
	{
#if defined(PYROS_RENDERER_GLSLANG) && defined(PYROS_GFX_SPIRV_TO_WGSL)
		std::vector<std::pair<std::string, bool> > params; std::vector<std::string> textures;
		std::string fragment, vertex[3];
		SurfaceTexts(source, params, textures, fragment, vertex);
		const std::pair<const std::string*, gfx::Stage> texts[4] = { { &fragment, gfx::Stage::Fragment },
			{ &vertex[0], gfx::Stage::Vertex }, { &vertex[1], gfx::Stage::Vertex }, { &vertex[2], gfx::Stage::Vertex } };
		for (const std::pair<const std::string*, gfx::Stage> &t : texts)
		{
			if (t.first->empty()) continue;
			std::vector<uint32> spirv; std::string wgsl, e;
			if (!GlslToSpirv(*t.first, t.second, spirv, e)) { error = e; return false; }
			if (!gfx::SpirvToWgsl(spirv.data(), spirv.size(), wgsl, e)) { error = "its WGSL: " + e; return false; }
			const std::string base = directory + SurfaceCacheName(*t.first, t.second);
			FILE* f = fopen((base + ".spv").c_str(), "wb");
			if (!f) { error = "could not write " + base + ".spv"; return false; }
			fwrite(spirv.data(), 4, spirv.size(), f); fclose(f);
			f = fopen((base + ".wgsl").c_str(), "wb");
			if (!f) { error = "could not write " + base + ".wgsl"; return false; }
			fwrite(wgsl.data(), 1, wgsl.size(), f); fclose(f);
		}
		return true;
#else
		(void)source; (void)directory;
		error = "this build makes no WGSL (it has no WebGPU) - a build with it writes a web game's surfaces";
		return false;
#endif
	}

	bool SceneRenderer::SurfaceProgram(const std::string &text, const gfx::Stage stage, std::vector<uint32> &spirv, std::string &wgsl, std::string &error)
	{
		if (surfaceCache.empty() && getenv("PYROS_RENDERER_SURFACE_CACHE")) surfaceCache = getenv("PYROS_RENDERER_SURFACE_CACHE");
		const std::string base = surfaceCache.empty() ? std::string() : surfaceCache + SurfaceCacheName(text, stage);
		std::vector<char> bytes;
		auto readFile = [&](const std::string &path) -> bool {
			FILE* f = path.empty() ? nullptr : fopen(path.c_str(), "rb");
			if (!f) return false;
			fseek(f, 0, SEEK_END); const long n = ftell(f); fseek(f, 0, SEEK_SET);
			bytes.resize(n > 0 ? (size_t)n : 0);
			const bool ok = n > 0 && fread(bytes.data(), 1, bytes.size(), f) == bytes.size();
			fclose(f);
			return ok;
		};
#if defined(PYROS_RENDERER_WGSL)
		// (a browser build: the WGSL a native run wrote beside the SPIR-V)
		if (readFile(base + ".wgsl")) { wgsl.assign(bytes.begin(), bytes.end()); return true; }
		error = "not in the surface cache (" + (base.empty() ? std::string("none set") : base + ".wgsl") + "): a native run with PYROS_RENDERER_SURFACE_CACHE writes it";
		return false;
#else
		if (readFile(base + ".spv") && bytes.size() % 4 == 0)
		{
			spirv.resize(bytes.size() / 4); memcpy(spirv.data(), bytes.data(), bytes.size());
			return true;
		}
#if defined(PYROS_RENDERER_GLSLANG)
		if (!GlslToSpirv(text, stage, spirv, error)) return false;
		if (!base.empty())
		{
			if (FILE* f = fopen((base + ".spv").c_str(), "wb")) { fwrite(spirv.data(), 4, spirv.size(), f); fclose(f); }
#if defined(PYROS_GFX_SPIRV_TO_WGSL)
			// (and its WGSL, for a browser build of the same game)
			std::string w, werr;
			if (gfx::SpirvToWgsl(spirv.data(), spirv.size(), w, werr)) { if (FILE* f = fopen((base + ".wgsl").c_str(), "wb")) { fwrite(w.data(), 1, w.size(), f); fclose(f); } }
			else fprintf(stderr, "Renderer: a surface's WGSL could not be made: %s\n", werr.c_str());
#endif
		}
		return true;
#else
		error = "not in the surface cache (" + (base.empty() ? std::string("none set") : base + ".spv") + "), and this build compiles no GLSL";
		return false;
#endif
#endif
	}

	void SceneRenderer::SurfaceTexts(const std::string &source, std::vector<std::pair<std::string, bool> > &params, std::vector<std::string> &textures,
		std::string &fragment, std::string vertex[3])
	{
		// Its parameters and textures, from its //@ lines
		params.clear(); textures.clear();
		std::istringstream lines(source);
		std::string line;
		while (std::getline(lines, line))
		{
			std::istringstream w(line);
			std::string tag, name, type;
			w >> tag >> name >> type;
			if (tag == "//@param") params.push_back(std::make_pair(name, type == "vec4"));
			else if (tag == "//@texture") textures.push_back(name);
		}
		// surface.frag with CUSTOM_SURFACE, the parameters' block and the textures declared, and the surface in
		std::ostringstream decl;
		decl << "layout(set = 1, binding = 20, std140) uniform MaterialParams { vec4 v[" << std::max<size_t>(1, params.size()) << "]; } uParams;\n";
		for (size_t i = 0; i < params.size(); i++) decl << "#define p_" << params[i].first << " (uParams.v[" << i << "]" << (params[i].second ? "" : ".x") << ")\n";
		for (size_t i = 0; i < textures.size(); i++)
			decl << "layout(set = 1, binding = " << (28 + i) << ") uniform texture2D p3d_texture" << i << ";\n#define " << textures[i] << " sampler2D(p3d_texture" << i << ", p3d_materialSampler)\n";
		// (both stages name the material's sampler the same: the fragment stage's is binding 6, the vertex stage declares it)
		const std::string vertexDecl = "layout(set = 1, binding = 6) uniform sampler p3d_materialSampler;\n" + decl.str();
		const std::string fragmentDecl = "#define p3d_materialSampler uMaterialSampler\n" + decl.str();
		std::set<std::string> done;
		fragment = Resolve("surface.frag", done);
		const size_t v = fragment.find('\n');
		fragment.insert(v + 1, "#define CUSTOM_SURFACE\n");
		const size_t d = fragment.find("//P3D_SURFACE_DECLARATIONS");
		if (d != std::string::npos) fragment.replace(d, strlen("//P3D_SURFACE_DECLARATIONS"), fragmentDecl);
		const size_t sfc = fragment.find("//P3D_SURFACE\n");
		if (sfc != std::string::npos) fragment.replace(sfc, strlen("//P3D_SURFACE"), source);
		// Its own vertex stage, where it has one: surface.vert with CUSTOM_VERTEX - plain, instanced (1) and posed by bones (2)
		for (int variant = 0; variant < 3; variant++) vertex[variant].clear();
		if (source.find("void Vertex(") == std::string::npos) return;
		for (int variant = 0; variant < 3; variant++)
		{
			std::set<std::string> vdone;
			std::string &vt = vertex[variant];
			vt = Resolve("surface.vert", vdone);
			const size_t vl = vt.find('\n');
			vt.insert(vl + 1, std::string("#define CUSTOM_VERTEX\n") + (variant == 1 ? "#define INSTANCED\n" : variant == 2 ? "#define SKINNED\n" : ""));
			const size_t vd = vt.find("//P3D_SURFACE_DECLARATIONS");
			if (vd != std::string::npos) vt.replace(vd, strlen("//P3D_SURFACE_DECLARATIONS"), vertexDecl);
			const size_t vs = vt.find("//P3D_SURFACE\n");
			if (vs != std::string::npos) vt.replace(vs, strlen("//P3D_SURFACE"), source);
		}
	}

	bool SceneRenderer::CheckSurface(const std::string &source, std::string &error)
	{
#if defined(PYROS_RENDERER_WGSL)
		error.clear();
		return true;     // (a browser build compiles none: its surfaces come from the cache)
#else
		std::vector<std::pair<std::string, bool> > params; std::vector<std::string> textures;
		std::string fragment, vertex[3];
		SurfaceTexts(source, params, textures, fragment, vertex);
		std::vector<uint32> spirv;
		std::string e;
		if (!GlslToSpirv(fragment, gfx::Stage::Fragment, spirv, e)) { error = "[surface] " + e; return false; }
		if (!vertex[0].empty() && !GlslToSpirv(vertex[0], gfx::Stage::Vertex, spirv, e)) { error = "[vertex] " + e; return false; }
		error.clear();
		return true;
#endif
	}

	SceneRenderer::CustomSurface* SceneRenderer::SurfaceFor(CustomShaderMaterial* material, uint64 &hash)
	{
		SurfaceRef &ref = surfaceRefs[material];
		if (ref.revision == material->GetSurfaceRevision() && ref.materialId == material->GetInternalID()) { hash = ref.hash; return ref.surface; }
		ref.revision = material->GetSurfaceRevision(); ref.surface = nullptr; ref.materialId = material->GetInternalID();
		const std::string &source = material->GetSurfaceSource();
		hash = 14695981039346656037ull;
		for (unsigned char c : source) { hash ^= c; hash *= 1099511628211ull; }
		ref.hash = hash;
		std::map<uint64, CustomSurface>::iterator found = customSurfaces.find(hash);
		if (found != customSurfaces.end()) return ref.surface = found->second.failed ? nullptr : &found->second;
		CustomSurface &cs = customSurfaces[hash];
		std::string text, vertexTexts[3];
		SurfaceTexts(source, cs.params, cs.textures, text, vertexTexts);
		std::vector<uint32> spirv;
		std::string error, wgsl;
		if (!SurfaceProgram(text, gfx::Stage::Fragment, spirv, wgsl, error))
		{
			fprintf(stderr, "Renderer: a material's surface did not compile:\n%s\n", error.c_str());
			cs.failed = true;
			return nullptr;
		}
		gfx::ShaderDesc sd; sd.stage = gfx::Stage::Fragment; sd.spirv = spirv.data(); sd.spirvWords = spirv.size(); sd.debugName = "surface.frag (custom)";
		if (!wgsl.empty()) sd.wgsl = wgsl.c_str();
		cs.shader = device->CreateShader(sd);
		if (!cs.shader) { cs.failed = true; return nullptr; }
		// Its own vertex stage, where it has one: surface.vert with CUSTOM_VERTEX - plain, instanced (1) and posed by bones (2)
		if (!vertexTexts[0].empty())
			for (int variant = 0; variant < 3; variant++)
			{
				const std::string &vt = vertexTexts[variant];
				std::vector<uint32> vspirv;
				std::string vwgsl;
				if (!SurfaceProgram(vt, gfx::Stage::Vertex, vspirv, vwgsl, error))
				{
					fprintf(stderr, "Renderer: a material's vertex stage did not compile:\n%s\n", error.c_str());
					cs.failed = true;
					return nullptr;
				}
				gfx::ShaderDesc vsd; vsd.stage = gfx::Stage::Vertex; vsd.spirv = vspirv.data(); vsd.spirvWords = vspirv.size(); vsd.debugName = "surface.vert (custom)";
				if (!vwgsl.empty()) vsd.wgsl = vwgsl.c_str();
				(variant == 1 ? cs.vertexInstanced : variant == 2 ? cs.vertexSkinned : cs.vertex) = device->CreateShader(vsd);
			}
		return ref.surface = &cs;
	}

	gfx::Pipeline SceneRenderer::CustomPipeline(const CustomKey &key, CustomSurface* cs)
	{
		std::map<CustomKey, gfx::Pipeline>::iterator i = customPipelines.find(key);
		if (i != customPipelines.end()) return i->second;
		gfx::GraphicsPipelineDesc p;
		p.vertex = key.instanced ? (cs->vertexInstanced ? cs->vertexInstanced : surfaceInstVS)
			: key.skinned ? (cs->vertexSkinned ? cs->vertexSkinned : surfaceSkinVS) : (cs->vertex ? cs->vertex : surfaceVS);
		p.fragment = cs->shader;
		p.material = { { 0, gfx::BindingKind::Uniform, true, true, false, true }, { 1, gfx::BindingKind::StorageRead, false, true, false, true },
			{ 2, gfx::BindingKind::Texture2D, false, true }, { 3, gfx::BindingKind::Texture2D, false, true }, { 4, gfx::BindingKind::Texture2D, false, true },
			{ 5, gfx::BindingKind::Texture2D, false, true }, { 6, gfx::BindingKind::Sampler, false, true },
			{ 7, gfx::BindingKind::Uniform, false, true, false, true }, { 8, gfx::BindingKind::ShadowTexture, false, true }, { 9, gfx::BindingKind::ComparisonSampler, false, true },
			{ 10, gfx::BindingKind::Texture2D, false, true }, { 11, gfx::BindingKind::Texture2D, false, true },
			{ 12, gfx::BindingKind::Texture2D, false, true }, { 13, gfx::BindingKind::DepthTexture, false, true }, { 14, gfx::BindingKind::Sampler, false, true },
			{ 15, gfx::BindingKind::Texture2D, false, true }, { 16, gfx::BindingKind::Texture2D, false, true },
			{ 17, gfx::BindingKind::StorageRead, true, false },     // (the skinned's bones: every custom group has them, as every surface group)
			{ 19, gfx::BindingKind::Texture2D, false, true },     // (the reflection probes' atlas)
			{ 20, gfx::BindingKind::Uniform, true, true, false, true },     // (18 kept for the renderer's own)
			{ 21, gfx::BindingKind::Texture2D, false, true }, { 22, gfx::BindingKind::UnfilteredTexture2D, false, true }, { 23, gfx::BindingKind::UnfilteredTexture2D, false, true },
			{ 24, gfx::BindingKind::Texture2D, false, true } };     // (the DDGI's: 24-27 kept for the renderer's own)
		// (its textures, and the material sampler, seen by its vertex stage too where it has one)
		if (cs->vertex) p.material[6].vertex = true;
		for (size_t t = 0; t < cs->textures.size(); t++) p.material.push_back({ (uint32_t)(28 + t), gfx::BindingKind::Texture2D, cs->vertex ? true : false, true });
		p.drawDataBytes = sizeof(DrawData);
		p.bindings = { { 0, (uint16_t)sizeof(Vertex), false } };
		p.attributes = { { 0, 0, gfx::Format::RGB32F, 0 }, { 1, 0, gfx::Format::RGB32F, 12 }, { 2, 0, gfx::Format::RG32F, 24 } };
		if (key.instanced) AddInstanceStream(p);
		if (key.skinned) { p.bindings.push_back({ 2, 32, false }); p.attributes.push_back({ 8, 2, gfx::Format::RGBA32F, 0 }); p.attributes.push_back({ 9, 2, gfx::Format::RGBA32F, 16 }); }
		p.cull = key.cull == CullFace::DoubleSided ? gfx::Cull::None : key.cull == CullFace::FrontFace ? gfx::Cull::Front : gfx::Cull::Back;
		p.depthTest = true; p.depthWrite = key.blend == (uint8)gfx::Blend::Opaque; p.depthCompare = gfx::Compare::GreaterEqual;
		p.colorCount = 1; p.colorFormats[0] = kHdrFormat; p.blend[0] = (gfx::Blend)key.blend;
		p.depthFormat = kDepthFormat; p.samples = kSceneSamples;
		p.debugName = "Renderer.CustomSurface";
		if (key.blend == kPrepassBlend)
		{
			// (its depth prepass: its own vertex stage - where it moves its vertices, there it is - no move of its own known)
			p.fragment = noVelocityFS; p.depthWrite = true;
			p.colorFormats[0] = gfx::Format::RG16F; p.blend[0] = gfx::Blend::Opaque; p.samples = 1;
			p.debugName = "Renderer.CustomSurface (depth prepass)";
		}
		const gfx::Pipeline made = device->CreatePipeline(p);
		customPipelines[key] = made;
		return made;
	}

	gfx::BindGroup SceneRenderer::CustomGroupFor(CustomShaderMaterial* m, CustomSurface* cs, gfx::Pipeline layoutOf, const bool prepass)
	{
		// (its textures, by the names its surface reads them by - the material's AddSampler() names)
		std::vector<gfx::Texture> tex(cs->textures.size(), white);
		const std::vector<std::string> &names = m->GetSamplerNames();
		for (size_t t = 0; t < cs->textures.size(); t++)
			for (size_t k = 0; k < names.size() && k < m->textures.size(); k++)
				if (names[k] == cs->textures[t]) tex[t] = TextureFor(m->textures[k].get(), false, white);     // (as stored: a surface decodes its colours itself - p3d_DecodeSRGB)
		std::vector<uint32_t> ids; for (const gfx::Texture &t : tex) ids.push_back(t.id);
		CustomGroup &g = (prepass ? customPrepassGroups : customGroups)[m];
		if (g.group && g.textures == ids && g.revision == m->GetSurfaceRevision() && g.materialId == m->GetInternalID()) return g.group;
		if (g.group) device->Destroy(g.group);
		g.materialId = m->GetInternalID();
		std::vector<gfx::Device::BindGroupEntry> e(24 + tex.size());
		e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
		e[1].binding = 1; e[1].buffer = lightData; e[1].size = kLightDataSlot;
		e[2].binding = 2; e[2].texture = white; e[3].binding = 3; e[3].texture = flatNormal; e[4].binding = 4; e[4].texture = white; e[5].binding = 5; e[5].texture = white;
		e[6].binding = 6; e[6].sampler = materialSampler;
		e[7].binding = 7; e[7].buffer = shadowData; e[7].size = sizeof(ShadowData);
		e[8].binding = 8; e[8].texture = shadowAtlas;
		e[9].binding = 9; e[9].sampler = shadowCompare;
		e[10].binding = 10; e[10].texture = skySH;
		e[11].binding = 11; e[11].texture = AOForSurfaces();
		e[12].binding = 12; e[12].texture = skyViewClouded;     // (the sky with its clouds: what the surface mirrors)
		e[13].binding = 13; e[13].texture = prepass ? standInDepth : prepassDepth;     // (not the depth being drawn)
		e[14].binding = 14; e[14].sampler = linearClamp;
		e[15].binding = 15; e[15].texture = cloudShadowTex;     // (the clouds' shadow on the sun)
		e[16].binding = 16; e[16].texture = PreviousForSurfaces();      // (last frame's picture: what the reflections find)
		e[17].binding = 20; e[17].buffer = materialParams; e[17].size = 16 * std::max<size_t>(1, cs->params.size());
		e[18].binding = 17; e[18].buffer = bonePalette;
		e[19].binding = 19; e[19].texture = probeAtlas ? probeAtlas : white;
		e[20].binding = 21; e[20].texture = giIrradiance ? giIrradiance : white;
		e[21].binding = 22; e[21].texture = giVisibility ? giVisibility : white;
		e[22].binding = 23; e[22].texture = giProbes ? giProbes : white;
		e[23].binding = 24; e[23].texture = giRadiance ? giRadiance : white;
		for (size_t t = 0; t < tex.size(); t++) { e[24 + t].binding = (uint32_t)(28 + t); e[24 + t].texture = tex[t]; }
		g.group = device->CreateBindGroup(layoutOf, e.data(), (uint32_t)e.size());
		g.textures = ids; g.revision = m->GetSurfaceRevision();
		return g.group;
	}

	uint32 SceneRenderer::WriteCustomParams(CustomShaderMaterial* m, CustomSurface* cs)
	{
		// (this frame's values, in a 256-aligned slice of the ring: a vec4 each, in the //@param order)
		const uint32 bytes = 16 * (uint32)std::max<size_t>(1, cs->params.size());
		const uint32 at = materialParamsUsed;
		if (at + bytes > kMaterialParamsSlot) return 0;
		materialParamsUsed += (bytes + 255) / 256 * 256;
		std::vector<f32> values(bytes / 4, 0.f);
		for (size_t i = 0; i < cs->params.size(); i++)
		{
			const Vec4 v = m->GetParameter(cs->params[i].first);
			values[i * 4 + 0] = v.x; values[i * 4 + 1] = v.y; values[i * 4 + 2] = v.z; values[i * 4 + 3] = v.w;
		}
		const uint32 slotBase = particleSlot * kMaterialParamsSlot;
		device->Upload(materialParams, slotBase + at, values.data(), bytes);
		return slotBase + at;
	}

	void SceneRenderer::Gather(SceneGraph* scene, const View &view, const Matrix &vp)
	{
		items.clear(); opaqueOrder.clear(); transparentOrder.clear(); worldTextOrder.clear();
		boneScratch.clear();
		skyboxSource = nullptr;
		if (!scene) return;
		// Sprites and 2D lines that changed since the scene was last drawn get their quads now, and their batches
		// re-merge; levels of detail made since are given - before the meshes below are listed (as the old renderer did)
		Sprite2D::FlushScene(scene);
		Line2D::FlushScene(scene);
		RenderingComponent::PollAutomaticLODs();
		const Vec3 eye = view.cameraWorld.GetTranslation();
		const Vec3 sortForward = (view.cameraWorld * Vec4(0.f, 0.f, -1.f, 0.f)).xyz().normalize();     // (the way the camera looks)
		// The frustum's planes, from the view-projection's rows (column-major: row r is m[r], m[4 + r], m[8 + r], m[12 + r]):
		// left, right, bottom, top and near - reversed Z with no far plane
		Vec4 planes[5];
		auto row = [&](int r) { return Vec4(vp.m[r], vp.m[4 + r], vp.m[8 + r], vp.m[12 + r]); };
		const Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
		planes[0] = r3 + r0; planes[1] = r3 - r0; planes[2] = r3 + r1; planes[3] = r3 - r1; planes[4] = r3 - r2;
		for (Vec4 &p : planes) { const f32 len = Vec3(p.x, p.y, p.z).magnitude(); if (len > 0.f) p = p / len; }
		// (the scene lists each component's first level; a component at another level is drawn from its own list of it)
		std::vector<RenderingMesh*> chosen;
		std::unordered_set<RenderingComponent*> done;
		done.reserve(1024);
		particleSystems.clear();
		std::set<ParticleSystem*> seenSystems;
		// The components with levels of detail, from the scene's components - not from its meshes: it lists the meshes
		// of the level the old renderer last chose, and none of one it put past its last level (an Erangel terrain tile,
		// set so by a view of another renderer, was never drawn again). A model with none gets its own the first time it
		// is seen, as the old renderer gives them; the level by the distance to its sphere (past its last: its last)
		if (lodOn)
			for (RenderingComponent* rc : scene->GetRenderingComponents())
			{
				if (!rc || !rc->GetOwner() || rc->GetRenderLayer() != RenderLayer::World || rc->GetComponentType() == ComponentType::ParticleSystem) continue;
				if (!rc->HasLOD()) rc->TryAutomaticLODs();
				if (!rc->HasLOD() || !done.insert(rc).second) continue;
				const uint32 level = rc->LevelAtDistanceSQR(rc->LODDistanceSQR(eye, rc->GetOwner()->GetDrawWorldPosition(), rc->GetOwner()->GetScale()));
				for (RenderingMesh* m : rc->GetMeshes(level)) chosen.push_back(m);
			}
		for (RenderingMesh* base : scene->GetRenderingMeshes())
		{
			if (!base || !base->renderingComponent || !base->renderingComponent->GetOwner()) continue;
			RenderingComponent* rc = base->renderingComponent;
			if (!rc->IsActive()) continue;      // (switched off - IComponent::Disable: a script hiding it, a scope's glass while its own view is drawn)
			if (rc->GetRenderLayer() != RenderLayer::World) continue;      // (the UI's - and the hidden layer's - are not the world's)
			if (rc->GetComponentType() == ComponentType::ParticleSystem)
			{
				ParticleSystem* ps = static_cast<ParticleSystem*>(rc);
				if (seenSystems.insert(ps).second && ps->GetLiveParticleCount() > 0 && (ps->GetParticleData() || (ps->SimulatedOnGPU() && ps->GetGpuStep().capacity > 0)))
					particleSystems.push_back(std::make_pair((rc->GetOwner()->GetDrawWorldPosition() - eye).magnitude(), ps));
				continue;
			}
			if (!rc->HasLOD() || !lodOn) { chosen.push_back(base); continue; }
		}

		for (RenderingMesh* rm : chosen)
		{
			if (!rm || !rm->Active || !rm->Geometry || !rm->renderingComponent || !rm->renderingComponent->GetOwner()) continue;
			RenderingComponent* rc = rm->renderingComponent;
			GameObject* owner = rc->GetOwner();
			const bool instancedComponent = rc->GetComponentType() == ComponentType::RenderingInstancedComponent;
			// What it is drawn with - kept while nothing it depends on has changed
			MeshDraw &md = meshDraws[rm];
			IMaterial* material = rm->Material.get();
			bool stale = md.material != material || md.geometry != rm->Geometry || md.geometryRevision != rm->Geometry->buffersRevision || md.instanced != instancedComponent
				|| md.materialId != material->GetInternalID() || md.geometryId != rm->Geometry->GetInstanceID();
			if (!stale && md.cm) stale = md.surfaceRevision != md.cm->GetSurfaceRevision();
			if (!stale) stale = md.transparent != Blended(material) || md.cull != (uint32)material->GetCullFace();
			if (!stale && md.gm)
			{
				Texture* const maps[5] = { md.gm->GetColorMap(), md.gm->GetNormalMap(), md.gm->GetMetallicRoughnessMap(), md.gm->GetEmissionMap(), md.gm->GetDisplacementMap() };
				for (int k = 0; k < 5 && !stale; k++) stale = maps[k] != md.maps[k] || (maps[k] && maps[k]->GetCpuRevision() != md.mapRevisions[k]);
			}
			if (stale)
			{
				md = MeshDraw();
				md.material = material; md.geometry = rm->Geometry; md.instanced = instancedComponent;
				md.materialId = material->GetInternalID(); md.geometryId = rm->Geometry->GetInstanceID();
				md.gm = dynamic_cast<GenericShaderMaterial*>(material);
				md.cm = md.gm ? nullptr : dynamic_cast<CustomShaderMaterial*>(material);
				md.skybox = md.gm ? md.gm->GetSkyboxMap() : nullptr;
				if (md.cm && md.cm->GetSurfaceSource().empty()) md.cm = nullptr;      // (a custom material without the renderer's surface: not drawn)
				// (PYROS_RENDERER_SKIP=<text>: materials whose shader file names it not drawn - to measure what one costs)
				static const char* skip = getenv("PYROS_RENDERER_SKIP");
				if (md.cm && skip && md.cm->GetShaderFile().find(skip) != std::string::npos) md.cm = nullptr;
				md.gpu = (md.gm || md.cm) ? MeshFor(rm->Geometry) : nullptr;
				md.content = ContentRevision(rm->Geometry);
				md.geometryRevision = rm->Geometry->buffersRevision;
				md.transparent = Blended(material); md.cull = (uint32)material->GetCullFace();
				if (md.cm)
				{
					md.surfaceRevision = md.cm->GetSurfaceRevision();
					md.surface = SurfaceFor(md.cm, md.surfaceHash);
					// (a material with a vertex stage of its own moves its vertices - the sea's sheet follows the eye: its
					// bounds say nothing of where it is drawn)
					md.movesItself = md.surface && md.surface->vertex;
				}
				if (md.gm)
				{
					Texture* const maps[5] = { md.gm->GetColorMap(), md.gm->GetNormalMap(), md.gm->GetMetallicRoughnessMap(), md.gm->GetEmissionMap(), md.gm->GetDisplacementMap() };
					for (int k = 0; k < 5; k++) { md.maps[k] = maps[k]; md.mapRevisions[k] = maps[k] ? maps[k]->GetCpuRevision() : 0; }
					md.alphaCut = (md.gm->GetOptions() & ShaderUsage::AlphaTest) != 0 && maps[0] != nullptr;
					// (its colours per vertex - a sprite's tint; and how it blends where it is see-through: added, premultiplied, or over)
					md.vertexColor = (md.gm->GetOptions() & ShaderUsage::VertexColor) != 0;
					const uint32 sf = md.gm->GetBlendingSFactor(), df = md.gm->GetBlendingDFactor();
					md.blend = (uint8)(df == BlendFunc::One ? gfx::Blend::Additive : sf == BlendFunc::One && df == BlendFunc::One_Minus_Src_Alpha ? gfx::Blend::PremultipliedAlpha : gfx::Blend::Alpha);
					if (md.alphaCut) md.baseMap = TextureFor(maps[0], true, white);
				}
			}
			if (md.skybox) { skyboxSource = md.skybox; continue; }     // (the scene's background, by direction: not its mesh)
			// (its bytes changed in place - a sprite's next frame: its GPU copy made again)
			if (md.gpu) { const uint64 c = ContentRevision(rm->Geometry); if (c != md.content) { md.gpu = MeshFor(rm->Geometry); md.content = c; } }
			if (!md.gpu || (md.cm && !md.surface)) continue;
			DrawItem it;
			it.mesh = rm; it.material = md.gm; it.custom = md.cm; it.movesItself = md.movesItself;
			it.vertexColor = md.vertexColor && md.gm && !instancedComponent; it.sortOrder = rc->GetSortOrder();
			{
				const Vec3 place = owner->GetDrawWorldPosition();
				it.sortDepth = view.orthographic ? (place - eye).dotProduct(sortForward) : (place - eye).dotProduct(place - eye);
			}
			it.gpu = md.gpu;
			it.model = owner->GetDrawWorld() * rm->Pivot;
			// (its matrix last frame, where it was drawn then: its move on screen)
			it.previousModel = md.modelFrame + 1 == frameNumber ? md.lastModel : it.model;
			for (int k = 0; k < 16 && !it.moved; k++) it.moved = it.previousModel.m[k] != it.model.m[k];
			md.lastModel = it.model; md.modelFrame = frameNumber;
			if (instancedComponent)
			{
				RenderingInstancedComponent* ic = static_cast<RenderingInstancedComponent*>(rc);
				InstancesGpu* inst = InstancesFor(ic, it.gpu);
				if (!inst) continue;
				it.instances = inst->buffer;
				it.instanceCount = std::min(ic->InstancesToDraw(), inst->count);
				if (it.instanceCount == 0) continue;
				WorldSphere(inst->center, inst->radius, it.model, it.center, it.radius);
				// (how it is drawn now: its instances grown - a thinned layer's - and faded with distance; and thinned here,
				// where it says to, by how far its nearest point is - fewer, each grown to cover the ground the others did)
				Vec2 grow = ic->GrowthToDraw();
				// (PYROS_INSTANCE_SHAPE=0: every instance drawn as it is - to compare)
				static const bool shaping = !(getenv("PYROS_INSTANCE_SHAPE") && getenv("PYROS_INSTANCE_SHAPE")[0] == '0');
				if (shaping && ic->thinTo > ic->thinFrom)
				{
					const f32 d = std::max(0.f, (it.center - eye).magnitude() - it.radius);
					const f32 t = std::min(std::max((d - ic->thinFrom) / (ic->thinTo - ic->thinFrom), 0.f), 1.f);
					const f32 keep = 1.f + (std::max(ic->thinLeast, 0.02f) - 1.f) * t * t * (3.f - 2.f * t);
					it.instanceCount = std::max(1u, std::min(it.instanceCount, (uint32)std::ceil(it.instanceCount * keep)));
					const f32 area = 1.f / keep;
					grow = Vec2(grow.x * powf(area, 0.75f), grow.y * powf(area, 0.25f));
				}
				if (shaping)
				{
					it.shape[0] = grow.x; it.shape[1] = grow.y;
					it.shape[2] = PackHalves(ic->fadeIn.x, ic->fadeIn.y); it.shape[3] = PackHalves(ic->fadeOut.x, ic->fadeOut.y);
					// (all of it faded away - nearer than its fade in begins, or further than its fade out ends: not drawn)
					if (ic->fadeIn.y > ic->fadeIn.x && (it.center - eye).magnitude() + it.radius < ic->fadeIn.x) continue;
					if (ic->fadeOut.y > ic->fadeOut.x && (it.center - eye).magnitude() - it.radius > ic->fadeOut.y) continue;
				}
			}
			else
				WorldSphere(it.gpu->center, it.gpu->radius, it.model, it.center, it.radius);
			it.distance = (it.center - eye).magnitude();
			it.caster = rc->IsCastingShadows();
			it.transparent = md.transparent;
			it.alphaCut = md.alphaCut;
			if (it.alphaCut) it.baseMap = md.baseMap;     // (the shadow's cut, and the depth prepass's)
			it.visible = true;
			// (posed by its bones: this frame's into the palette)
			{
				const std::vector<Matrix> &bones = rm->BonesToDraw();
				if (!bones.empty() && it.gpu->skin && !instancedComponent)
				{
					if (boneScratch.size() + bones.size() <= kMaxBones)
					{
						it.skinned = true;
						it.boneBase = (uint32)((frameNumber % kFramesInFlight) * kMaxBones + boneScratch.size());
						boneScratch.insert(boneScratch.end(), bones.begin(), bones.end());
					}
					else if (!warnedBones) { warnedBones = true; fprintf(stderr, "[renderer] more than %u bones in a frame: the rest drawn unposed\n", kMaxBones); }
				}
				// (its pipeline is chosen posed or not: chosen again where that changed)
				if (it.skinned != md.skinnedPipeline) { md.group = gfx::BindGroup(); md.pipeline = gfx::Pipeline(); md.pipelineInstanced = gfx::Pipeline(); }
				md.skinnedPipeline = it.skinned;
			}
			if (cullingOn && !md.movesItself) for (const Vec4 &p : planes)
				if (p.x * it.center.x + p.y * it.center.y + p.z * it.center.z + p.w < -it.radius) { it.visible = false; break; }
			// (its pipelines and bind group: made once, the group once the pipeline is ready)
			if (it.visible && !md.group)
			{
				if (md.cm)
				{
					CustomKey key; key.surface = md.surfaceHash; key.cull = (uint8)md.cull;
					key.blend = (uint8)(md.transparent ? gfx::Blend::Alpha : gfx::Blend::Opaque); key.instanced = instancedComponent ? 1 : 0;
					key.skinned = it.skinned ? 1 : 0;
					md.pipeline = CustomPipeline(key, md.surface);
					if (!instancedComponent && !md.transparent && !it.skinned) { CustomKey ik = key; ik.instanced = 1; md.pipelineInstanced = CustomPipeline(ik, md.surface); }
					if (device->PipelineReady(md.pipeline)) md.group = CustomGroupFor(md.cm, md.surface, md.pipeline);
				}
				else
				{
					PipelineKey key;
					key.cull = (uint8)md.cull;
					key.blend = md.transparent ? md.blend : (uint8)gfx::Blend::Opaque;
					key.instanced = instancedComponent ? 1 : 0;
					key.skinned = it.skinned ? 1 : 0;
					key.vertexColor = md.vertexColor && !instancedComponent && !it.skinned ? 1 : 0;
					md.pipeline = SurfacePipeline(key);
					if (!instancedComponent && !md.transparent && !it.skinned && !key.vertexColor) { PipelineKey ik = key; ik.instanced = 1; md.pipelineInstanced = SurfacePipeline(ik); }
					// (alpha-cut and opaque: its two draws as well - its cut, then its light at that depth)
					md.plain = md.pipeline; md.plainInstanced = md.pipelineInstanced; md.cut = gfx::Pipeline(); md.cutInstanced = gfx::Pipeline();
					if (md.alphaCut && !md.transparent && !it.skinned)
					{
						PipelineKey ck = key; ck.cut = 1; md.cut = SurfacePipeline(ck);
						PipelineKey lk = key; lk.cut = 2; md.pipeline = SurfacePipeline(lk);
						if (md.plainInstanced) { ck.instanced = lk.instanced = 1; md.cutInstanced = SurfacePipeline(ck); md.pipelineInstanced = SurfacePipeline(lk); }
					}
					if (device->PipelineReady(md.plain))
					{
						const std::array<gfx::Texture, 5> maps = { TextureFor(md.maps[0], true, white), TextureFor(md.maps[1], false, flatNormal),
							TextureFor(md.maps[2], false, white), TextureFor(md.maps[3], true, white), TextureFor(md.maps[4], false, white) };
						md.group = SurfaceGroupFor(md.plain, maps);
					}
				}
			}
			it.pipeline = md.pipeline; it.pipelineInstanced = md.pipelineInstanced; it.group = md.group;
			if (md.cut)
			{
				// (its two draws once all of theirs are ready - until then the one, with the cut in it)
				if (device->PipelineReady(md.cut) && device->PipelineReady(md.pipeline)) { it.cutPipeline = md.cut; it.cutPipelineInstanced = md.cutInstanced; }
				else { it.pipeline = md.plain; it.pipelineInstanced = md.plainInstanced; }
				if (it.cutPipeline && md.pipelineInstanced && !(device->PipelineReady(md.cutInstanced) && device->PipelineReady(md.pipelineInstanced))) { it.pipelineInstanced = gfx::Pipeline(); it.cutPipelineInstanced = gfx::Pipeline(); }
			}
			items.push_back(it);
		}
		notDrawnYet = 0;
		for (const DrawItem &x : items) if (x.visible && !x.group) notDrawnYet++;     // (its pipeline still being made: not drawn this frame)
		for (uint32 i = 0; i < items.size(); i++)
		{
			// (text in the world: drawn unlit, as text - the characters' colours ride in its normals)
			if (items[i].visible && items[i].material && (items[i].material->GetOptions() & ShaderUsage::TextRendering)) { worldTextOrder.push_back(i); items[i].caster = false; continue; }
			if (items[i].visible && items[i].group) (items[i].transparent ? transparentOrder : opaqueOrder).push_back(i);
		}
		// Opaque by state - pipeline, then material, then mesh; transparent from the back to the front
		std::sort(opaqueOrder.begin(), opaqueOrder.end(), [&](uint32 a, uint32 b) {
			const DrawItem &x = items[a], &y = items[b];
			if (x.pipeline.id != y.pipeline.id) return x.pipeline.id < y.pipeline.id;
			if (x.group.id != y.group.id) return x.group.id < y.group.id;
			return x.gpu < y.gpu;
		});
		// As the old renderer ordered them (a 2D scene's layering is authored against it): by their components' sort order,
		// lower first, then far to near by their owners' places; at one place, the scene's later before its earlier - its
		// stable sort of (-order, depth), drawn backwards
		std::stable_sort(transparentOrder.begin(), transparentOrder.end(), [&](uint32 a, uint32 b) {
			if (items[a].sortOrder != items[b].sortOrder) return -items[a].sortOrder < -items[b].sortOrder;
			return items[a].sortDepth < items[b].sortDepth; });
		std::reverse(transparentOrder.begin(), transparentOrder.end());
		// This frame's bones, into its slot of the palette
		if (!boneScratch.empty()) device->Upload(bonePalette, (uint64)(frameNumber % kFramesInFlight) * kMaxBones * 64, boneScratch.data(), boneScratch.size() * 64);
		// The shadow casters by how they are drawn - instanced or not, alpha-cut by which map - then by mesh: a cascade's
		// casters of the same mesh one after another, drawn as one
		casterOrder.clear();
		for (uint32 i = 0; i < items.size(); i++) if (items[i].caster) casterOrder.push_back(i);
		std::sort(casterOrder.begin(), casterOrder.end(), [&](uint32 a, uint32 b) {
			const DrawItem &x = items[a], &y = items[b];
			if (!!x.instances != !!y.instances) return !x.instances;
			if (x.alphaCut != y.alphaCut) return !x.alphaCut;
			if (x.alphaCut && x.baseMap.id != y.baseMap.id) return x.baseMap.id < y.baseMap.id;
			return x.gpu < y.gpu;
		});
		std::sort(particleSystems.begin(), particleSystems.end(), [](const std::pair<f32, ParticleSystem*> &a, const std::pair<f32, ParticleSystem*> &b) { return a.first > b.first; });
	}

	gfx::Pipeline SceneRenderer::SurfacePipeline(const PipelineKey &key)
	{
		std::map<PipelineKey, gfx::Pipeline>::iterator i = surfacePipelines.find(key);
		if (i != surfacePipelines.end()) return i->second;
		gfx::GraphicsPipelineDesc p;
		p.vertex = key.instanced ? surfaceInstVS : key.skinned ? surfaceSkinVS : key.vertexColor ? surfaceColorVS : surfaceVS;
		p.fragment = key.cut == 1 ? alphaCutFS : key.cut == 2 ? surfaceNoCutFS : surfaceFS;
		p.material = { { 0, gfx::BindingKind::Uniform, true, true, false, true }, { 1, gfx::BindingKind::StorageRead, false, true, false, true },
			{ 2, gfx::BindingKind::Texture2D, false, true }, { 3, gfx::BindingKind::Texture2D, false, true }, { 4, gfx::BindingKind::Texture2D, false, true },
			{ 5, gfx::BindingKind::Texture2D, false, true }, { 6, gfx::BindingKind::Sampler, false, true },
			{ 7, gfx::BindingKind::Uniform, false, true, false, true }, { 8, gfx::BindingKind::ShadowTexture, false, true }, { 9, gfx::BindingKind::ComparisonSampler, false, true },
			{ 10, gfx::BindingKind::Texture2D, false, true }, { 11, gfx::BindingKind::Texture2D, false, true },
			{ 12, gfx::BindingKind::Texture2D, false, true }, { 13, gfx::BindingKind::DepthTexture, false, true }, { 14, gfx::BindingKind::Sampler, false, true },
			{ 15, gfx::BindingKind::Texture2D, false, true }, { 16, gfx::BindingKind::Texture2D, false, true },
			{ 17, gfx::BindingKind::StorageRead, true, false },     // (the skinned's bones: every surface group has them)
			{ 18, gfx::BindingKind::Texture2D, false, true },     // (the height map: parallax)
			{ 19, gfx::BindingKind::Texture2D, false, true },     // (the reflection probes' atlas)
			{ 21, gfx::BindingKind::Texture2D, false, true }, { 22, gfx::BindingKind::UnfilteredTexture2D, false, true }, { 23, gfx::BindingKind::UnfilteredTexture2D, false, true },
			{ 24, gfx::BindingKind::Texture2D, false, true } };     // (the DDGI's: 24-27 kept for the renderer's own)
		p.drawDataBytes = sizeof(DrawData);
		p.bindings = { { 0, (uint16_t)sizeof(Vertex), false } };
		p.attributes = { { 0, 0, gfx::Format::RGB32F, 0 }, { 1, 0, gfx::Format::RGB32F, 12 }, { 2, 0, gfx::Format::RG32F, 24 } };
		if (key.instanced) AddInstanceStream(p);
		if (key.skinned) { p.bindings.push_back({ 2, 32, false }); p.attributes.push_back({ 8, 2, gfx::Format::RGBA32F, 0 }); p.attributes.push_back({ 9, 2, gfx::Format::RGBA32F, 16 }); }
		if (key.vertexColor) { p.bindings.push_back({ 3, 16, false }); p.attributes.push_back({ 10, 3, gfx::Format::RGBA32F, 0 }); }     // (ColorsFor)
		p.cull = key.cull == CullFace::DoubleSided ? gfx::Cull::None : key.cull == CullFace::FrontFace ? gfx::Cull::Front : gfx::Cull::Back;
		p.frontCounterClockwise = true;
		p.depthTest = true; p.depthWrite = key.blend == (uint8)gfx::Blend::Opaque; p.depthCompare = gfx::Compare::GreaterEqual;
		p.colorCount = 1; p.colorFormats[0] = kHdrFormat; p.blend[0] = (gfx::Blend)key.blend;
		p.depthFormat = kDepthFormat; p.samples = kSceneSamples;
		p.debugName = "Renderer.Surface";
		// (an alpha-cut surface's two draws: its cut, depth alone; then its light where the cut left it - the same vertex
		// stage both times, so the depths are the same to the bit)
		if (key.cut == 1) { p.colorWrite = false; p.debugName = "Renderer.Surface (cut)"; }
		if (key.cut == 2) { p.depthWrite = false; p.depthCompare = gfx::Compare::Equal; p.debugName = "Renderer.Surface (lit at the cut)"; }
		gfx::Pipeline made = device->CreatePipeline(p);
		surfacePipelines[key] = made;
		return made;
	}

	void SceneRenderer::EnsureTargets(const uint32 outW, const uint32 outH)
	{
		// (the scene's targets at the render scale's share of the view - the resolved picture, and what reads it, at the view's)
		const f32 scale = taaOn ? renderScale : 1.f;
		const uint32 width = std::max(1u, (uint32)std::lround(outW * scale)), height = std::max(1u, (uint32)std::lround(outH * scale));
		if (hdr && width == targetWidth && height == targetHeight && outW == outputWidth && outH == outputHeight) return;
		outputWidth = outW; outputHeight = outH;
		if (picture && !(picture == hdr)) device->Destroy(picture);
		picture = gfx::Texture();
		graph.Reset();
		if (overlayGroup) { device->Destroy(overlayGroup); overlayGroup = gfx::BindGroup(); }     // (it holds the prepass depth, made again)
		if (hdr) device->Destroy(hdr);
		if (hdrPrevious) device->Destroy(hdrPrevious);
		if (depth) device->Destroy(depth);
		if (msaaColor) device->Destroy(msaaColor);
		gfx::TextureDesc t; t.width = width; t.height = height;
		t.format = kHdrFormat; t.usage = gfx::TexColorTarget | gfx::TexSampled | gfx::TexCopySrc; t.debugName = "Renderer.HDR";     // (copied: last frame's, for the reflections)
		hdr = device->CreateTexture(t);
		if (width != outW || height != outH)
		{
			gfx::TextureDesc pt = t; pt.width = outW; pt.height = outH; pt.debugName = "Renderer.Picture";
			pt.usage = gfx::TexColorTarget | gfx::TexSampled | gfx::TexCopySrc | gfx::TexCopyDst;
			picture = device->CreateTexture(pt);
		}
		else picture = hdr;
		t.usage = gfx::TexSampled | gfx::TexCopySrc; t.debugName = "Renderer.HDRPrevious";
		hdrPrevious = device->CreateTexture(t);
		hdrPreviousMade = false;
		t.usage = gfx::TexColorTarget | gfx::TexSampled;
		t.samples = kSceneSamples; t.transient = true;
		t.format = kHdrFormat; t.usage = gfx::TexColorTarget; t.debugName = "Renderer.SceneMSAA";
		msaaColor = device->CreateTexture(t);
		t.format = kDepthFormat; t.usage = gfx::TexDepthTarget; t.debugName = "Renderer.Depth";
		depth = device->CreateTexture(t);
		// The AO's targets, and every bind group that names them, made again at the new size
		for (gfx::Texture x : { prepassDepth, aoA, aoB, aoRT, rtReflectTex }) if (x) device->Destroy(x);
		aoRT = gfx::Texture(); rtReflectTex = gfx::Texture();
		if (rtReflectGroup) { device->Destroy(rtReflectGroup); rtReflectGroup = gfx::BindGroup(); }
		if (rtShadowGroup) { device->Destroy(rtShadowGroup); rtShadowGroup = gfx::BindGroup(); }
		for (gfx::BindGroup* g : { &depthGroup, &gtaoGroup, &aoBlurGroupH, &aoBlurGroupV }) if (*g) { device->Destroy(*g); *g = gfx::BindGroup(); }
		for (std::map<std::array<uint32_t, 5>, gfx::BindGroup>::iterator i = surfaceGroups.begin(); i != surfaceGroups.end(); ++i) device->Destroy(i->second);
		surfaceGroups.clear();
		for (std::map<CustomShaderMaterial*, CustomGroup>::iterator i = customGroups.begin(); i != customGroups.end(); ++i) if (i->second.group) device->Destroy(i->second.group);
		customGroups.clear();
		for (std::map<CustomShaderMaterial*, CustomGroup>::iterator i = customPrepassGroups.begin(); i != customPrepassGroups.end(); ++i) if (i->second.group) device->Destroy(i->second.group);
		customPrepassGroups.clear();
		for (std::map<uint32_t, gfx::BindGroup>::iterator i = particleGroups.begin(); i != particleGroups.end(); ++i) device->Destroy(i->second);
		particleGroups.clear();
		// (and every mesh's copy of its group: chosen again - it named a group just let go, and the next frame drew with it)
		for (std::unordered_map<RenderingMesh*, MeshDraw>::iterator i = meshDraws.begin(); i != meshDraws.end(); ++i) i->second.group = gfx::BindGroup();
		for (DrawItem &it : items) it.group = gfx::BindGroup();
		gfx::TextureDesc a; a.width = width; a.height = height;
		a.format = kDepthFormat; a.usage = gfx::TexDepthTarget | gfx::TexSampled; a.debugName = "Renderer.PrepassDepth";
		prepassDepth = device->CreateTexture(a);
		a.width = std::max(1u, width / 2); a.height = std::max(1u, height / 2);
		a.format = gfx::Format::R8; a.usage = gfx::TexColorTarget | gfx::TexSampled; a.debugName = "Renderer.AO";
		aoA = device->CreateTexture(a);
		aoB = device->CreateTexture(a);
		// (the ray-traced shadows' - the AO and the sun's share, at the full size)
		if (RtShadows())
		{
			gfx::TextureDesc r; r.width = width; r.height = height; r.format = gfx::Format::RG8; r.usage = gfx::TexColorTarget | gfx::TexSampled; r.debugName = "Renderer.AOAndRTShadow";
			aoRT = device->CreateTexture(r);
		}
		if (RtReflections())
		{
			gfx::TextureDesc r; r.width = width; r.height = height; r.format = kHdrFormat; r.usage = gfx::TexColorTarget | gfx::TexSampled; r.debugName = "Renderer.RTReflections";
			rtReflectTex = device->CreateTexture(r);
		}
		// The bloom chain, from half the size down; and the groups that name it or the HDR target
		for (uint32 i = 0; i < kBloomLevels; i++)
		{
			if (bloom[i]) device->Destroy(bloom[i]);
			for (gfx::BindGroup* g : { &bloomDownGroup[i], &bloomUpGroup[i] }) if (*g) { device->Destroy(*g); *g = gfx::BindGroup(); }
			bloomW[i] = std::max(1u, outW >> (i + 1)); bloomH[i] = std::max(1u, outH >> (i + 1));
			gfx::TextureDesc bt; bt.width = bloomW[i]; bt.height = bloomH[i]; bt.format = BloomFormat(); bt.usage = gfx::TexColorTarget | gfx::TexSampled; bt.debugName = "Renderer.Bloom";
			bloom[i] = device->CreateTexture(bt);
		}
		for (gfx::BindGroup* g : { &lumGroup, &tonemapGroups[0], &tonemapGroups[1] }) if (*g) { device->Destroy(*g); *g = gfx::BindGroup(); }
		// The clouds' target, at half the size, and the sky's group that names it
		for (gfx::Texture &t : cloudTex) if (t) { device->Destroy(t); t = gfx::Texture(); }
		for (gfx::BindGroup* g : { &skyGroups[0], &skyGroups[1], &cloudsGroup[0], &cloudsGroup[1] }) if (*g) { device->Destroy(*g); *g = gfx::BindGroup(); }
		for (gfx::Texture &t : cloudTex)
		{
			gfx::TextureDesc ct; ct.width = std::max(1u, width / 2); ct.height = std::max(1u, height / 2); ct.format = gfx::Format::RGBA16F;
			ct.usage = gfx::TexColorTarget | gfx::TexSampled; ct.debugName = "Renderer.Clouds";
			t = device->CreateTexture(ct);
		}
		cloudHistory = false;     // (the new targets hold nothing to blend into)
		for (gfx::Texture &t : taaTex) if (t) { device->Destroy(t); t = gfx::Texture(); }
		for (gfx::BindGroup &g : taaGroups) if (g) { device->Destroy(g); g = gfx::BindGroup(); }
		if (postTex) device->Destroy(postTex);
		if (postGroup) { device->Destroy(postGroup); postGroup = gfx::BindGroup(); }
		{
			gfx::TextureDesc pt; pt.width = width; pt.height = height; pt.format = kHdrFormat;
			pt.usage = gfx::TexColorTarget | gfx::TexSampled | gfx::TexCopySrc; pt.debugName = "Renderer.Post";
			postTex = device->CreateTexture(pt);
		}
		if (particleTex) device->Destroy(particleTex);
		if (particleCompositeGroup) { device->Destroy(particleCompositeGroup); particleCompositeGroup = gfx::BindGroup(); }
		{
			gfx::TextureDesc pt; pt.width = std::max(1u, width / 2); pt.height = std::max(1u, height / 2); pt.format = gfx::Format::RGBA16F;
			pt.usage = gfx::TexColorTarget | gfx::TexSampled; pt.debugName = "Renderer.HalfParticles";
			particleTex = device->CreateTexture(pt);
		}
		if (velocityTex) device->Destroy(velocityTex);
		{
			gfx::TextureDesc vt; vt.width = width; vt.height = height; vt.format = gfx::Format::RG16F; vt.usage = gfx::TexColorTarget | gfx::TexSampled; vt.debugName = "Renderer.Velocity";
			velocityTex = device->CreateTexture(vt);
			graph.Transient(velocityTex); graph.Transient(aoB); graph.Transient(particleTex);     // (this frame's only)
		}
		for (gfx::Texture &t : taaTex)
		{
			gfx::TextureDesc tt; tt.width = outW; tt.height = outH; tt.format = kHdrFormat;     // (the resolved picture's size)
			tt.usage = gfx::TexColorTarget | gfx::TexSampled | gfx::TexCopySrc; tt.debugName = "Renderer.TemporalAA";
			t = device->CreateTexture(tt);
		}
		taaHistory = false;
		targetWidth = width; targetHeight = height;
	}

	// A geometry's vertex attribute, as its bytes are kept on the CPU: its own array, or - a geometry packed for the GPU
	// (a terrain tile) - its place in the interleaved buffer (kept on the CPU while the old device is off:
	// GeometryBuffer::CpuData)
	AttributeSource SceneRenderer::SourceOf(IGeometry* g, const char* name)
	{
		AttributeSource src;
		for (AttributeArray* aa : g->Attributes)
		{
			if (!aa) continue;
			for (VertexAttribute* va : aa->Attributes)
			{
				if (!va || va->Name != name) continue;
				src.count = va->DataLength;
				if (!va->Data.empty()) { src.base = &va->Data[0]; src.stride = va->byteSize; return src; }
				AttributeBuffer* ab = dynamic_cast<AttributeBuffer*>(aa);
				if (ab && ab->Buffer && ab->BufferOffset > 0 && ab->Buffer->CpuData.size() >= (size_t)ab->BufferOffset * va->DataLength)
				{
					// (its count as the buffer's size says, where the attribute was declared without one - a sprite's quads)
					if (src.count == 0) src.count = (uint32)(ab->Buffer->CpuData.size() / ab->BufferOffset);
					src.base = &ab->Buffer->CpuData[va->Offset]; src.stride = ab->BufferOffset; return src;
				}
				return src;
			}
		}
		return src;
	}
	// (or a primitive's own arrays - a terrain's distant pieces keep theirs there)
	uint64 SceneRenderer::ContentRevision(IGeometry* g)
	{
		uint64 key = (uint64)g->buffersRevision << 40;
		for (AttributeArray* aa : g->Attributes) if (aa) if (GeometryBuffer* b = aa->PackedBuffer()) key += (uint64)b->cpuRevision * 0x9E3779B1ull;
		if (g->IndexBuffer) key += (uint64)g->IndexBuffer->cpuRevision * 0x85EBCA77ull;
		return key;
	}

	void SceneRenderer::PrimitiveArrays(IGeometry* g, AttributeSource &pos, AttributeSource &nor, AttributeSource &uv)
	{
		if (pos.base) return;
		PrimitiveGeometry* pg = dynamic_cast<PrimitiveGeometry*>(g);
		if (!pg || pg->tVertex.empty()) return;
		pos.base = (const uchar*)pg->tVertex.data(); pos.stride = sizeof(Vec3); pos.count = (uint32)pg->tVertex.size();
		if (pg->tNormal.size() == pg->tVertex.size()) { nor.base = (const uchar*)pg->tNormal.data(); nor.stride = sizeof(Vec3); nor.count = pos.count; }
		if (pg->tTexcoord.size() == pg->tVertex.size()) { uv.base = (const uchar*)pg->tTexcoord.data(); uv.stride = sizeof(Vec2); uv.count = pos.count; }
	}
	// (the indices: the geometry's own, or its index buffer's bytes)
	void SceneRenderer::IndicesOf(IGeometry* g, const uint32* &data, size_t &count)
	{
		data = g->index.empty() ? nullptr : g->index.data();
		count = g->index.size();
		if (!data && g->IndexBuffer && !g->IndexBuffer->CpuData.empty())
		{
			data = (const uint32*)g->IndexBuffer->CpuData.data();
			count = g->IndexBuffer->CpuData.size() / sizeof(uint32);
		}
	}

	RenderingMesh* SceneRenderer::Pick(const View &view, const f32 x, const f32 y, f32* distance)
	{
		if (view.width == 0 || view.height == 0) return nullptr;
		// The pixel's ray: through the view's near plane and a point beyond (reversed Z: 1 the near, 0.5 further)
		const Matrix inv = (ProjectionFor(view) * view.cameraWorld.Inverse()).Inverse();
		const f32 nx = x / (f32)view.width * 2.f - 1.f, ny = 1.f - y / (f32)view.height * 2.f;
		auto unproject = [&](const f32 z) { const Vec4 p = inv * Vec4(nx, ny, z, 1.f); return Vec3(p.x / p.w, p.y / p.w, p.z / p.w); };
		const Vec3 origin = unproject(1.f), dir = (unproject(0.5f) - origin).normalize();
		RenderingMesh* best = nullptr; f32 bestT = 1e30f;
		static const bool debugPick = getenv("PYROS_RENDERER_PICK_DEBUG") != nullptr;     // (what each item did in the pick)
		if (debugPick) fprintf(stderr, "[pick] %.0f,%.0f of %ux%u: ray %.2f %.2f %.2f -> %.3f %.3f %.3f, %zu items\n", x, y, view.width, view.height, origin.x, origin.y, origin.z, dir.x, dir.y, dir.z, items.size());
		for (const DrawItem &it : items)
		{
			if (debugPick && it.mesh && it.mesh->renderingComponent && it.mesh->renderingComponent->GetOwner())
				fprintf(stderr, "[pick]   %s visible %d centre %.1f %.1f %.1f radius %.1f\n", it.mesh->renderingComponent->GetOwner()->GetName().c_str(), (int)it.visible, it.center.x, it.center.y, it.center.z, it.radius);
			if (!it.visible || !it.mesh || !it.mesh->Geometry) continue;
			// (its bounding sphere first: the ray must pass within it, nearer than the best yet)
			const Vec3 oc = it.center - origin;
			const f32 along = oc.dotProduct(dir), miss = oc.dotProduct(oc) - along * along;
			if (miss > it.radius * it.radius || along + it.radius < 0.f || along - it.radius > bestT) continue;
			AttributeSource pos = SourceOf(it.mesh->Geometry, "aPosition"), n, uv;
			PrimitiveArrays(it.mesh->Geometry, pos, n, uv);
			const uint32* index = nullptr; size_t count = 0;
			IndicesOf(it.mesh->Geometry, index, count);
			if (debugPick) fprintf(stderr, "[pick]     tested: positions %s (%u), %zu indices\n", pos.base ? "yes" : "none", pos.count, count);
			if (!pos.base || !index) continue;
			auto at = [&](const uint32 i) { Vec3 p; memcpy(&p, pos.base + (size_t)i * pos.stride, sizeof(Vec3)); const Vec4 w = it.model * Vec4(p.x, p.y, p.z, 1.f); return Vec3(w.x, w.y, w.z); };
			for (size_t k = 0; k + 2 < count; k += 3)
			{
				if (index[k] >= pos.count || index[k + 1] >= pos.count || index[k + 2] >= pos.count) continue;
				// (Moller-Trumbore, both faces)
				const Vec3 a = at(index[k]), e1 = at(index[k + 1]) - a, e2 = at(index[k + 2]) - a;
				const Vec3 p = dir.cross(e2);
				const f32 det = e1.dotProduct(p);
				if (std::fabs(det) < 1e-10f) continue;
				const f32 invDet = 1.f / det;
				const Vec3 s = origin - a;
				const f32 u = s.dotProduct(p) * invDet;
				if (u < 0.f || u > 1.f) continue;
				const Vec3 q = s.cross(e1);
				const f32 v = dir.dotProduct(q) * invDet;
				if (v < 0.f || u + v > 1.f) continue;
				const f32 t = e2.dotProduct(q) * invDet;
				if (t > 0.f && t < bestT) { bestT = t; best = it.mesh; }
			}
		}
		if (distance) *distance = bestT;
		return best;
	}

	SceneRenderer::MeshGpu* SceneRenderer::MeshFor(IGeometry* g)
	{
		MeshGpu &m = meshes[g];
		// (another geometry made where a freed one was: not its buffers - made again)
		if (m.geometryId != g->GetInstanceID())
		{
			for (gfx::Buffer b : { m.vertices, m.indices, m.colors, m.skin }) if (b) device->Destroy(b);
			m = MeshGpu();
			m.geometryId = g->GetInstanceID();
		}
		const uint64 content = ContentRevision(g);
		if (m.revision == content && m.vertices) return &m;
		// Each attribute's bytes for vertex i: its own array, or - a geometry packed for the GPU (a terrain tile) -
		// its place in the interleaved buffer (kept on the CPU while the old device is off: GeometryBuffer::CpuData)
		AttributeSource pos = SourceOf(g, "aPosition"), nor = SourceOf(g, "aNormal"), uv = SourceOf(g, "aTexcoord");
		PrimitiveArrays(g, pos, nor, uv);
		const uint32* indexData = nullptr; size_t indexCount = 0;
		IndicesOf(g, indexData, indexCount);
		if (!pos.base || !indexData || indexCount == 0)
		{
			// (said once for each geometry the renderer cannot draw, and why)
			static std::set<IGeometry*> said;
			if (said.insert(g).second)
			{
				std::string names;
				for (AttributeArray* aa : g->Attributes) if (aa) for (VertexAttribute* va : aa->Attributes) if (va) names += va->Name + "(" + std::to_string(va->Data.size()) + "/" + std::to_string(va->DataLength) + ") ";
				fprintf(stderr, "[renderer] geometry not drawn: positions %s, %zu indices; attributes: %s\n", pos.base ? "yes" : "none", indexCount, names.c_str());
			}
			return nullptr;
		}
		const uint32 count = pos.count;
		std::vector<Vertex> v(count);
		for (uint32 i = 0; i < count; i++)
		{
			memcpy(v[i].p, pos.base + i * pos.stride, sizeof(f32) * 3);
			if (nor.base && nor.count == count) memcpy(v[i].n, nor.base + i * nor.stride, sizeof(f32) * 3);
			else { v[i].n[0] = 0.f; v[i].n[1] = 1.f; v[i].n[2] = 0.f; }
			if (uv.base && uv.count == count) memcpy(v[i].uv, uv.base + i * uv.stride, sizeof(f32) * 2);
			else v[i].uv[0] = v[i].uv[1] = 0.f;
		}
		// (into the buffers it has where they hold it - a text rebuilt each frame is written, not made anew; else made,
		// with room to grow)
		const uint64 vertexBytes = v.size() * sizeof(Vertex), indexBytes = (uint64)indexCount * sizeof(uint32);
		if (m.vertices && vertexBytes <= m.vertexCapacity) device->Upload(m.vertices, 0, v.data(), vertexBytes);
		else
		{
			if (m.vertices) device->Destroy(m.vertices);
			const bool grows = (bool)m.vertices;
			m.vertexCapacity = grows ? vertexBytes * 2 : vertexBytes;
			gfx::BufferDesc vb; vb.size = m.vertexCapacity; vb.usage = gfx::BufVertex; vb.debugName = "Renderer.Vertices";
			m.vertices = device->CreateBuffer(vb, nullptr);
			device->Upload(m.vertices, 0, v.data(), vertexBytes);
		}
		if (m.indices && indexBytes <= m.indexCapacity) device->Upload(m.indices, 0, indexData, indexBytes);
		else
		{
			if (m.indices) device->Destroy(m.indices);
			const bool grows = (bool)m.indices;
			m.indexCapacity = grows ? indexBytes * 2 : indexBytes;
			gfx::BufferDesc ib; ib.size = m.indexCapacity; ib.usage = gfx::BufIndex; ib.debugName = "Renderer.Indices";
			m.indices = device->CreateBuffer(ib, nullptr);
			device->Upload(m.indices, 0, indexData, indexBytes);
		}
		m.indexCount = (uint32)indexCount;
		m.vertexCount = count;
		if (m.colors) { device->Destroy(m.colors); m.colors = gfx::Buffer(); }
		// (a skinned mesh's: each vertex's four bones and their weights, a stream of its own)
		if (m.skin) { device->Destroy(m.skin); m.skin = gfx::Buffer(); }
		{
			const AttributeSource ids = SourceOf(g, "aBonesID"), weights = SourceOf(g, "aBonesWeight");
			if (ids.base && weights.base && ids.count == count && weights.count == count)
			{
				std::vector<f32> sk((size_t)count * 8);
				for (uint32 i = 0; i < count; i++) { memcpy(&sk[(size_t)i * 8], ids.base + i * ids.stride, 16); memcpy(&sk[(size_t)i * 8 + 4], weights.base + i * weights.stride, 16); }
				gfx::BufferDesc sb; sb.size = sk.size() * 4; sb.usage = gfx::BufVertex; sb.debugName = "Renderer.Skin";
				m.skin = device->CreateBuffer(sb, sk.data());
			}
		}
		m.revision = content;
		// Its bounding sphere from its own vertices (a geometry's own bounds are not always computed)
		Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
		for (const Vertex &vx : v) { lo = Vec3(std::min(lo.x, vx.p[0]), std::min(lo.y, vx.p[1]), std::min(lo.z, vx.p[2])); hi = Vec3(std::max(hi.x, vx.p[0]), std::max(hi.y, vx.p[1]), std::max(hi.z, vx.p[2])); }
		m.center = (lo + hi) * 0.5f;
		m.radius = 0.f;
		for (const Vertex &vx : v) m.radius = std::max(m.radius, (Vec3(vx.p[0], vx.p[1], vx.p[2]) - m.center).magnitude());
		return &m;
	}

	bool SceneRenderer::SolveGlobalIllumination(SceneGraph* scene, const SceneGISettings &settings)
	{
		if (gi == ownGI.get()) SetGlobalIllumination(nullptr);
		ownGI.reset(new DDGIVolume());
		ownGIRays.reset();
		if (!scene || !SolveSceneGI(scene, settings, *ownGI)) { ownGI.reset(); return false; }
		ownGIRays.reset(new RayScene());
		if (ownGIRays->BuildFromScene(scene)) ownGIRays->Build(4); else ownGIRays.reset();
		SetGlobalIllumination(ownGI.get());
		return true;
	}

	void SceneRenderer::RefreshGlobalIllumination(SceneGraph* scene, const uint32 raysPerProbe, const uint32 probes, const f32 hysteresis, const f32 budgetMs)
	{
		if (!scene || !ownGI || !ownGIRays || gi != ownGI.get()) return;
		const RaySceneChange::Enum change = ownGIRays->RefreshTransforms();
		if (change == RaySceneChange::NeedsRebuild) { if (ownGIRays->BuildFromScene(scene)) ownGIRays->Build(4); }
		else if (change == RaySceneChange::Moved) ownGIRays->RefitBVH();
		ownGI->SetUpdateTimeBudget(budgetMs);
		UpdateSceneGI(scene, *ownGIRays, *ownGI, raysPerProbe, ownGIFrame++, hysteresis, probes);
		GlobalIlluminationChanged();
	}

	void SceneRenderer::UploadGlobalIllumination()
	{
		giUploaded = true;
		// (a volume of the shape already uploaded - a refresh, every frame of a dynamic one - written into its
		// textures; another made anew, and every surface group, which names them, made again with the targets)
		const bool same = gi && giIrradiance && giShape[0] == gi->GetIrradianceAtlas().GetWidth() && giShape[1] == gi->GetIrradianceAtlas().GetHeight()
			&& giShape[2] == gi->GetVisibilityAtlas().GetWidth() && giShape[3] == gi->GetRadianceAtlas().GetWidth() && giShape[4] == gi->ProbeCount();
		auto make = [&](gfx::Texture &tex, const gfx::TextureDesc &t, const gfx::TextureData &td) {
			if (same && tex) device->Upload(tex, &td, 1); else tex = device->CreateTexture(t, &td, 1); };
		if (!same)
		{
			for (gfx::Texture* t : { &giIrradiance, &giVisibility, &giProbes, &giRadiance }) if (*t) { device->Destroy(*t); *t = gfx::Texture(); }
			targetWidth = 0;
		}
		if (!gi || gi->ProbeCount() == 0) return;
		giShape[0] = gi->GetIrradianceAtlas().GetWidth(); giShape[1] = gi->GetIrradianceAtlas().GetHeight();
		giShape[2] = gi->GetVisibilityAtlas().GetWidth(); giShape[3] = gi->GetRadianceAtlas().GetWidth(); giShape[4] = gi->ProbeCount();
		const ProbeAtlas &irr = gi->GetIrradianceAtlas(), &vis = gi->GetVisibilityAtlas();
		if (irr.GetWidth() == 0 || vis.GetWidth() == 0) return;
		auto half = [](const f32 f) -> uint16 {
			uint32 x; memcpy(&x, &f, 4);
			const uint32 sign = (x >> 16) & 0x8000u; const int32 e = (int32)((x >> 23) & 0xff) - 127 + 15; const uint32 m = x & 0x7fffffu;
			if (e <= 0) return (uint16)sign;
			if (e >= 31) return (uint16)(sign | 0x7bffu);
			return (uint16)(sign | ((uint32)e << 10) | (m >> 13));
		};
		// Irradiance as half floats (light needs no finer, and they filter everywhere); its channels to four
		{
			const uint32 w = irr.GetWidth(), h = irr.GetHeight(), ch = irr.GetChannels();
			std::vector<uint16> px((size_t)w * h * 4, 0);
			const std::vector<f32> &d = irr.GetData();
			for (size_t i = 0; i < (size_t)w * h; i++)
				for (uint32 c = 0; c < 4; c++) px[i * 4 + c] = c < ch ? half(d[i * ch + c]) : half(1.f);
			gfx::TextureDesc t; t.width = w; t.height = h; t.format = gfx::Format::RGBA16F; t.usage = gfx::TexSampled | gfx::TexCopyDst; t.debugName = "Renderer.DDGIIrradiance";
			gfx::TextureData td; td.bytes = px.data(); td.size = px.size() * 2;
			make(giIrradiance, t, td);
		}
		// Visibility as floats (squared distances: past a half float's range at a street's scale), two channels
		{
			const uint32 w = vis.GetWidth(), h = vis.GetHeight(), ch = vis.GetChannels();
			std::vector<f32> px((size_t)w * h * 2, 0.f);
			const std::vector<f32> &d = vis.GetData();
			for (size_t i = 0; i < (size_t)w * h; i++) for (uint32 c = 0; c < 2 && c < ch; c++) px[i * 2 + c] = d[i * ch + c];
			gfx::TextureDesc t; t.width = w; t.height = h; t.format = gfx::Format::RG32F; t.usage = gfx::TexSampled | gfx::TexCopyDst; t.debugName = "Renderer.DDGIVisibility";
			gfx::TextureData td; td.bytes = px.data(); td.size = px.size() * 4;
			make(giVisibility, t, td);
		}
		// Its reflections, where it has them (half floats, every level's tiles one after another)
		const ProbeAtlas &rad = gi->GetRadianceAtlas();
		if (rad.GetResolution() > 0 && rad.GetWidth() > 0 && !rad.GetData().empty())
		{
			const uint32 w = rad.GetWidth(), h = rad.GetHeight(), ch = rad.GetChannels();
			std::vector<uint16> px((size_t)w * h * 4, 0);
			const std::vector<f32> &d = rad.GetData();
			for (size_t i = 0; i < (size_t)w * h; i++)
				for (uint32 c = 0; c < 4; c++) px[i * 4 + c] = c < ch ? half(d[i * ch + c]) : half(1.f);
			gfx::TextureDesc t; t.width = w; t.height = h; t.format = gfx::Format::RGBA16F; t.usage = gfx::TexSampled | gfx::TexCopyDst; t.debugName = "Renderer.DDGIRadiance";
			gfx::TextureData td; td.bytes = px.data(); td.size = px.size() * 2;
			make(giRadiance, t, td);
		}
		// The probes' offsets (w: 1 active), a texel each, as many a row as the atlases have
		{
			const uint32 perRow = std::max(irr.GetProbesPerRow(), 1u), rows = (gi->ProbeCount() + perRow - 1) / perRow;
			std::vector<Vec4> px((size_t)perRow * rows, Vec4(0.f, 0.f, 0.f, 1.f));
			const std::vector<Vec4> &d = gi->GetProbeData();
			for (size_t i = 0; i < d.size() && i < px.size(); i++) px[i] = d[i];
			gfx::TextureDesc t; t.width = perRow; t.height = rows; t.format = gfx::Format::RGBA32F; t.usage = gfx::TexSampled | gfx::TexCopyDst; t.debugName = "Renderer.DDGIProbes";
			gfx::TextureData td; td.bytes = px.data(); td.size = px.size() * sizeof(Vec4);
			make(giProbes, t, td);
		}
	}

	Vec3 SceneRenderer::ProbeFaceForward(const int i)
	{
		// (probefilter.frag's Forward: the same)
		static const Vec3 f[6] = { Vec3(1, 0, 0), Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, -1, 0), Vec3(0, 0, 1), Vec3(0, 0, -1) };
		return f[i];
	}
	Vec3 SceneRenderer::ProbeFaceUp(const int i) { return i == 2 ? Vec3(0, 0, 1) : i == 3 ? Vec3(0, 0, -1) : Vec3(0, 1, 0); }

	bool SceneRenderer::IsProbeCaptured(ReflectionProbe* probe) const
	{
		std::map<ReflectionProbe*, ProbeState>::const_iterator it = probeStates.find(probe);
		return it != probeStates.end() && it->second.seen && it->second.slot >= 0 && it->second.revision == probe->GetCaptureRevision();
	}

	uint32 SceneRenderer::ProbesPending(SceneGraph* scene) const
	{
		uint32 n = 0;
		for (ReflectionProbe* p : ReflectionProbes::All())
			if (p && p->GetScene() == scene && p->GetOwner() && !IsProbeCaptured(p)) n++;
		return n;
	}

	void SceneRenderer::CaptureProbe(SceneGraph* scene)
	{
		// (what was a probe and is no longer: its block free)
		const std::vector<ReflectionProbe*> &all = ReflectionProbes::All();
		for (std::map<ReflectionProbe*, ProbeState>::iterator i = probeStates.begin(); i != probeStates.end(); )
			if (std::find(all.begin(), all.end(), i->first) == all.end()) i = probeStates.erase(i); else ++i;
		// The one being seen goes on (a view a frame: a renderer's frame data is its frame's, and six views in one frame
		// would all be drawn with the last one's), else the first one waiting: never seen, asked again (Capture), or
		// moved since (one dragged in an editor)
		if (probeSeeing && std::find(all.begin(), all.end(), probeSeeing) == all.end()) probeSeeing = nullptr;
		ReflectionProbe* next = probeSeeing;
		if (!next)
			for (ReflectionProbe* p : all)
			{
				if (!p || p->GetScene() != scene || !p->GetOwner()) continue;
				const ProbeState &st = probeStates[p];
				const Vec3 at = p->GetOwner()->GetWorldPosition();
				if (!st.seen || st.revision != p->GetCaptureRevision() || (at - st.seenFrom).magnitude() > 0.01f) { next = p; break; }
			}
		if (!next || next->GetScene() != scene || !next->GetOwner()) return;
		ProbeState &st = probeStates[next];
		if (st.slot < 0)
		{
			bool used[kProbeColumns * kProbeRows] = {};
			for (const std::pair<ReflectionProbe* const, ProbeState> &o : probeStates) if (o.second.slot >= 0) used[o.second.slot] = true;
			for (int32 b = 0; b < (int32)(kProbeColumns * kProbeRows); b++) if (!used[b]) { st.slot = b; break; }
			if (st.slot < 0) { st.seen = true; st.revision = next->GetCaptureRevision(); st.seenFrom = next->GetOwner()->GetWorldPosition(); return; }     // (no block left: not reflected)
		}
		if (!device->PipelineReady(probeFilterPipeline)) return;
		if (!probeSeeing) { probeSeeing = next; probeFace = 0; probeRevision = next->GetCaptureRevision(); probeAt = next->GetOwner()->GetWorldPosition(); }
		// Its views by a renderer of its own as this one draws (its sky, clouds and ambient), no probes of its own,
		// nothing carried from frame to frame, into this renderer's six pictures
		if (!probeRenderer)
		{
			probeRenderer.reset(new SceneRenderer(device));
			probeRenderer->SetName("Reflection probes");
			probeRenderer->probesOn = false;
			probeRenderer->SetTemporalAA(false);
			probeRenderer->SetReflections(false);
		}
		SceneRenderer &r = *probeRenderer;
		r.ambient = ambient; r.background = background; r.skyOn = skyOn; r.hazeDensity = hazeDensity;
		r.cloudCover = cloudCover; r.cloudSteps = cloudSteps; r.cloudShadow = cloudShadow;
		r.surfaceCache = surfaceCache; r.lodOn = lodOn; r.cullingOn = cullingOn;
		if (r.gi != gi) r.SetGlobalIllumination(gi);     // (the probe's views lit as the scene is)
		r.SetAmbientEnvironment(ambientMode, ambientSky, ambientEquator, ambientGround, ambientSH);
		{
			const int i = probeFace;
			const Vec3 f = ProbeFaceForward(i), u = ProbeFaceUp(i), rt = f.cross(u);
			View v;
			Matrix &m = v.cameraWorld;
			m.m[0] = rt.x; m.m[1] = rt.y; m.m[2] = rt.z; m.m[3] = 0.f;
			m.m[4] = u.x; m.m[5] = u.y; m.m[6] = u.z; m.m[7] = 0.f;
			m.m[8] = -f.x; m.m[9] = -f.y; m.m[10] = -f.z; m.m[11] = 0.f;
			m.m[12] = probeAt.x; m.m[13] = probeAt.y; m.m[14] = probeAt.z; m.m[15] = 1.f;
			v.fovY = 90.f; v.nearZ = 0.05f; v.width = v.height = kProbeFace;
			v.target = probeFaceLdr; v.hdrCopy = probeFaces[i]; v.hostFrame = true; v.ui = false;
			r.Render(scene, v);
			// (a view with something not drawn yet - its pipeline still being made - is drawn again next frame)
			if (r.notDrawnYet == 0) probeFace++;
		}
		if (probeFace < 6) return;
		probeSeeing = nullptr;
		st.seen = true; st.revision = probeRevision; st.seenFrom = probeAt;
		probeFilterSlot = (uint32)st.slot;
		echo("INFO: Renderer - reflection probe '" + next->GetOwner()->GetName() + "' seen, into block " + std::to_string(st.slot));
	}

	void SceneRenderer::Render(SceneGraph* scene, const View &view)
	{
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		if (!view.hostFrame) device->BeginFrame();
		drawingInto = view.target;
		// (the weather a game set over the scene's own: its clouds and haze - and the scene's back when it lets go)
		{
			const f32 cover = RenderGlobals::CloudCover(), haze = RenderGlobals::Haze();
			if (cover >= 0.f) { if (!weatherCloud) { sceneCloudCover = cloudCover; weatherCloud = true; } cloudCover = cover; }
			else if (weatherCloud) { cloudCover = sceneCloudCover; weatherCloud = false; }
			if (haze >= 0.f) { if (!weatherHaze) { sceneHaze = hazeDensity; weatherHaze = true; } hazeDensity = haze; }
			else if (weatherHaze) { hazeDensity = sceneHaze; weatherHaze = false; }
		}
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		waitMs = std::chrono::duration<f64, std::milli>(t1 - t0).count();
		const uint32 slot = (uint32)(frameNumber % kFramesInFlight);
		const uint32 frameOffset = slot * kFrameDataSlot;
		// The scene's reflection probes: their atlas made the first time a scene has one (every surface group names it:
		// made again with the targets), and the next one waiting seen - before this frame's own passes, which filter it
		int32 probeFiltered = -1;
		if (probesOn && scene)
		{
			bool any = false;
			for (ReflectionProbe* pr : ReflectionProbes::All()) if (pr && pr->GetScene() == scene && pr->GetOwner()) { any = true; break; }
			if (any && !probeAtlas)
			{
				gfx::TextureDesc a; a.width = kProbeColumns * kProbeBlockWidth; a.height = kProbeRows * kProbeBlockHeight;
				a.format = kHdrFormat; a.usage = gfx::TexColorTarget | gfx::TexSampled; a.debugName = "Renderer.ProbeAtlas";
				probeAtlas = device->CreateTexture(a);
				a.width = a.height = kProbeFace; a.usage = gfx::TexCopyDst | gfx::TexSampled;
				for (int i = 0; i < 6; i++) { a.debugName = "Renderer.ProbeFace"; probeFaces[i] = device->CreateTexture(a); }
				a.format = device->BackbufferFormat(); a.usage = gfx::TexColorTarget | gfx::TexSampled; a.debugName = "Renderer.ProbeFacePicture";
				probeFaceLdr = device->CreateTexture(a);
				targetWidth = 0;     // (the targets and their groups made again: with the atlas in them)
			}
			if (probeAtlas)
			{
				probeFilterSlot = UINT32_MAX;
				CaptureProbe(scene);
				probeFiltered = probeFilterSlot != UINT32_MAX ? (int32)probeFilterSlot : -1;
			}
		}
		if (!giUploaded) UploadGlobalIllumination();
		// (the shaders' own tracer's scene made before the targets: too big for it, and no hardware - refused, the targets
		// made without the rays' - the shadow maps and the screen's reflections)
		if (rtRefused && rtSceneOf != scene) { rtRefused = false; rtScene.reset(); rtSceneOf = nullptr; targetWidth = 0; }     // (another scene: asked again)
		// (what the scene's sun asks for: its ray-traced shadows - with its softness - and reflections; a change, the targets
		// made again with the rays' or without)
		{
			bool sunShadows = false, sunReflections = false; f32 softness = rtSunSoftness;
			if (scene)
				for (IComponent* c : scene->GetLights())
					if (c && c->GetComponentType() == ComponentType::DirectionalLight)
					{
						DirectionalLight* sun = static_cast<DirectionalLight*>(c);
						sunShadows = sun->GetRayTracedShadows(); sunReflections = sun->GetRayTracedReflections(); softness = sun->GetRayTracedShadowSoftness();
						break;
					}
			if (sunShadows != rtSunShadows || sunReflections != rtSunReflections) { rtSunShadows = sunShadows; rtSunReflections = sunReflections; targetWidth = 0; }
			rtSunSoftness = softness;
		}
		if ((RtShadows() || RtReflections()) && !RtHardware()) PrepareRayTracing(scene);
		EnsureTargets(view.width, view.height);

		// The frame's data: camera, sun, ambient
		FrameData fd = {};
		const Matrix viewMatrix = view.cameraWorld.Inverse();
		const Matrix vp = ProjectionFor(view) * viewMatrix;
		lastViewProjection = vp;
		// (drawn a sub-pixel off each frame for the temporal anti-aliasing - Halton 2, 3 over 8 frames; culling, the sky and
		// the reprojection keep the matrix without it)
		Matrix drawn = vp;
		if (taaOn && device->PipelineReady(taaPipeline))
		{
			auto halton = [](uint32 i, uint32 b) { f32 f = 1.f, r = 0.f; while (i > 0) { f /= (f32)b; r += f * (f32)(i % b); i /= b; } return r; };
			const uint32 k = (taaFrame % 8) + 1;
			Matrix jitter;
			jitter.m[12] = (halton(k, 2) - 0.5f) * 2.f / (f32)std::max(1u, targetWidth);
			jitter.m[13] = (halton(k, 3) - 0.5f) * 2.f / (f32)std::max(1u, targetHeight);
			taaJitter[0] = jitter.m[12] * 0.5f; taaJitter[1] = -jitter.m[13] * 0.5f;
			drawn = jitter * vp;
		}
		memcpy(fd.viewProjection, drawn.m, sizeof(fd.viewProjection));
		// (and the resolve's: this frame's inverse and last frame's, both without the jitter)
		{
			const Matrix inv = vp.Inverse();
			f32 td[52];
			memcpy(td, inv.m, 64); memcpy(td + 16, taaPreviousViewProjection.m, 64); memcpy(td + 32, vp.m, 64);
			memcpy(fd.previousViewProjection, taaPreviousViewProjection.m, 64);
			td[48] = taaHistory ? 1.f : 0.f; td[49] = targetWidth < outputWidth ? 0.08f : 0.1f; td[50] = taaJitter[0]; td[51] = taaJitter[1];
			device->Upload(taaData, slot * 256, td, sizeof(td));
			taaPreviousViewProjection = vp;
		}
		const Vec3 eye = view.cameraWorld.GetTranslation();
		fd.cameraPosition[0] = eye.x; fd.cameraPosition[1] = eye.y; fd.cameraPosition[2] = eye.z; fd.cameraPosition[3] = 1.f;
		if (scene)
			for (IComponent* c : scene->GetLights())
				if (c && c->GetComponentType() == ComponentType::DirectionalLight)
				{
					Vec3 toLight;
					const Vec4 radiance = static_cast<DirectionalLight*>(c)->GetLightRadianceNow(&toLight);
					fd.sunDirection[0] = toLight.x; fd.sunDirection[1] = toLight.y; fd.sunDirection[2] = toLight.z; fd.sunDirection[3] = 1.f;
					lastSunDirection = Vec4(toLight.x, toLight.y, toLight.z, 1.f);
					// (a light's intensity is what a white diffuse surface it lights head-on gives back: the
					// illuminance it means is pi times that - the shader's Lambert term divides by pi)
					const f32 k = (f32)PI;
					fd.sunRadiance[0] = radiance.x * k; fd.sunRadiance[1] = radiance.y * k; fd.sunRadiance[2] = radiance.z * k;
					break;
				}
		// (scaled by what the game says for every view: RenderGlobals::AmbientScale)
		const f32 ambientScale = RenderGlobals::AmbientScale();
		fd.ambient[0] = ambient.x * ambientScale; fd.ambient[1] = ambient.y * ambientScale; fd.ambient[2] = ambient.z * ambientScale;
		{
			const Vec3* g[3] = { &ambientSky, &ambientEquator, &ambientGround };
			f32* to[3] = { fd.ambientSky, fd.ambientEquator, fd.ambientGround };
			for (int i = 0; i < 3; i++) { to[i][0] = g[i]->x * ambientScale; to[i][1] = g[i]->y * ambientScale; to[i][2] = g[i]->z * ambientScale; }
			fd.ambientSky[3] = (f32)ambientMode;
			for (int i = 0; i < 9; i++) { fd.ambientSH[i][0] = ambientSH[i].x * ambientScale; fd.ambientSH[i][1] = ambientSH[i].y * ambientScale; fd.ambientSH[i][2] = ambientSH[i].z * ambientScale; }
		}
		{
			const f64 now = std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
			fd.frame[0] = lastFrameTime > 0.0 ? (f32)std::min(now - lastFrameTime, 0.25) : 0.f;     // (seconds since the last frame)
			lastFrameTime = now;
		}
		fd.frame[1] = toneCurve ? exposure : -exposure;     // (negative: no curve - tonemap.frag)
		{
			// (PYROS_RENDERER_DEBUG: cascades - tinted by shadow cascade; ao-off; ao - the AO alone)
			const char* dbg = getenv("PYROS_RENDERER_DEBUG");
			const std::string d = dbg ? dbg : "";
			fd.frame[3] = d == "cascades" ? 1.f : d == "ao-off" ? 2.f : d == "ao" ? 3.f : d == "shadows-off" ? 4.f : d == "cloudshadow" ? 5.f
				: d == "albedo" ? 6.f : d == "normal" ? 7.f : d == "depth" ? 8.f : d == "ssr" ? 9.f : d == "ssrcolor" ? 10.f : 0.f;
			if (debugView >= 0) fd.frame[3] = (f32)debugView;
		}
		const bool skyLit = skyOn && fd.sunDirection[3] > 0.f;
		fd.frame[2] = skyLit ? 1.f : 0.f;
		fd.sky[0] = std::max(eye.y, 0.f) * 0.001f + 0.05f;       // (km: the scene's units are metres)
		fd.sky[1] = 0.00465f * 2.f;                              // (the sun's angular radius, a little larger than it is)
		{
			static const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
			fd.sky[2] = (f32)std::chrono::duration<f64>(std::chrono::steady_clock::now() - started).count();     // (seconds: a material's uTime)
			drawPreviousSeconds = drawSeconds > 0.f ? drawSeconds : fd.sky[2]; drawSeconds = fd.sky[2];
		}
		fd.sky[3] = skyLit ? hazeDensity : 0.f;
		fd.weather[0] = RenderGlobals::Rain();
		fd.weather[2] = view.transparent ? 1.f : 0.f;
		fd.weather[1] = RtShadows() && aoRT ? 1.f : 0.f;     // (the sun's shadow from the rays: uAO's green)
		fd.weather[3] = view.target ? 1.f : device->OutputHeadroom();     // (only what goes to the screen itself)
		{
			const Matrix proj = ProjectionFor(view);
			fd.projection[0] = proj.m[0]; fd.projection[1] = proj.m[5]; fd.projection[2] = view.nearZ;
			fd.projection[3] = view.orthographic ? std::max(view.farZ, view.nearZ + 0.01f) : 0.f;
			fd.viewport[0] = (f32)targetWidth; fd.viewport[1] = (f32)targetHeight; fd.viewport[2] = 1.f / std::max(1u, targetWidth); fd.viewport[3] = 1.f / std::max(1u, targetHeight);
			memcpy(fd.view, viewMatrix.m, sizeof(fd.view));
			for (uint32 g = 0; g < ShaderGlobals::kCount; g++) { const Vec4 &v = ShaderGlobals::Get(g); fd.globals[g][0] = v.x; fd.globals[g][1] = v.y; fd.globals[g][2] = v.z; fd.globals[g][3] = v.w; }
		}
		if (skyLit)
		{
			const Matrix inv = vp.Inverse();
			// (and the clouds': cover, extinction, the slab 1.5-3.5 km up; the wind's drift, steps)
			const f32 seconds = fd.sky[2];
			f32 sd[48] = { inv.m[0], inv.m[1], inv.m[2], inv.m[3], inv.m[4], inv.m[5], inv.m[6], inv.m[7], inv.m[8], inv.m[9], inv.m[10], inv.m[11], inv.m[12], inv.m[13], inv.m[14], inv.m[15],
				// (the cover as a scene means it - 0.52 a sky of scattered clouds: of one level of the slab, its 1.6th power)
				device->PipelineReady(cloudsPipeline) ? std::pow(cloudCover, 1.6f) : 0.f, 0.03f, 1500.f, 3500.f, seconds * 9.f, seconds * 3.f, seconds * 0.004f, (f32)cloudSteps };
			// (last frame's view, to find where these clouds were in its picture; each frame an eighth of the new)
			memcpy(&sd[24], previousViewProjection.m, sizeof(previousViewProjection.m));
			sd[40] = cloudHistory ? 1.f : 0.f; sd[41] = 0.125f; sd[42] = (f32)(cloudFrame % 1024); sd[43] = 0.f;
			// (the clouds' shadow map round the camera, 16 km, its corner snapped to whole texels: it does not crawl)
			{
				const Vec3 e = view.cameraWorld.GetTranslation();
				const f32 texel = cloudGrid[2] / 256.f;
				cloudGrid[0] = std::floor((e.x - cloudGrid[2] * 0.5f) / texel) * texel; cloudGrid[1] = std::floor((e.z - cloudGrid[2] * 0.5f) / texel) * texel;
				cloudGrid[3] = cloudShadow;
			}
			memcpy(&sd[44], cloudGrid, sizeof(cloudGrid));
			device->Upload(skyData, slot * 256, sd, sizeof(sd));
			previousViewProjection = vp;
		}
		WriteLights(scene, view, slot);
		particleSlot = slot; batchSlot = slot;
		BatchFrameStart();
		materialParamsUsed = 0;
		const std::chrono::steady_clock::time_point g0 = std::chrono::steady_clock::now();
		Gather(scene, view, vp);
		gatherMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - g0).count();
		PrepareSunShadow(scene, view, slot);
		// (with a skybox - Gather found it - the sky's table and harmonics are its, all round: the surfaces lit by the sky)
		const gfx::Texture skybox = SkyboxFor(skyboxSource);
		fd.ambient[3] = skybox ? 1.f : 0.f;
		meterBackground = skyOn || skybox;     // (a sky behind everything is light, metered; a cleared background is not)
		if (skybox) fd.frame[2] = 1.f;
		device->Upload(frameData, frameOffset, &fd, sizeof(fd));
		const uint32 dynamicOffsets[3] = { frameOffset, slot * kLightDataSlot, slot * kShadowDataSlot };

		// The frame, as a graph (FrameGraph.h): each pass what it writes and reads, recorded in this order with the
		// barriers between made by the graph
		graph.Begin();
		typedef FrameGraph::Write W;
		const gfx::Access Color = gfx::Access::ColorTarget, Depth = gfx::Access::DepthTarget;
		// The probe seen this frame, filtered into its block: a level of roughness a draw, 256 wide halving to 32
		if (probeFiltered >= 0 && device->PipelineReady(probeFilterPipeline))
		{
			if (!probeFilterGroup)
			{
				gfx::Device::BindGroupEntry e[7];
				for (int i = 0; i < 6; i++) { e[i].binding = (uint32_t)(1 + i); e[i].texture = probeFaces[i]; }
				e[6].binding = 7; e[6].sampler = linearClamp;
				probeFilterGroup = device->CreateBindGroup(probeFilterPipeline, e, 7);
			}
			const uint32 block = (uint32)probeFiltered;
			graph.AddPass("ProbeFilter", { { probeAtlas, Color } }, { probeFaces[0], probeFaces[1], probeFaces[2], probeFaces[3], probeFaces[4], probeFaces[5] }, [=](gfx::CommandList* cl)
			{
				gfx::PassDesc pass;
				pass.colorCount = 1; pass.colors[0].texture = probeAtlas; pass.colors[0].load = gfx::Load::Keep;     // (the other probes' blocks kept)
				pass.width = kProbeColumns * kProbeBlockWidth; pass.height = kProbeRows * kProbeBlockHeight; pass.debugName = "ProbeFilter";
				cl->BeginPass(pass);
				cl->SetPipeline(probeFilterPipeline);
				cl->SetBindGroup(probeFilterGroup, nullptr, 0);
				const uint32 ox = (block % kProbeColumns) * kProbeBlockWidth, oy0 = (block / kProbeColumns) * kProbeBlockHeight;
				uint32 oy = oy0;
				for (uint32 k = 0; k < 6; k++)
				{
					const uint32 lw = std::max(kProbeBlockWidth >> k, 32u), lh = lw / 2;
					cl->SetViewport((f32)ox, (f32)oy, (f32)lw, (f32)lh);
					cl->SetScissor((int32)ox, (int32)oy, lw, lh);
					// (the sharpest level as seen; the rougher ones the more samples their wider lobes want)
					const f32 dd[8] = { (f32)ox, (f32)oy, (f32)lw, (f32)lh, (f32)k / 5.f, k == 0 ? 1.f : k < 3 ? 128.f : 384.f, 0.f, 0.f };
					cl->DrawData(dd, sizeof(dd));
					cl->Draw(3, 1, 0, 0);
					oy += lh;
				}
				cl->EndPass();
			}, true);
		}
		// The sky's tables, then the sun's shadow (cleared even when off: the surfaces sample it either way)
		if (skybox)
			graph.AddPass("SkyboxTables", { { skyViewClouded, Color }, { skySHPartial, Color }, { skySH, Color } }, { skybox }, [=](gfx::CommandList* cl) { RecordSkyboxTables(cl, frameOffset); });
		else if (skyLit)
		{
			std::vector<W> w = { { skyView, Color }, { skyViewClouded, Color }, { skySHPartial, Color }, { skySH, Color } };
			if (!transmittanceMade) w.push_back({ transmittance, Color });
			graph.AddPass("SkyTables", w, { cloudNoise }, [=](gfx::CommandList* cl) { RecordSkyTables(cl, frameOffset, slot); });
		}
		if (!skybox && skyLit && cloudCover > 0.f)     // (with a skybox: its own clouds, none of these - nor their shadow)
		{
			const uint32 cur = cloudFrame & 1;
			graph.AddPass("Clouds", { { cloudTex[cur], Color } }, { cloudTex[cur ^ 1], skyView, transmittance },
				[=](gfx::CommandList* cl) { RecordClouds(cl, frameOffset, slot); });
			if (cloudShadow > 0.f) graph.AddPass("CloudShadow", { { cloudShadowTex, Color } }, { cloudNoise }, [=](gfx::CommandList* cl) { RecordCloudShadow(cl, frameOffset, slot); });
		}
		else cloudShadowOn = false;
		if (scene) graph.AddPass("Shadows", { { shadowAtlas, Depth } }, {}, [=](gfx::CommandList* cl) { RecordSunShadow(cl, scene, slot); });
		graph.AddPass("DepthAndAO", { { prepassDepth, Depth }, { velocityTex, Color }, { aoA, Color }, { aoB, Color } }, {},
			[=](gfx::CommandList* cl) { RecordDepthAndAO(cl, scene, frameOffset); });
		if (((RtShadows() && aoRT) || (RtReflections() && rtReflectTex)) && RtHardware()) BuildRayInstances();
		if (RtShadows() && aoRT)
			graph.AddPass("RTShadows", { { aoRT, Color } }, { prepassDepth, aoA }, [=](gfx::CommandList* cl) { RecordRTShadows(cl, frameOffset); });
		if (RtReflections() && rtReflectTex)
			graph.AddPass("RTReflections", { { rtReflectTex, Color } }, { prepassDepth, skySH, hdrPrevious }, [=](gfx::CommandList* cl) { RecordRTReflections(cl, frameOffset); });

		// (the scene's skybox, where a material has one: its group made here, drawn in the scene pass)
		if (skybox && !skyboxGroup && device->PipelineReady(skyboxPipeline))
		{
			gfx::Device::BindGroupEntry e[3];
			e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
			e[1].binding = 1; e[1].texture = skybox;
			e[2].binding = 2; e[2].sampler = linearClamp;
			skyboxGroup = device->CreateBindGroup(skyboxPipeline, e, 3);
		}
		const bool drawSkybox = skybox && skyboxGroup;
		// Scene pass: linear HDR, reversed Z (what the target and depth held before: not wanted)
		// (what the game asked every view to clear to, over the scene's own: RenderGlobals::BackgroundOverride)
		const Vec4 &backgroundOverride = RenderGlobals::BackgroundOverride();
		const Vec3 clearColour = backgroundOverride.w > 0.f ? SrgbToLinear(Vec3(backgroundOverride.x, backgroundOverride.y, backgroundOverride.z)) : background;
		// (the systems simulated on the GPU stepped first: their streams are what the scene draws them from)
		{
			bool anyOnGpu = false;
			for (const std::pair<f32, ParticleSystem*> &e : particleSystems) anyOnGpu = anyOnGpu || e.second->SimulatedOnGPU();
			if (anyOnGpu) graph.AddPass("ParticleSim", {}, {}, [=](gfx::CommandList* cl) { RecordParticleSim(cl); });
		}
		graph.AddPass("Scene", { { hdr, Color, true }, { depth, Depth, true, false } },
			{ shadowAtlas, prepassDepth, aoA, skySH, skyView, skyViewClouded, transmittance, cloudShadowTex, hdrPrevious, cloudTex[cloudFrame & 1], skybox },
			[=](gfx::CommandList* cl)
		{
				gfx::PassDesc pass;
				pass.colorCount = 1; pass.colors[0].texture = msaaColor; pass.colors[0].store = gfx::StoreOp::Resolve; pass.colors[0].resolve = hdr;
				pass.colors[0].clear[0] = clearColour.x; pass.colors[0].clear[1] = clearColour.y; pass.colors[0].clear[2] = clearColour.z; pass.colors[0].clear[3] = 1.f;
				pass.depth.texture = depth; pass.depth.clear = 0.f; pass.depth.store = gfx::StoreOp::DontCare;
				pass.width = targetWidth; pass.height = targetHeight; pass.debugName = "Scene";
				cl->BeginPass(pass);
				cl->SetViewport(0.f, 0.f, (f32)targetWidth, (f32)targetHeight);

				if (scene)
				{
					gfx::Pipeline bound;
					gfx::BindGroup boundGroup;
					// One draw's data: its matrix (identity for a batch - its instances carry theirs) and its material's values
					auto fillDrawData = [&](const DrawItem &it, const bool batch, DrawData &dd) { FillDrawData(it, batch, &dd); };
					// (the same mesh, material and pipeline: drawn as one)
					auto sameBatch = [&](const DrawItem &a, const DrawItem &b)
					{
						return a.gpu == b.gpu && a.material == b.material && a.custom == b.custom && a.pipeline == b.pipeline && a.group == b.group
							&& !a.instances && !b.instances && a.pipelineInstanced == b.pipelineInstanced && !a.skinned && !b.skinned;
					};
					auto drawList = [&](const std::vector<uint32> &order, const bool batching, const bool cutOnly = false)
					{
					// The runs that are drawn as one
					std::vector<std::pair<uint32, uint32> > runs;      // (where in `order`, how many)
					for (size_t k = 0; k < order.size();)
					{
						const DrawItem &a = items[order[k]];
						size_t e = k + 1;
						if (batching && a.pipelineInstanced && device->PipelineReady(a.pipelineInstanced))
							while (e < order.size() && sameBatch(a, items[order[e]])) e++;
						runs.push_back(std::make_pair((uint32)k, (uint32)(e - k)));
						k = e;
					}
					// One draw - a single item, or a batch of `count` from its instances at `batchAt` in the ring
					auto drawOne = [&](const DrawItem &it, const uint32 count, const uint64 batchAt)
					{
						const bool batch = count > 1;
						const gfx::Pipeline pipe = cutOnly ? (batch ? it.cutPipelineInstanced : it.cutPipeline) : batch ? it.pipelineInstanced : it.pipeline;
						const gfx::BindGroup group = it.group;
						if (!(pipe == bound))
						{
							cl->SetPipeline(pipe);
							bound = pipe;
							boundGroup = gfx::BindGroup();
						}
						if (it.custom)
						{
							// (its surface's parameters this frame at their own offset)
							uint64 hash = 0;
							CustomSurface* cs = SurfaceFor(it.custom, hash);
							const uint32 offsets[4] = { dynamicOffsets[0], dynamicOffsets[1], dynamicOffsets[2], cs ? WriteCustomParams(it.custom, cs) : 0 };
							cl->SetBindGroup(group, offsets, 4);
							boundGroup = gfx::BindGroup();
						}
						else if (!(group == boundGroup))
						{
							cl->SetBindGroup(group, dynamicOffsets, 3);
							boundGroup = group;
						}
						DrawData dd;
						fillDrawData(it, batch, dd);
						cl->DrawData(&dd, sizeof(dd));
						if (!batch) { DrawItemGeometry(cl, it); return; }
						cl->SetVertexBuffer(0, it.gpu->vertices, 0);
						cl->SetVertexBuffer(1, batchRing, batchAt);
						cl->SetIndexBuffer(it.gpu->indices, 0, true);
						cl->DrawIndexed(it.gpu->indexCount, count, 0, 0, 0);
					};
					for (const std::pair<uint32, uint32> &r : runs)
					{
						uint64 batchAt = 0;
						if (r.second > 1 && BatchWrite(order, r.first, r.second, batchAt)) drawOne(items[order[r.first]], r.second, batchAt);
						else for (uint32 k = 0; k < r.second; k++) drawOne(items[order[r.first + k]], 1, 0);     // (no room in the ring: one by one)
					}
					};
					// Opaque - the alpha-cut's cuts first, so their light comes at the depth they leave - then the sky behind it,
					// then what is seen through: transparent surfaces (in their order: not batched), particles
					{
						std::vector<uint32> cuts;
						for (uint32 i : opaqueOrder) if (items[i].cutPipeline) cuts.push_back(i);
						if (!cuts.empty()) drawList(cuts, true, true);
					}
					drawList(opaqueOrder, true);
				// The sky, behind everything drawn
					// (a skybox in its place, where the scene has one)
				if (drawSkybox)
				{
					cl->SetPipeline(skyboxPipeline);
					cl->SetBindGroup(skyboxGroup, &frameOffset, 1);
					cl->Draw(3, 1, 0, 0);
				}
				else if (skyLit && device->PipelineReady(skyPipeline))
					{
						gfx::BindGroup &skyGroup = skyGroups[cloudFrame & 1];
						if (!skyGroup)
						{
							gfx::Device::BindGroupEntry e[5];
							e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
							e[1].binding = 1; e[1].texture = transmittance;
							e[2].binding = 2; e[2].texture = skyView;
							e[3].binding = 3; e[3].sampler = linearClamp;
							e[4].binding = 4; e[4].buffer = skyData; e[4].size = 192;
							gfx::Device::BindGroupEntry all[6] = { e[0], e[1], e[2], e[3], e[4], gfx::Device::BindGroupEntry() };
							all[5].binding = 5; all[5].texture = cloudTex[cloudFrame & 1];
							skyGroup = device->CreateBindGroup(skyPipeline, all, 6);
						}
						const uint32 offsets[2] = { frameOffset, slot * 256 };
						cl->SetPipeline(skyPipeline);
						cl->SetBindGroup(skyGroup, offsets, 2);
						cl->Draw(3, 1, 0, 0);
					}
					bound = gfx::Pipeline(); boundGroup = gfx::BindGroup();     // (the sky changed both)
					drawList(transparentOrder, false);
					if (!worldTextOrder.empty() && device->PipelineReady(worldTextPipeline))
					{
						cl->SetPipeline(worldTextPipeline);
						for (uint32 index : worldTextOrder) DrawUIMesh(cl, items[index].mesh, vp * items[index].model, true);
					}
					if (!halfParticles) RecordParticles(cl, frameOffset);     // (else at half resolution, after this pass)
				}
				cl->EndPass();
		});
		// Over the picture: the particles at half resolution, then the temporal resolve
		if (halfParticles && !particleSystems.empty())
			graph.AddPass("HalfParticles", { { particleTex, Color }, { hdr, Color } }, { prepassDepth, skySH }, [=](gfx::CommandList* cl) { RecordHalfParticles(cl, frameOffset); });
		// The lens and shutter, where any of it is on: shafts, depth of field, motion blur (post.frag) - ahead of the temporal resolve, which smooths their sampling noise
		if ((shaftsStrength > 0.f && skyOn) || dofBlur > 0.f || motionShutter > 0.f)
			graph.AddPass("Post", { { postTex, Color } }, { hdr, velocityTex, prepassDepth }, [=](gfx::CommandList* cl) { RecordPost(cl, view, frameOffset); });
		if (taaOn)
			graph.AddPass("TemporalAA", { { taaTex[taaFrame & 1], Color } }, { hdr, taaTex[(taaFrame & 1) ^ 1], velocityTex, prepassDepth },     // (hdr read, then the result copied into it: Use())
				[=](gfx::CommandList* cl) { RecordTemporalAA(cl, frameOffset, slot); });
		// (its chain of targets its own: said within it, Use())
		graph.AddPass("BloomAndExposure", {}, { hdr, picture, prepassDepth }, [=](gfx::CommandList* cl) { RecordBloomAndExposure(cl, frameOffset); });
		// Tonemap: the HDR scene into the backbuffer, with its bloom and this frame's exposure; the UI over it
		std::vector<W> tonemapWrites;
		if (view.target) tonemapWrites.push_back({ view.target, Color, true });     // (left sampleable: its reader's)
		graph.AddPass("Tonemap", tonemapWrites, { picture, prepassDepth }, [=](gfx::CommandList* cl)
		{
				// (the display's pace shows here too: Metal's nextDrawable waits for one the display let go)
				const std::chrono::steady_clock::time_point a0 = std::chrono::steady_clock::now();
				const gfx::Texture back = view.target ? view.target : device->AcquireBackbuffer();
				acquireMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - a0).count();
				gfx::PassDesc pass;
				pass.colorCount = 1; pass.colors[0].texture = back; pass.colors[0].load = gfx::Load::DontCare;
				pass.width = view.width; pass.height = view.height; pass.debugName = "Tonemap";
				cl->BeginPass(pass);
				cl->SetViewport(0.f, 0.f, (f32)view.width, (f32)view.height);
				if (device->PipelineReady(tonemapPipeline))
				{
					const uint32 cur = (uint32)(frameNumber & 1);
					if (!tonemapGroups[cur])
					{
						gfx::Device::BindGroupEntry e[6];
						e[0].binding = 0; e[0].buffer = frameData; e[0].size = sizeof(FrameData);
						e[1].binding = 1; e[1].texture = picture;
						e[2].binding = 2; e[2].sampler = linearClamp;
						e[3].binding = 3; e[3].texture = bloom[0];
						e[4].binding = 4; e[4].texture = exposureTex[cur];
						e[5].binding = 5; e[5].texture = prepassDepth;     // (where nothing was drawn: the background as it was set)
						tonemapGroups[cur] = device->CreateBindGroup(tonemapPipeline, e, 6);
					}
					cl->SetPipeline(tonemapPipeline);
					cl->SetBindGroup(tonemapGroups[cur], &frameOffset, 1);
					cl->Draw(3, 1, 0, 0);
				}
				// The overlay (an editor's grid, gizmos, debug lines), then the UI over it, in display terms: the scene's, then its overlays'
				RecordOverlay(cl, vp, slot);
				if (view.ui)
				{
					RecordUI(cl, scene, view.width, view.height);
					for (SceneGraph* overlay : overlayScenes) if (overlay) RecordUI(cl, overlay, view.width, view.height);
				}
				cl->EndPass();
		}, true);
		// This frame's picture kept for the next one's reflections
		if ((ScreenReflections() || RtReflections()) && hdr && hdrPrevious)
		{
			graph.AddPass("KeepPicture", { { hdrPrevious, gfx::Access::CopyDst } }, {}, [=](gfx::CommandList* cl)
			{
				graph.Use(cl, { { hdr, gfx::Access::CopySrc } });
				cl->CopyTexture(hdr, hdrPrevious);
			});
			hdrPreviousMade = true;
		}
		// The picture before its exposure, where the view wants it too (a reflection probe's view)
		if (view.hdrCopy && hdr)
			graph.AddPass("CopyHDR", { { view.hdrCopy, gfx::Access::CopyDst } }, {}, [=](gfx::CommandList* cl)
			{
				graph.Use(cl, { { picture, gfx::Access::CopySrc } });
				cl->CopyTexture(picture, view.hdrCopy);
			}, true);
		gfx::CommandList* cl = device->NewCommandList("Renderer");
		graph.Execute(cl);
		overlayVertices.clear(); overlayBatches.clear();     // (the next frame says its own)
		const std::chrono::steady_clock::time_point s0 = std::chrono::steady_clock::now();
		device->Submit(&cl, 1);
		const std::chrono::steady_clock::time_point s1 = std::chrono::steady_clock::now();
		if (!view.hostFrame) device->Present();     // (else the host's, after its own over this)
		submitMs = std::chrono::duration<f64, std::milli>(s1 - s0).count();
		presentMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - s1).count();
		if (!view.hostFrame) device->EndFrame();
		frameNumber++;
		if (cloudHistory) cloudFrame++;
		cpuMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t1).count() - acquireMs;
		waitMs += acquireMs;
	}

}
