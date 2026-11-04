//============================================================================
// Name        : SceneRenderer.h
// Author      : Duarte Peixinho
// Description : The renderer - the scene drawn on the device layer (RenderDevice.h), and nothing of the old renderer.
//
//               It reads the scene as data - transforms, the meshes' vertex arrays, the materials'
//               parameters, the lights - and keeps its own GPU side of each: a mesh's buffers, the
//               pipelines. Each frame is recorded explicitly: the scene into a linear HDR target
//               (reversed Z), then tonemapped into the backbuffer. Its shaders are resources/shaders,
//               compiled to SPIR-V when the engine is built.
//============================================================================

#ifndef PYROS_RENDERER_SCENERENDERER_H
#define PYROS_RENDERER_SCENERENDERER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <Pyros3D/Rendering/Renderer/FluidVolumes.h>
#include <Pyros3D/Rendering/Renderer/FrameGraph.h>
#include <Pyros3D/Rendering/Culling/OcclusionBuffer.h>
#include <Pyros3D/Rendering/Terrain/TerrainOcclusion.h>
#include <array>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace p3d {

	class SceneGraph;
	class IGeometry;
	class GeometryBuffer;
	class Texture;
	class RenderingMesh;
	class RenderingComponent;
	class RenderingInstancedComponent;
	class ParticleSystem;
	class CustomShaderMaterial;
	class ReflectionProbe;
	class DDGIVolume;
	class RayScene;
	struct SceneGISettings;
	class IMaterial;
	class GenericShaderMaterial;


	// (a geometry's vertex attribute as its bytes are kept on the CPU - SceneRenderer::SourceOf)
	struct AttributeSource { const uchar* base = nullptr; size_t stride = 0; uint32 count = 0; };

	struct View
	{
		Matrix cameraWorld;           // the camera's world transform (it looks down its -Z)
		f32 fovY = 70.f;              // degrees
		f32 nearZ = 0.1f;             // reversed Z, infinite far plane: no far to give
		// Orthographic (2D scenes, an editor's side views, previews and bakes): orthoHeight world units from the bottom
		// of the picture to its top, its width by the aspect; nearZ to farZ in front of the eye (reversed Z)
		bool orthographic = false;
		f32 orthoHeight = 10.f, farZ = 1000.f;
		uint32 width = 0, height = 0;
		// Drawn into this - a colour target, sampled after, of the device's BackbufferFormat() and this size - not
		// the window's backbuffer: an editor's viewport, a preview. Left ready to sample (an ImGui image shows it).
		gfx::Texture target;
		// The host begins the device's frame, presents and ends it: several views a frame, its own UI over them
		bool hostFrame = false;
		// The picture before its exposure and curve - linear radiance - copied here too (of kHdrFormat and this size,
		// made with TexCopyDst: a reflection probe's view)
		gfx::Texture hdrCopy;
		// The scene's screen UI (its canvases, and the overlay scenes') drawn over it - not in a second view of the world
		// (a SceneView: a scope, a monitor) nor a reflection probe's, which see the world, not the player's HUD
		bool ui = true;
		// Where nothing was drawn, nothing: alpha 0 there - a picture to lay over something else (an inventory's figure)
		bool transparent = false;
	};

	class PYROS3D_API SceneRenderer
	{
	public:
		// (an overlay's vertex - AddOverlay: a world position, a colour as written)
		struct OverlayVertex { f32 position[3]; f32 color[4]; };
		explicit SceneRenderer(gfx::Device* device);
		~SceneRenderer();

		void SetAmbient(const Vec3 &linear) { ambient = linear; }
		// Where there is no sky: 0 the flat ambient, 1 a gradient ground - horizon - sky, 2 spherical harmonics (linear
		// radiance, order 2) - SceneSettings' ApplyAmbientEnvironment
		void SetAmbientEnvironment(const int mode, const Vec3 &sky, const Vec3 &equator, const Vec3 &ground, const Vec3 sh[9])
		{ ambientMode = mode; ambientSky = sky; ambientEquator = equator; ambientGround = ground; for (int i = 0; i < 9; i++) ambientSH[i] = sh[i]; }
		int ambientMode = 0; Vec3 ambientSky, ambientEquator, ambientGround, ambientSH[9];
		void SetBackground(const Vec3 &linear) { background = linear; }
		void SetExposure(const f32 e) { exposure = e; }
		// Off: the picture as lit, exposed and clamped - no filmic curve, no bloom (a bake, a colour map read back)
		void SetToneCurve(const bool on) { toneCurve = on; }
		// What the surfaces show in place of their light (else PYROS_RENDERER_DEBUG says): 0 their light, 1 the sun's cascades,
		// 2 no AO, 3 the AO, 4 no sun shadow, 5 the clouds' shadow, 6 albedo, 7 view-space normal, 8 depth (0..1 near to far)
		void SetDebugView(const int view) { debugView = view; }
		int debugView = -1;
		bool toneCurve = true;
		// Auto exposure (on by default): the scene's average brought to middle grey, compensated by `ev` stops; off,
		// the exposure is 2^ev
		void SetAutoExposure(const bool on, const f32 ev = 0.f) { autoExposure = on; exposureEV = ev; exposureRestart = true; }
		// The physical sky (atmosphere.glsl): drawn behind everything, and lighting the scene in place of the flat ambient
		void SetSky(const bool on) { skyOn = on; }
		// The air between the eye and what it sees (aerial perspective, with the sky): 1 the real atmosphere's, more for
		// a hazier day, 0 none
		void SetHaze(const f32 density) { hazeDensity = std::max(density, 0.f); }
		// Clouds, with the sky: how much of it they cover (0 none), and the steps along a ray (more: smoother, dearer)
		// Temporal anti-aliasing: each frame a sub-pixel off, blended into the last (on unless turned off; PYROS_RENDERER_TAA=0 too)
		// (the history forgotten only when the mode changes: a scene's settings are applied every frame - the editor's - and
		// forgotten each time, the resolve never had one, the picture shook with its jitter)
		void SetTemporalAA(const bool on) { if (on != taaOn) taaHistory = false; taaOn = on; if (on) fxaaOn = false; }
		// The anti-aliasing after the scene's 4x MSAA: TAA (the default - sub-pixel detail and shimmer resolved over
		// frames, what render scale needs), FXAA (edges found and blended in one frame: no softness, no trails), or none
		enum class AntiAliasing : uint8 { None = 0, FXAA = 1, TAA = 2 };
		// A colourist's grade on the finished picture (a scene's "Grade" effect), after the tone curve, as the old
		// renderer's GradeEffect: exposure (stops) and a highlight shoulder, white balance (temperature, tint),
		// lift / gamma / gain (rgb, and w for all), a film S-curve about a pivot, split toning (rgb, w strength),
		// saturation and vibrance, a vignette, film grain and chromatic aberration
		struct Grade
		{
			bool on = false;
			f32 exposure = 0.f, shoulder = 0.f, temperature = 0.f, tint = 0.f, contrast = 1.f, pivot = 0.43f;
			Vec4 lift = Vec4(0.f, 0.f, 0.f, 0.f), gamma = Vec4(1.f, 1.f, 1.f, 1.f), gain = Vec4(1.f, 1.f, 1.f, 1.f);
			Vec4 shadowTint = Vec4(0.f, 0.f, 0.f, 0.f), highlightTint = Vec4(0.f, 0.f, 0.f, 0.f);
			f32 saturation = 1.f, vibrance = 0.f, vignette = 0.f, grain = 0.f, chromatic = 0.f;
		};
		void SetGrade(const Grade &g) { grade = g; }
		const Grade &GetGrade() const { return grade; }
		// How much of the bloom's spread light the picture takes (a scene's Bloom uIntensity: 0.5 the usual)
		void SetBloomIntensity(const f32 intensity) { bloomIntensity = std::max(intensity, 0.f); }
		// (Bloom's uThreshold and uKnee: only what is brighter than the threshold, exposed, glows - softly from threshold - knee)
		void SetBloomThreshold(const f32 threshold, const f32 knee) { bloomThreshold = std::max(threshold, 0.f); bloomKnee = std::max(knee, 0.f); }
		// A height fog lit by the sun through its shadows - light shafts - and by the sky (a scene's VolumetricLight; the sun's
		// own DirectionalLight::SetVolumetric, where it has one, takes its place): density at its base per metre (0 off),
		// forward scattering, thinning with height per metre, its base height, how far out it is marched, steps, the sun's
		// share, the sky's share, the scattered light's colour
		struct VolumetricFog
		{
			f32 density = 0.f, anisotropy = 0.6f, falloff = 0.05f, baseHeight = 0.f, maxDistance = 150.f, steps = 32.f, scattering = 1.f, ambient = 0.2f;
			Vec3 tint = Vec3(1.f, 1.f, 1.f);
		};
		void SetVolumetricFog(const VolumetricFog &f) { fog = f; }
		// A scene's own post effects (asset effects: a .glsl as the old renderer's PostEffectChain took them - "//! effect",
		// "//! input LastRTT|Depth", "//! param <type> <name> <default>..." lines, then its main() reading
		// texture_2D(uTex0, vTexcoord) and writing FragColor), in its chain's order: run on the finished picture in display
		// terms (after the tone curve, linear 0..1 - what they were written for), its parameters as the scene set them
		struct AssetEffect { std::string name, source; std::map<std::string, std::vector<f32> > params; };
		void SetAssetEffects(const std::vector<AssetEffect> &effects) { assetEffects = effects; }
		// The ambient occlusion's reach (metres), strength (a power on what it leaves lit: 1 as it is) and its share on the
		// direct light (0 the ambient's alone; 0.2, the default, as the old renderer's SSAO) - a scene's SSAO
		void SetAmbientOcclusion(const f32 radius, const f32 strength, const f32 direct = 0.2f) { aoRadius = std::min(std::max(radius, 0.05f), 8.f); aoStrength = std::min(std::max(strength, 0.f), 4.f); aoDirect = std::min(std::max(direct, 0.f), 1.f); }
		void SetAntiAliasing(const AntiAliasing mode) { if ((mode == AntiAliasing::TAA) != taaOn) taaHistory = false; taaOn = mode == AntiAliasing::TAA; fxaaOn = mode == AntiAliasing::FXAA; }
		AntiAliasing GetAntiAliasing() const { return taaOn ? AntiAliasing::TAA : fxaaOn ? AntiAliasing::FXAA : AntiAliasing::None; }
		// A cut: what the temporal anti-aliasing kept of the last view is not the next one's
		void ResetHistory() { taaHistory = false; }
		// Particles at half resolution, composited over the picture (on unless turned off; PYROS_RENDERER_HALF_PARTICLES=0 too)
		void SetHalfResolutionParticles(const bool on) { halfParticles = on; }
		// How many visible surfaces its last frame left undrawn - their pipelines still being made (a bake draws again)
		uint32 PendingDraws() const { return notDrawnYet; }
		bool halfParticles = true;
		FrameGraph graph;     // (the frame's passes, and every texture's state between them)
		struct OverlayBatch { uint32 first, count; bool lines, depthTested; };
		std::vector<OverlayVertex> overlayVertices; std::vector<OverlayBatch> overlayBatches;
		gfx::Shader overlayVS, overlayFS; gfx::Pipeline overlayLinePipeline, overlayTrianglePipeline; gfx::BindGroup overlayGroup;
		gfx::Buffer overlayRing; uint64 overlayRingBytes = 0;
		void RecordOverlay(gfx::CommandList* cl, const Matrix &viewProjection, const uint32 slot);
		// The scene's skybox: a material's six-face cubemap, the background where it has one (in place of the sky)
		gfx::Shader skyboxFS, skyboxTableFS; gfx::Pipeline skyboxPipeline, skyboxTablePipeline; gfx::BindGroup skyboxGroup, skyboxTableGroup;
		// (its sky-view table and harmonics: what the surfaces mirror and are lit by from all round)
		void RecordSkyboxTables(gfx::CommandList* cl, const uint32 frameOffset);
		Texture* skyboxSource = nullptr;     // (this frame's, from Gather)
		Texture* skyboxMadeFrom = nullptr; uint32 skyboxRevision = 0xFFFFFFFFu; gfx::Texture skyboxTex;
		gfx::Texture SkyboxFor(Texture* source);
		gfx::Shader particleHalfFS, particleCompositeFS; gfx::Pipeline particleHalfAlphaPipeline, particleHalfAddPipeline, particleCompositePipeline;
		gfx::Texture particleTex; gfx::BindGroup particleCompositeGroup;
		// (the full-size particles' layer - sparks, rain, what is small or streaked - and its composite; the shader that
		// hides them behind the scene by the prepass depth; which layers this frame has, for the temporal resolve)
		gfx::Texture particleFullTex; gfx::BindGroup particleFullCompositeGroup; gfx::Shader particleScreenFS;
		bool particlesFull = false, particlesHalf = false;
		Vec3 backdropNow;     // (this frame's background where nothing is drawn, a scene with no sky's - linear: the tonemap keeps it as set)
		std::unique_ptr<SceneRenderer> impostorRenderer;
		// (a still water's planar reflection - RenderGlobals::MirrorLevel, sspr.vert: its picture and depth, half size)
		gfx::Texture mirrorTex, mirrorDepth; gfx::Shader ssprVS, ssprFS; gfx::Pipeline ssprPipeline; gfx::BindGroup ssprGroup;
		bool mirrorOn = false; f32 mirrorLevel = 0.f;
		void RecordMirrorPlane(gfx::CommandList* cl, const uint32 frameOffset);
		// (render targets made and not yet cleared - MakeTexture: cleared at the next frame's start)
		std::vector<std::pair<gfx::Texture, std::pair<uint32, uint32> > > pendingClears;
		gfx::Texture MakeTexture(const gfx::TextureDesc &d, const gfx::TextureData* data = nullptr, const uint32_t count = 0);     // (ImpostorBank's bakes: a renderer of its own, made on the first)
		// Each component's level of detail as drawn: the level, the one it is leaving and since when (a time crossfade)
		struct LodState { uint32 level = 0, from = 0; f64 start = -1.0; uint64 seen = 0; };
		std::unordered_map<RenderingComponent*, LodState> lodStates;
		uint32 lodStateGeneration = 0;     // (a new one each time any of lodStates is let go: RenderingComponent::lodRecord points into it)
		f64 lodClock = 0.0;
		bool ParticleWantsHalf(const ParticleSystem* ps) const;
		void RecordParticleLayer(gfx::CommandList* cl, const uint32 frameOffset, const bool half);
		// The scene is going (another is loaded in its place): what is kept by its objects' addresses - their GPU meshes,
		// textures, instances, groups - let go, as a new object may land at a freed one's address. Pipelines and compiled
		// surfaces (by their text) stay.
		void ForgetScene();
		// What is under a pixel (x, y from the picture's top left) of the view last drawn: the nearest of its drawn
		// meshes the pixel's ray meets - their triangles where it is, on the CPU (an editor's click). Instanced, skinned
		// and self-moving meshes are met as their own shape, unposed.
		RenderingMesh* Pick(const View &view, const f32 x, const f32 y, f32* distance = nullptr);
		// A material's surface (CustomShaderMaterial::SetSurfaceSource) compiled as the renderer will compile it - its
		// glslang log in `error` where it does not (an editor's feedback, before it is drawn)
		static bool CheckSurface(const std::string &source, std::string &error);
		// A surface compiled into a cache directory, as a native run writes it there (SPIR-V and WGSL, by the hash of
		// each text compiled) - what a browser build reads, as it compiles none. Build Game writes a web game's so
		static bool WriteSurfaceCache(const std::string &source, const std::string &directory, std::string &error);
		// Every the renderer alive, and a name for one (an editor's Render Targets tab lists them and what they draw into)
		static const std::vector<SceneRenderer*>& Live();
		gfx::Device* GetDevice() const { return device; }
		// Its look as another's is set: the ambient (and how it is made), background, exposure and curve, sky, haze,
		// clouds, reflections, levels of detail, culling, the solved light - a second view of the same scene
		void CopySettingsFrom(const SceneRenderer &o)
		{
			ambient = o.ambient; ambientMode = o.ambientMode; ambientSky = o.ambientSky; ambientEquator = o.ambientEquator; ambientGround = o.ambientGround;
			for (int i = 0; i < 9; i++) ambientSH[i] = o.ambientSH[i];
			background = o.background; exposure = o.exposure; toneCurve = o.toneCurve;
			if (autoExposure != o.autoExposure || exposureEV != o.exposureEV) { autoExposure = o.autoExposure; exposureEV = o.exposureEV; exposureRestart = true; }
			skyOn = o.skyOn; hazeDensity = o.hazeDensity; cloudCover = o.cloudCover; cloudSteps = o.cloudSteps; cloudShadow = o.cloudShadow;
			reflectionsOn = o.reflectionsOn; reflectionReach = o.reflectionReach; reflectionSteps = o.reflectionSteps;
			lodOn = o.lodOn; cullingOn = o.cullingOn; probesOn = o.probesOn;
			shaftsStrength = o.shaftsStrength; dofFocus = o.dofFocus; dofRange = o.dofRange; dofBlur = o.dofBlur; motionShutter = o.motionShutter;
			SetGlobalIllumination(o.gi);
		}
		gfx::Texture drawingInto;     // (this view's target while it draws: a SceneView's picture is not read into itself)
		void SetName(const std::string &n) { name = n; }
		const std::string& GetName() const { return name; }
		std::string name = "The renderer";
		// Its targets that can be looked at as pictures (not depth): the HDR picture, the AO, the motion, the bloom's top,
		// the sky tables, the clouds and their shadow, the half-size particles, the temporal history
		struct DebugTarget { const char* name; gfx::Texture texture; uint32 width, height; };
		std::vector<DebugTarget> DebugTargets() const;
		bool HasDrawn() const { return targetWidth > 0; }
		// A texture asset on the GPU as the renderer keeps it (its pixels kept on the CPU: made once, made again when they
		// change) - an editor's icons and thumbnails shown by ImGuiRenderer; sRGB for a picture
		gfx::Texture GpuTexture(Texture* source, const bool srgb) { return TextureFor(source, srgb, gfx::Texture()); }
		// A Texture whose pixels are a picture drawn on the GPU (a SceneView's): every renderer samples that picture
		// for it, not its CPU pixels. An empty one forgets it.
		static void SetViewTexture(const Texture* texture, const gfx::Texture picture);
		// (the texts a surface is compiled from: surface.frag with it in; surface.vert's three where it has a Vertex())
		static void SurfaceTexts(const std::string &source, std::vector<std::pair<std::string, bool> > &params, std::vector<std::string> &textures,
			std::string &fragment, std::string vertex[3]);
		// Drawn over the next frame's finished picture, then let go: lines (pairs) or triangles (threes) in world space,
		// colours as written (sRGB, alpha blended), hidden behind the scene where depthTested - an editor's grid, its
		// gizmos and debug lines
		void AddOverlay(const std::vector<OverlayVertex> &vertices, const bool lines, const bool depthTested);
		// The last frame's graph: its passes, which were recorded and the barriers it made (FrameGraph.h)
		const FrameGraph& Graph() const { return graph; }
		// Scenes whose UI is drawn over the frame too, after the scene's own (a game's overlay: menus, a HUD of its own)
		void SetOverlayScenes(const std::vector<SceneGraph*> &scenes) { overlayScenes = scenes; }
		std::vector<SceneGraph*> overlayScenes;
		// Screen-space reflections on glossy surfaces (the sea, wet stone, polished floors): on, how far a ray goes, its steps
		void SetReflections(const bool on, const f32 reach = 150.f, const uint32 steps = 48) { reflectionsOn = on; reflectionReach = std::max(reach, 1.f); reflectionSteps = std::min(std::max(steps, 8u), 64u); }
		void SetClouds(const f32 cover, const uint32 steps = 40, const f32 shadow = 0.45f) { cloudCover = std::min(std::max(cover, 0.f), 1.f); cloudSteps = std::min(std::max(steps, 8u), 128u); cloudShadow = std::min(std::max(shadow, 0.f), 1.f); }
		// Where the materials' surfaces compiled to SPIR-V are kept (PYROS_RENDERER_SURFACE_CACHE too): read before compiling,
		// written after - a browser build has no compiler, and reads only what a native run put there
		void SetSurfaceCache(const std::string &directory) { surfaceCache = directory; }
		// Levels of detail (each component's chosen by its distance) and frustum culling: both on unless turned off
		void SetLOD(const bool on) { lodOn = on; }
		// Levels of detail handing over by a dithered crossfade (on), or at once
		void SetLODCrossfade(const bool on) { lodCrossfade = on; }
		// Fewer pixels drawn than shown (0.5 .. 1): the scene drawn at this share of the view's size and resolved up to it by
		// the temporal anti-aliasing - its jitter reaching between the drawn pixels over the frames (with TAA off: 1)
		// Ray-traced sun shadows: a ray a pixel toward a point of the sun's disc through the scene's triangles (its
		// RayScene, kept with what moves), gathered by the temporal AA into the penumbra the sun's size makes - in place
		// of the cascades. For scenes of up to a few hundred thousand triangles; what is cut out by its texture's alpha
		// shadows as its whole triangles. sunAngle: the sun's angular radius (radians; the real one 0.0047)
		void SetRayTracedShadows(const bool on, const f32 sunAngle = 0.0093f) { if (on != rtShadowsOn) { rtShadowsOn = on; targetWidth = 0; } rtSunAngle = sunAngle; }
		bool GetRayTracedShadows() const { return rtShadowsOn; }
		bool rtShadowsOn = false; f32 rtSunAngle = 0.0093f;
		// Ray-traced reflections: the view's ray mirrored at each pixel through the scene's triangles, what it finds lit by
		// the sun and the sky - what the screen does not show too (behind the camera, under what hides it) - in place of the
		// screen's reflections, on what is smooth enough to mirror (rough under 0.4)
		void SetRayTracedReflections(const bool on) { if (on != rtReflectionsOn) { rtReflectionsOn = on; targetWidth = 0; } }
		bool GetRayTracedReflections() const { return rtReflectionsOn; }
		bool rtReflectionsOn = false;
		void SetRenderScale(const f32 scale) { renderScale = std::min(std::max(scale, 0.5f), 1.f); }
		f32 GetRenderScale() const { return renderScale; }
		f32 renderScale = 1.f;
		// The lens and the shutter (post.frag; each off at 0): light shafts from the sun through what stands against the sky
		// (strength ~0.3-1), depth of field (focused at `focus` metres, sharp `range` metres deep, `blur` the most pixels
		// out of focus), motion blur (the shutter: 1 a whole frame's move on screen; ~0.5 a film camera's)
		void SetSunShafts(const f32 strength) { shaftsStrength = std::max(strength, 0.f); }
		void SetDepthOfField(const f32 focus, const f32 range, const f32 blur) { dofFocus = std::max(focus, 0.01f); dofRange = std::max(range, 0.f); dofBlur = std::min(std::max(blur, 0.f), 24.f); }
		void SetMotionBlur(const f32 shutter) { motionShutter = std::min(std::max(shutter, 0.f), 2.f); }
		f32 shaftsStrength = 0.f, dofFocus = 10.f, dofRange = 4.f, dofBlur = 0.f, motionShutter = 0.f;
		// The scene's reflection probes (ReflectionProbe): seen by this renderer - a probe a frame, six views of it by a
		// renderer of its own - into its probe atlas, and reflected inside their boxes (on unless said)
		void SetReflectionProbes(const bool on) { probesOn = on; }
		// Whether this renderer has seen the probe as it is now (its Capture() since answered)
		bool IsProbeCaptured(ReflectionProbe* probe) const;
		// The scene's probes still to be seen
		uint32 ProbesPending(SceneGraph* scene) const;
		// A scene's solved light (DDGIVolume - SolveSceneGI): its probes' irradiance and what they see, each surface in
		// its grid lit by the eight round it as the old renderer's DDGI lights it, in place of the sky's or the flat
		// ambient. Uploaded when given, and when Changed() is said of it (a refresh); nullptr: none. Held, not owned
		void SetGlobalIllumination(const DDGIVolume* volume) { if (volume != gi) { gi = volume; giUploaded = false; } }
		void GlobalIlluminationChanged() { giUploaded = false; }
		// The same, a volume of its own: the scene's light solved on the CPU (SolveSceneGI - kept in settings.cachePath
		// when it names one) and drawn; then, for one whose light moves, a share of it traced again each frame
		// (UpdateSceneGI: as much as budgetMs takes, or `probes` of them) - what a script asks (renderer:bakeGI)
		bool SolveGlobalIllumination(SceneGraph* scene, const SceneGISettings &settings);
		void RefreshGlobalIllumination(SceneGraph* scene, const uint32 raysPerProbe, const uint32 probes, const f32 hysteresis, const f32 budgetMs = 2.f);
		std::unique_ptr<DDGIVolume> ownGI; std::unique_ptr<RayScene> ownGIRays; uint32 ownGIFrame = 0, ownGIRayCount = 128;
		void SetFrustumCulling(const bool on) { cullingOn = on; }
		bool GetLOD() const { return lodOn; }
		bool GetFrustumCulling() const { return cullingOn; }

		// One whole frame: begins it, records the passes, submits, presents and ends it
		void Render(SceneGraph* scene, const View &view);

		const gfx::FrameStats &Stats() const { return device->LastFrameStats(); }
		// CPU time of the last Render (ms): waiting for a frame slot (BeginFrame - the display's pace
		// shows here), and the work - the frame's data, recording, submitting, presenting
		f64 WaitMilliseconds() const { return waitMs; }
		f64 CpuMilliseconds() const { return cpuMs; }
		// (of the CPU's: gathering the scene, the device's submit and present; the rest is recording)
		f64 GatherMilliseconds() const { return gatherMs; }
		f64 SubmitMilliseconds() const { return submitMs; }
		f64 PresentMilliseconds() const { return presentMs; }

	private:
		struct MeshGpu
		{
			gfx::Buffer vertices, indices;
			uint32 indexCount = 0;
			uint64 revision = ~0ull;            // (ContentRevision of what it was made of)
			uint32 geometryId = 0;              // (the geometry it was made of: another at a freed one's address is not it)
			Vec3 center; f32 radius = 0.f;      // (its bounding sphere, from its own vertices)
			gfx::Buffer colors;                 // (its aColor per vertex, white where it has none - made for the UI pass)
			gfx::Buffer skin;                   // (a skinned mesh's: each vertex's four bones and their weights)
			uint32 vertexCount = 0;
			uint64 vertexCapacity = 0, indexCapacity = 0;     // (its buffers' sizes: a geometry rebuilt - a text - written into them)
			uint64 writtenAt = 0;     // (the gather that found one of its buffers written - contentScan's)
		};
		// (which geometries each buffer is of - noted as their GPU meshes are made - and how far GeometryBuffer's writes
		// have been read: each gather marks the geometries of the buffers written since)
		std::unordered_map<GeometryBuffer*, std::vector<IGeometry*> > bufferUsers;
		uint64 writtenRead = 0, contentScan = 0, contentLostAt = 0;
		void NoteBuffersWritten();
		struct PipelineKey
		{
			uint8 cull, blend, instanced = 0, skinned = 0, vertexColor = 0;
			uint8 cut = 0;     // (an alpha-cut surface's two draws: 1 its cut alone, laying down depth; 2 its light at that depth, no cut)
			uint8 impostor = 0;     // (an octahedral impostor: surface.vert/.frag IMPOSTOR - instanced)
			bool operator<(const PipelineKey &o) const { return cull != o.cull ? cull < o.cull : blend != o.blend ? blend < o.blend : instanced != o.instanced ? instanced < o.instanced
				: skinned != o.skinned ? skinned < o.skinned : vertexColor != o.vertexColor ? vertexColor < o.vertexColor : cut != o.cut ? cut < o.cut : impostor < o.impostor; }
		};

		struct TextureGpu
		{
			gfx::Texture texture;
			uint32 revision = 0xFFFFFFFFu;
			uint32 textureId = 0;     // (the texture it was made of: another at a freed one's address is not it)
			uint32 floatW = 0, floatH = 0;     // (a float texture's size, written again in place while it keeps it)
		};

		// What the frame draws: every mesh at the level of detail its distance picks, with its sphere in the world;
		// `visible` the ones in the camera's frustum, `caster` the ones that cast the sun's shadow
		struct DrawItem
		{
			RenderingMesh* mesh = nullptr; GenericShaderMaterial* material = nullptr; MeshGpu* gpu = nullptr;
			Matrix model; Vec3 center; f32 radius = 0.f, distance = 0.f;
			bool visible = false, caster = false, transparent = false;
			gfx::Pipeline pipeline, pipelineInstanced; gfx::BindGroup group;     // (pipelineInstanced: its pipeline drawing many at once)
			gfx::Pipeline cutPipeline, cutPipelineInstanced;     // (alpha-cut: its cut alone, ahead of its light at that depth - `pipeline` then)
			gfx::Buffer instances; uint32 instanceCount = 0;     // (an instanced component's: its per-instance stream)
			bool alphaCut = false; gfx::Texture baseMap;          // (an alpha-cut caster's: its shadow cut by its base colour's alpha)
			CustomShaderMaterial* custom = nullptr;              // (a material with its own surface: drawn by its compiled pipeline)
			bool movesItself = false;                            // (its surface's vertex stage moves it: not in the depth prepass)
			bool lensCut = false;                                // (not drawn inside the screen's lens: RenderingComponent::SetScreenLensCut)
			Matrix previousModel; bool moved = false;            // (its matrix last frame: its move on screen, for the temporal resolve)
			bool skinned = false; uint32 boneBase = 0;           // (posed by its bones: theirs from this frame's palette, from boneBase)
			bool vertexColor = false;                            // (its material's VertexColor: its colours a stream of their own)
			int32 sortOrder = 0;                                 // (its component's: a 2D scene's draw order among the see-through)
			f32 sortDepth = 0.f;                                 // (its owner's place from the eye: along the view looked down orthographic, else squared)
			f32 shape[4] = { 0.f, 0.f, 0.f, 0.f };              // (an instanced set's: grown and faded - instancing.glsl's ShapedInstance)
			f32 lodFade = 0.f;                                   // (a level of detail mid crossfade: + going, - coming - alphacut.frag)
			uint32 lodLevel = 0;                                 // (which of its levels this is - PYROS_RENDERER_DEBUG=lod tints by it)
			bool fades = false;                                  // (an instanced set whose plants dissolve - thinned, or past their fade)
			// (one level of an instanced set drawn level by level per instance - the GPU culling picks each instance's level
			// by its own distance: x, y the distances this level covers, z the crossfade band's share, w 1 on)
			Vec4 instanceLod; uint64 cullKey = 0;
			bool impostor = false;                               // (an octahedral impostor: not in the depth prepass, casts no shadow)
			gfx::Buffer cullVisible, cullArgs; f32 cullFade = 0.f;     // (a big instanced set culled on the GPU: its visible instances, its indirect draw)
			// What the passes take of its material and mesh, read once while the frame is prepared - the recording reads
			// nothing of the scene's objects (PrepareItems): the UVs' scale and offset, the alpha cut-off, the sway, an
			// impostor's numbers; its draw data (itemData[dataIndex]), a custom surface's parameters' offset and its prepass
			// pipeline and group, its colours' stream
			f32 uv[4] = { 1.f, 1.f, 0.f, 0.f }; f32 alphaCutoff = 0.f; f32 wind[4] = { 0.f, 0.f, 0.f, 0.f }; Vec4 impostorData;
			bool alphaTestOption = false, textRendering = false; uint8 customCull = 0;
			uint32 dataIndex = ~0u, customParams = 0;
			gfx::Pipeline customPrepass; gfx::BindGroup customPrepassGroup; gfx::Buffer colors;
		};
		// (each drawn item's draw data, made while the frame is prepared: DrawItem::dataIndex)
		struct DrawDataBlock { f32 v[64]; };     // (a draw's DrawData as bytes: SceneRenderer.cpp's DrawData, 256)
		std::vector<DrawDataBlock> itemData;
		void PrepareItems();
		// The frame's levels of detail mid crossfade: each mesh's fade (+ the level going, - the one coming)
		std::unordered_map<RenderingMesh*, f32> lodFades;
		// An instanced component's instances on the GPU: matrix and tint each (80 bytes), and the sphere round them all
		struct InstancesGpu
		{
			gfx::Buffer buffer; uint32 revision = 0xFFFFFFFFu, count = 0;
			Vec3 center; f32 radius = 0.f;
		};
		InstancesGpu* InstancesFor(RenderingInstancedComponent* component, MeshGpu* mesh);
		std::map<RenderingInstancedComponent*, InstancesGpu> instanceBuffers;
		void DrawItemGeometry(gfx::CommandList* cl, const DrawItem &it, const bool culled = false);
		// Big instanced sets (kCullInstances or more) culled instance by instance on the GPU (instance_cull.comp): for each
		// set, a frame in flight's visible stream and indirect arguments; the view's frustum
		struct InstanceCull { gfx::Buffer visible[3], args[3]; gfx::BindGroup group[3]; uint32 capacity = 0; uint64 seen = 0; };
		std::unordered_map<uint64, InstanceCull> instanceCulls;     // (by its instances and its mesh: a stream for each level's mesh)
		// (the levels of an instanced set drawn per instance this frame: each mesh's - its range, and whether its block's
		// own level, the one that casts the shadow)
		struct InstanceLodPick { Vec4 range; bool caster = false; };
		std::unordered_map<RenderingMesh*, InstanceLodPick> instanceLodPicks;
		gfx::Shader instanceCullCS; gfx::Pipeline instanceCullPipeline;
		Vec4 cullPlanes[6]; Vec3 cullEye;
		void RecordInstanceCull(gfx::CommandList* cl, const uint32 slot);
		// Materials with a surface of their own (CustomShaderMaterial::SetSurfaceSource): compiled once into surface.frag
		// (by the source's hash), a pipeline for each way it is drawn, a bind group for each material (its textures),
		// its parameters written each frame into a ring (a fourth dynamic offset)
		struct CustomSurface
		{
			gfx::Shader shader, vertex, vertexInstanced, vertexSkinned;     // (its own vertex stage, where its source has a Vertex())
			std::vector<std::pair<std::string, bool> > params;     // (name, vec4)
			std::vector<std::string> textures;
			bool failed = false;
			std::string name, prepassName;     // (its material's file, for the device's messages - "Renderer.CustomSurface Sea.glsl")
		};
		struct CustomKey
		{
			uint64 surface; uint8 cull, blend, instanced, skinned = 0;
			bool operator<(const CustomKey &o) const { return surface != o.surface ? surface < o.surface : cull != o.cull ? cull < o.cull : blend != o.blend ? blend < o.blend : instanced != o.instanced ? instanced < o.instanced : skinned < o.skinned; }
		};
		struct CustomGroup { gfx::BindGroup group; uint32 revision = 0xFFFFFFFFu; std::vector<uint32_t> textures; uint32 materialId = 0xFFFFFFFFu; uint64 checkedFrame = ~0ull; };
		CustomSurface* SurfaceFor(CustomShaderMaterial* material, uint64 &hash);
		// (SPIR-V - or, in a browser build, its WGSL - of one stage of a surface: from the cache, else compiled)
		bool SurfaceProgram(const std::string &text, const gfx::Stage stage, std::vector<uint32> &spirv, std::string &wgsl, std::string &error);
		std::string surfaceCache;
		gfx::Pipeline CustomPipeline(const CustomKey &key, CustomSurface* surface);
		// (prepass: its group for the depth prepass - the prepass's own depth not in it, a stand-in in its place)
		gfx::BindGroup CustomGroupFor(CustomShaderMaterial* material, CustomSurface* surface, gfx::Pipeline layoutOf, const bool prepass = false);
		std::map<CustomShaderMaterial*, CustomGroup> customPrepassGroups;
		gfx::Texture standInDepth; gfx::Shader noVelocityFS;
		static const uint8 kPrepassBlend = 255;           // (a CustomKey's blend for its depth prepass pipeline)
		// One draw's DrawData (surface_draw.glsl's) for an item: its matrix (identity for a batch) and its material's values
		void FillDrawData(const DrawItem &it, const bool batch, void* out);
		uint32 WriteCustomParams(CustomShaderMaterial* material, CustomSurface* surface);
		std::map<uint64, CustomSurface> customSurfaces;
		std::map<CustomKey, gfx::Pipeline> customPipelines;
		std::map<CustomShaderMaterial*, CustomGroup> customGroups;
		gfx::Buffer materialParams;
		uint32 materialParamsUsed = 0;
		// Particles: the frame's systems (back to front), their live particles streamed through a per-frame ring
		// The UI: each canvas's draw list (UICanvas), flat over the window after the tonemap - text and pictures
		// The UI as commands made while the frame is prepared (PrepareUI: its canvases solved, each mesh's buffers, texture,
		// colour and place) and recorded from those alone (RecordUI) - the recording reads nothing of the canvases
		struct UICommand { MeshGpu* mesh = nullptr; gfx::Buffer colors; gfx::BindGroup group; f32 dd[24]; int32 sx = 0, sy = 0; uint32 sw = 0, sh = 0; bool scissor = false; };
		std::vector<UICommand> uiCommands, worldTextCommands;
		bool uiReady = false;
		void PrepareUI(SceneGraph* scene, const uint32 width, const uint32 height);
		bool MakeUICommand(RenderingMesh* m, const Matrix &transform, const bool inWorld, UICommand &out);
		void RecordUI(gfx::CommandList* cl, const std::vector<UICommand> &commands, const uint32 width, const uint32 height);
		gfx::Buffer ColorsFor(IGeometry* geometry, MeshGpu* mesh);
		// (R11G11B10F where the device draws into it - half the bytes of RGBA16F; else RGBA16F)
		gfx::Format BloomFormat() const { return device->Caps().rg11b10Target ? gfx::Format::R11G11B10F : gfx::Format::RGBA16F; }
		gfx::Pipeline uiPipeline, worldTextPipeline;     // (text in the world: the UI's shaders into the scene's HDR, depth-tested)
		std::vector<uint32> worldTextOrder;
		std::vector<uint8> lightScratch;
		// Draws of the same mesh, material and pipeline one after another made one instanced draw: their matrices into
		// this ring (a frame slot each), drawn by the instanced pipeline
		gfx::Buffer batchRing; uint64 batchRingBytes = 0; uint32 batchSlot = 0;
		uint64 batchCursor = 0, batchWanted = 0;          // (this frame's use of its slot; what the frame wanted, room or not)
		std::vector<uint8> batchScratch;
		// The instances of one batch, into this frame's slot of the ring: where they went, or false where there is no room
		// (then drawn one by one - the ring is made bigger at the next frame's start)
		bool BatchWrite(const std::vector<uint32> &order, uint32 first, uint32 count, uint64 &offset);
		void BatchFrameStart();
		std::vector<uint32> casterOrder;                  // (the shadow casters by kind, alpha map, mesh: their batches)
		void DrawUIMesh(gfx::CommandList* cl, const UICommand &c);
		std::map<uint32_t, gfx::BindGroup> uiGroups;     // (by texture)
		void CreateParticles();
		void RecordParticles(gfx::CommandList* cl, const uint32 frameOffset, const bool half = false);
		std::vector<std::pair<f32, ParticleSystem*> > particleSystems;
		gfx::Pipeline particleAlphaPipeline, particleAddPipeline;
		gfx::Buffer particleRing;
		std::map<uint32_t, gfx::BindGroup> particleGroups;     // (by texture)
		// Systems simulated on the GPU (ParticleSystemDesc::gpuSimulation): each one's ring of particles and the stream
		// it draws from, stepped once a step (its GpuStep's id) by particle_sim.comp ahead of the scene pass
		struct GpuParticles { gfx::Buffer state, stream; gfx::BindGroup group; uint32 capacity = 0; uint64 step = 0, seen = 0; };
		std::unordered_map<ParticleSystem*, GpuParticles> gpuParticles;
		gfx::Shader particleSimCS; gfx::Pipeline particleSimPipeline;
		void RecordParticleSim(gfx::CommandList* cl);
		// The particles as the frame was prepared (PrepareParticles): the GPU-simulated systems' steps and every system's draw -
		// their data uploaded, groups made - so their recording reads nothing of the systems
		struct ParticleSimStep { GpuParticles* g = nullptr; f32 dd[28]; bool fresh = false; uint32 capacity = 0; };
		struct ParticleDraw { bool half = false, additive = false; gfx::BindGroup group; f32 dd[20]; gfx::Buffer stream; uint64 ringAt = 0; uint32 n = 0; };
		std::vector<ParticleSimStep> particleSteps; std::vector<ParticleDraw> particleDraws;
		void PrepareParticles();
		void ForgetGpuParticles(GpuParticles &g);
		// Volumetric smoke and fire (VolumetricSmoke - its clouds, process-wide): their voxel atlas on the GPU, marched
		// at half size (smoke.frag) and laid over the picture (smokecomposite.frag) with the screen flash
		gfx::Shader smokeFS, smokeCompositeFS, fluidCS; gfx::Pipeline smokePipeline, smokeCompositePipeline;
		gfx::Texture smokeTex, smokeReflectTex;
		// (the smoke simulated once for every renderer of a device: a scope's view drew a simulation of its own, begun when
		// the scope was first looked through - other smoke than the eye's, or none)
		std::shared_ptr<FluidVolumes> fluidsShared; FluidVolumes* fluidsP = nullptr;
		FluidVolumes &Fluids() { return *fluidsP; }
		// The beams of point and spot lights through the air (ILightComponent::SetVolumetricScattering): marched at half
		// size (beams.frag) and added to the picture (beamscomposite.frag); the frame's lamps that have one (their
		// indices in the light buffer, 16 at most) and the most steps any asks for
		gfx::Shader beamsFS, beamsCompositeFS; gfx::Pipeline beamsPipeline, beamsCompositePipeline;
		gfx::Texture beamsTex; gfx::BindGroup beamsGroup, beamsCompositeGroup;
		std::vector<uint32> beamLamps; uint32 beamSteps = 0;
		void RecordBeams(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot);
		void RecordBeamsComposite(gfx::CommandList* cl, const uint32 frameOffset);
		gfx::Buffer smokeData; gfx::BindGroup smokeGroup, smokeCompositeGroup;
		bool smokeOn = false, smokeKeptPicture = false;
		void RecordSmoke(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot);
		void PrepareSmoke(const uint32 slot);
		void RecordSmokeComposite(gfx::CommandList* cl, const uint32 frameOffset);
		// (the ray-traced shadows' scene and pass - rtshadow.frag: its triangles, nodes and leaves' lists on the GPU, and
		// the AO with the sun's share beside it, what the surfaces read for AO while it is on)
		std::unique_ptr<RayScene> rtScene; SceneGraph* rtSceneOf = nullptr;
		gfx::Buffer rtTriangles, rtNodes, rtIndices; uint64 rtTrianglesBytes = 0, rtNodesBytes = 0, rtIndicesBytes = 0;
		gfx::Texture aoRT; gfx::Shader rtShadowFS; gfx::Pipeline rtShadowPipeline; gfx::BindGroup rtShadowGroup;
		void PrepareRayTracing(SceneGraph* scene);
		void RecordRTShadows(gfx::CommandList* cl, const uint32 frameOffset);
		gfx::Texture AOForSurfaces() const { return RtShadows() && aoRT ? aoRT : aoA; }
		// Which tracer: the GPU's own ray tracing where the device has it (Caps().rayQuery - Metal, Vulkan's ray query),
		// else the shaders' own walk of the scene's BVH (WebGPU too), and where that is too many triangles for it (and no
		// hardware), neither: the shadow maps and the screen's reflections, said once
		bool rtRefused = false;
		// (asked for by the renderer's own settings, or by the scene's sun - DirectionalLight::SetRayTracedShadows/
		// Reflections, what scenes saved them as)
		bool rtSunShadows = false, rtSunReflections = false; f32 rtSunSoftness = 0.0093f;
		bool RtShadows() const { return (rtShadowsOn || rtSunShadows) && !rtRefused; }
		bool RtReflections() const { return (rtReflectionsOn || rtSunReflections) && !rtRefused; }
		bool ScreenReflections() const { return reflectionsOn || ((rtReflectionsOn || rtSunReflections) && rtRefused); }     // (the screen's: asked for, or traced ones refused)
		bool RtHardware() const { return device && device->Caps().rayQuery && rtShadowRQPipeline; }
		struct RtGeometry { gfx::RayGeometry geometry; uint64 revision = ~0ull; uint32 geometryId = 0; };
		std::unordered_map<IGeometry*, RtGeometry> rtGeometries;
		// What is hidden - behind the big still things in view (OcclusionBuffer), behind the ground (TerrainOcclusion) -
		// left out of this view: each renderer its own, from its own camera (a scope's view is another renderer)
		void HideOccluded(SceneGraph* scene, const View &view, const Matrix &projection);
		OcclusionBuffer occlusionBuffer; TerrainOcclusion terrainOcclusion;
		uint32 occlusionTick = 0, occlusionQuiet = 0; size_t occlusionLookedAt = (size_t)-1;
		Vec3 occlusionLookedFrom, occlusionLookedAlong;
		std::unordered_map<RenderingComponent*, std::pair<Vec3, bool> > underGround;     // (its centre when asked, and the answer)
		gfx::Buffer rtInstanceMaterials; uint64 rtInstanceMaterialsBytes = 0; uint32 rtInstanceCount = 0;
		gfx::Shader rtShadowRQFS, rtReflectRQFS; gfx::Pipeline rtShadowRQPipeline, rtReflectRQPipeline; gfx::BindGroup rtShadowRQGroup, rtReflectRQGroup;
		void BuildRayInstances();
		void ForgetRayTracing();
		gfx::Buffer rtMaterials; uint64 rtMaterialsBytes = 0; uint32 rtMaterialCount = 0;
		gfx::Texture rtReflectTex; gfx::Shader rtReflectFS; gfx::Pipeline rtReflectPipeline; gfx::BindGroup rtReflectGroup;
		void RecordRTReflections(gfx::CommandList* cl, const uint32 frameOffset);
		gfx::Texture PreviousForSurfaces() const { return RtReflections() && rtReflectTex ? rtReflectTex : hdrPrevious; }
		uint32 particleSlot = 0;
		uint64 particleRingBytes = 0;     // (one frame slot's share)
		void Gather(SceneGraph* scene, const View &view, const Matrix &viewProjection);
		std::vector<DrawItem> items;
		std::vector<uint32> opaqueOrder, transparentOrder;     // (indices into items: by state, and back to front)

		void CreateShaders();
		void CreateDefaults();
		void EnsureTargets(const uint32 width, const uint32 height);
		MeshGpu* MeshFor(IGeometry* geometry);
		gfx::Shader surfaceColorVS;     // (surface.vert@VERTEX_COLOR)
		static AttributeSource SourceOf(IGeometry* g, const char* name);
		// What a geometry's GPU copy is made of: its buffers' handles and every packed buffer's bytes (a sprite
		// animating changes the bytes alone)
		static uint64 ContentRevision(IGeometry* g);
		static void PrimitiveArrays(IGeometry* g, AttributeSource &pos, AttributeSource &nor, AttributeSource &uv);
		static void IndicesOf(IGeometry* g, const uint32* &data, size_t &count);
		// A texture's GPU copy (made from its CPU pixels, mips built here), or `fallback` while it has none
		// (deferrable - a material's map: past this frame's budget for making textures, the fallback this frame and the
		// texture on a later one - textureDeferred says so; PYROS_TEXTURE_BUDGET_MS, 4 by default)
		gfx::Texture TextureFor(Texture* texture, const bool srgb, const gfx::Texture fallback, const bool deferrable = false);
		f64 textureMsThisFrame = 0.0; bool textureDeferred = false;
		// (a material's texture destroyed - remade: every group made with it forgotten too, the ones kept by its id above all -
		// its id handed out again, a group found by it read the old one's view, destroyed)
		void ForgetTexture(const gfx::Texture t);
		uint32 textureEpoch = 0;     // (counted up by ForgetTexture)
		gfx::BindGroup SurfaceGroupFor(const gfx::Pipeline layoutOf, const std::array<gfx::Texture, 5> &maps);
		uint32 WriteLights(SceneGraph* scene, const View &view, const uint32 slot);
		// The sun's cascades for this view, written to the frame's slot; false when nothing casts a sun shadow
		bool PrepareSunShadow(SceneGraph* scene, const View &view, const uint32 slot);
		void RecordSunShadow(gfx::CommandList* cl, SceneGraph* scene, const uint32 slot);
		void CreateSky();
		void RecordSkyTables(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot);
		void CreateAO();
		void RecordDepthAndAO(gfx::CommandList* cl, SceneGraph* scene, const uint32 frameOffset);
		void CreatePost();
		void RecordBloomAndExposure(gfx::CommandList* cl, const uint32 frameOffset);
		gfx::Pipeline SurfacePipeline(const PipelineKey &key);

		gfx::Device* device;
		gfx::Shader surfaceVS, surfaceFS, fullscreenVS, tonemapFS, shadowVS, depthOnlyFS, transmittanceFS, skyViewFS, skyFS, skyShFS, depthVS, gtaoFS, aoBlurFS, skyShSumFS, surfaceInstVS, depthInstVS, shadowInstVS, shadowAlphaVS, shadowInstAlphaVS, shadowAlphaFS, particleVS, particleFS, uiVS, uiFS, bloomDownFS, bloomUpFS, luminanceFS, reduceFS, adaptFS;
		std::map<PipelineKey, gfx::Pipeline> surfacePipelines;
		gfx::Pipeline tonemapPipeline, shadowPipeline, shadowInstPipeline, shadowAlphaPipeline, shadowInstAlphaPipeline;
		std::map<uint32_t, gfx::BindGroup> shadowAlphaGroups;     // (by the caster's base colour map)
		gfx::BindGroup shadowGroup;
		gfx::Buffer cascadeData, shadowData;       // (each cascade's matrix for the shadow pass; the surfaces' ShadowData)
		gfx::Texture shadowAtlas;
		gfx::Sampler shadowCompare;
		Matrix cascadeMatrix[4];
		f32 cascadeRadius[4] = {}, cascadeNear[4] = {}, cascadeFar[4] = {};
		Vec3 lastSunGoes = Vec3(0.f, -1.f, 0.f);     // (the way the sun's light went last frame)
		f32 lastSunShadowFar = -1.f;     // (the sun's shadow distance last frame, 0 for none: what Gather keeps off-screen for shadows; -1 until known)
		Vec3 shadowEye, shadowForward; f32 shadowStretch = 1.f;     // (how far a shadow reaches beyond its caster, per its size)
		f32 shadowLightForward = 0.f, shadowLightDown = 1.f, shadowFloorY = 0.f;     // (the light's steepness; the lowest anything drawn reaches - how far a shadow can run)     // (the light's way along the view's: + away from the eye - a shadow runs down the cascades, - up them)
		bool sunShadowOn = false;
		// A point or spot light's shadow tile (a spot one, a point light six - a face of its cube each), this frame's
		struct LocalTile { Matrix view, viewProjection; Vec3 position; f32 tanHalf = 1.f, nearZ = 0.1f, farZ = 1.f; bool ortho = false; f32 halfSize = 0.f; f32 softness = 1.f; };     // (softness: its light's ILightComponent::GetShadowSoftness, texels)
		int rainTile = -1;     // (the local tile looking down round the eye while it rains: RainExposure)
		std::vector<LocalTile> localTiles;
		gfx::Pipeline transmittancePipeline, skyViewPipeline, skyPipeline, skyShPipeline, skyShSumPipeline;
		gfx::BindGroup transmittanceGroup, skyViewGroup, skyGroup, skyShGroup, skyShSumGroup;
		gfx::Texture transmittance, skyView, skySH, skySHPartial;
		gfx::Texture skyViewClouded;     // (the sky-view table with the clouds in it: what surfaces mirror and the sky's light is summed from)
		gfx::Pipeline cloudsDomePipeline; gfx::Shader cloudsDomeFS; gfx::BindGroup cloudsDomeGroup;
		gfx::Buffer skyData;                       // (each frame's inverse view-projection, for the sky's directions)
		bool skyOn = false, transmittanceMade = false;
		f32 hazeDensity = 1.f;
		f32 cloudCover = 0.52f, cloudShadow = 0.45f; uint32 cloudSteps = 40;
		// (the clouds' shadow map: over the ground round the camera, snapped to its texels)
		gfx::Texture cloudShadowTex; gfx::Pipeline cloudShadowPipeline; gfx::Shader cloudShadowFS; gfx::BindGroup cloudShadowGroup;
		f32 cloudGrid[4] = { 0.f, 0.f, 16000.f, 0.f }; bool cloudShadowOn = false;
		void RecordCloudShadow(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot);
		// (two targets in turn: each frame's clouds blended into the last's)
		gfx::Texture cloudNoise, cloudTex[2]; gfx::Sampler repeatLinear; gfx::Pipeline cloudsPipeline; gfx::Shader cloudsFS; gfx::BindGroup cloudsGroup[2], skyGroups[2];
		Matrix previousViewProjection; bool cloudHistory = false; uint32 cloudFrame = 0;
		void CreateClouds();
		void RecordClouds(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot);
		// Ambient occlusion: a 1x depth prepass, GTAO at half resolution, blurred both ways (aoA is the result)
		gfx::Pipeline depthPipeline, depthInstPipeline, gtaoPipeline, aoBlurPipeline;
		gfx::Shader depthAlphaVS, depthInstAlphaVS, depthAlphaFS, alphaCutFS, surfaceNoCutFS; gfx::Pipeline depthAlphaPipeline, depthInstAlphaPipeline;
		std::map<uint32_t, gfx::BindGroup> depthAlphaGroups;     // (an alpha-cut surface's prepass group, by its base colour map)
		gfx::BindGroup depthGroup, gtaoGroup, aoBlurGroupH, aoBlurGroupV;
		gfx::Texture prepassDepth, aoA, aoB;
		gfx::Sampler pointClamp;
		// Bloom (a chain of halving levels) and auto exposure (log luminance reduced to 1x1, adapted in a 1x1 pair)
		static const uint32 kBloomLevels = 6;
		bool lensCutting = false;     // (this frame's view has the screen's lens: what is cut by it is left out of the prepass)
		static const uint32 kLensBlurLevel = 2;     // (the screen's lens outside its circle: the bloom chain at an eighth, summed up from below - the picture very blurred, for nothing)
		gfx::Pipeline bloomDownPipeline, bloomUpPipeline, luminancePipeline, reducePipeline, adaptPipeline;
		gfx::Texture bloom[kBloomLevels], lum64, lum16, lum4, lum1, exposureTex[2];
		gfx::BindGroup bloomDownGroup[kBloomLevels], bloomUpGroup[kBloomLevels], lumGroup, reduceGroup[3], adaptGroup[2], tonemapGroups[2];
		uint32 bloomW[kBloomLevels] = {}, bloomH[kBloomLevels] = {};
		bool autoExposure = true, exposureRestart = true, lodOn = true, cullingOn = true, lodCrossfade = true;
		f32 exposureEV = 0.f;
		f64 lastFrameTime = 0.0;
		f32 drawSeconds = 0.f, drawPreviousSeconds = 0.f;
		bool weatherCloud = false, weatherHaze = false; f32 sceneCloudCover = 0.52f, sceneHaze = 1.f;     // (RenderGlobals' weather over the scene's: what the scene had)     // (this frame's clock and the last's - one clock for every renderer: a plant's sway, wind.glsl)
		std::map<std::array<uint32_t, 5>, gfx::BindGroup> surfaceGroups;     // (by the material's five maps)
		gfx::Buffer frameData, lightData;         // a slot for each frame in flight: given by dynamic offset
		gfx::Sampler linearClamp, materialSampler;
		gfx::Texture white, flatNormal;           // (a map a material has not: white, or the normal straight out)
		std::map<std::pair<Texture*, bool>, TextureGpu> textures;
		gfx::Texture hdr, depth, msaaColor, hdrPrevious;
		// The bones of this frame's skinned draws, one palette (a slot of it a frame in flight)
		gfx::Buffer bonePalette; std::vector<Matrix> boneScratch; bool warnedBones = false;
		gfx::Shader surfaceSkinVS, depthSkinVS, shadowSkinVS, shadowSkinAlphaVS;
		gfx::Pipeline depthSkinPipeline, shadowSkinPipeline, shadowSkinAlphaPipeline;
		gfx::Texture picture; uint32 outputWidth = 0, outputHeight = 0; f32 taaJitter[2] = { 0.f, 0.f };     // (the resolved picture at the view's size - hdr itself unless a render scale)
		gfx::Texture postTex; gfx::Shader postFS; gfx::Pipeline postPipeline; gfx::BindGroup postGroup;     // (the lens and shutter - post.frag)
		void RecordPost(gfx::CommandList* cl, const View &view, const uint32 frameOffset);
		Vec4 lastSunDirection; Matrix lastViewProjection;     // (this frame's: where the sun is on the screen)
		gfx::Texture taaTex[2], velocityTex; gfx::Shader velocityFS; gfx::Pipeline taaPipeline; gfx::Shader taaFS; gfx::BindGroup taaGroups[2]; gfx::Buffer taaData;
		bool taaOn = true, taaHistory = false; uint32 taaFrame = 0; Matrix taaPreviousViewProjection;
		Grade grade; f32 bloomThreshold = 1.f, bloomKnee = 0.5f; f32 bloomIntensity = 0.f, aoRadius = 1.f, aoStrength = 1.f, aoDirect = 0.2f;
		VolumetricFog fog, fogNow;     // (the scene's, and this frame's - the sun's own where it has one)
		// The asset effects, and each one compiled (by its source): its pipeline, its parameters' order and defaults, its
		// inputs; the picture in display terms ping-ponged between two targets, then brought to the screen (present.frag)
		std::vector<AssetEffect> assetEffects;
		struct AssetEffectGpu
		{
			gfx::Shader shader; gfx::Pipeline pipeline; bool failed = false;
			std::vector<std::string> paramNames; std::vector<Vec4> paramDefaults;
		};
		std::map<uint64, AssetEffectGpu> assetEffectGpus;
		AssetEffectGpu* AssetEffectFor(const AssetEffect &effect);
		gfx::Shader surfaceImpostorVS, surfaceImpostorPlainVS, surfaceImpostorFS, surfaceInstColorVS;     // (instanced, and one object's - a level of detail's)
		gfx::Texture displayTex[2]; gfx::Shader presentFS; gfx::Pipeline tonemapToDisplayPipeline, presentPipeline; gfx::BindGroup tonemapToDisplayGroups[2];
		bool fxaaOn = false; gfx::Shader fxaaFS; gfx::Pipeline fxaaPipeline; gfx::BindGroup fxaaGroup;
		void RecordFXAA(gfx::CommandList* cl, const uint32 frameOffset);
		void RecordTemporalAA(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot);     // (hdrPrevious: last frame's picture - what screen-space reflections find)
		// Reflection probes (CaptureProbe): their atlas - a block each, 4 across, each 256x272 (six roughnesses, 256 wide
		// halving to 32, one under the other) - the six views' pictures of the one being seen, and the renderer that sees them
		// (one a declaration: in an exported class MSVC refuses every name after the first of a list - C2487)
		static const uint32 kProbeColumns = 4;
		static const uint32 kProbeRows = 4;
		static const uint32 kProbeBlockWidth = 256;
		static const uint32 kProbeBlockHeight = 272;
		static const uint32 kProbeFace = 128;
		struct ProbeState { int32 slot = -1; uint32 revision = 0; Vec3 seenFrom; bool seen = false; };
		ReflectionProbe* probeSeeing = nullptr; int probeFace = 0; uint32 probeRevision = 0; Vec3 probeAt;     // (the one being seen, a view a frame)
		// The scene's DDGI (SetGlobalIllumination): its irradiance (RGBA16F, filtered), visibility (RG32F: squared
		// distances), probes' offsets (RGBA32F), and its grid - after the probes in the lights buffer (count.w: 1)
		const DDGIVolume* gi = nullptr; bool giUploaded = false;
		gfx::Texture giIrradiance, giVisibility, giProbes, giRadiance; uint32 giShape[5] = {};
		void UploadGlobalIllumination();
		bool meterBackground = true;     // (the exposure meters what is behind everything: a sky)
		uint32 notDrawnYet = 0;     // (the last frame's visible surfaces whose pipelines were not ready)
		bool probesOn = true; uint32 probeFilterSlot = UINT32_MAX;     // (the block of the one seen this frame)
		gfx::Texture probeAtlas, probeFaces[6], probeFaceLdr;
		gfx::Shader probeFilterFS; gfx::Pipeline probeFilterPipeline; gfx::BindGroup probeFilterGroup;
		std::unique_ptr<SceneRenderer> probeRenderer;
		std::map<ReflectionProbe*, ProbeState> probeStates;
		void CaptureProbe(SceneGraph* scene);
		static Vec3 ProbeFaceForward(const int i);
		static Vec3 ProbeFaceUp(const int i);
		bool reflectionsOn = true, hdrPreviousMade = false; f32 reflectionReach = 150.f; uint32 reflectionSteps = 48;        // (the scene drawn 4x multisampled - transient, on tile memory - resolved into hdr)
		uint32 targetWidth = 0, targetHeight = 0;
		uint64 frameNumber = 0;
		std::unordered_map<IGeometry*, MeshGpu> meshes;
		// What a mesh is drawn with, kept from frame to frame while its material, geometry and textures are the same:
		// the scene's tens of thousands of meshes are not looked up, cast and matched to pipelines every frame
		struct MeshDraw
		{
			// (first, together: what a mesh not moved and out of sight is passed over by - three cache lines, read ahead of
			// the loop - and only then the rest)
			// (where it was last frame - its owner's matrix and its pivot - and what that made: its sphere in the world. An
			// object that has not moved is not worked out again; and one neither in view nor shadowing anything in it is passed
			// over before anything else of it is looked at)
			bool placeKnown = false, movesItself = false, impostor = false;     // (impostor: an octahedral one - ShaderUsage::Impostor: its own stages, not in the prepass, no shadow)
			uint32 geometryRevision = 0xFFFFFFFFu;
			Vec3 center; f32 radius = 0.f;
			uint64 contentSeenAt = 0;     // (the gather - contentScan - in which its content was last found as its GPU mesh was made)
			IMaterial* material = nullptr; IGeometry* geometry = nullptr; MeshGpu* gpu = nullptr;
			Texture* skybox = nullptr;     // (its material's skybox cubemap: it is the background, not drawn as a mesh)
			Matrix ownerWorld, pivot;
			Matrix model;
			uint32 materialId = 0xFFFFFFFFu, geometryId = 0;     // (what they were: others at freed ones' addresses are not them)
			uint64 content = ~0ull;     // (its geometry's ContentRevision when its GPU mesh was made)
			bool vertexColor = false; uint8 blend = 1;     // (its material's VertexColor; how it blends, see-through: gfx::Blend)
			uint32 surfaceRevision = 0xFFFFFFFFu;
			GenericShaderMaterial* gm = nullptr; CustomShaderMaterial* cm = nullptr;
			CustomSurface* surface = nullptr; uint64 surfaceHash = 0;
			bool transparent = false, alphaCut = false; uint32 cull = 0;
			bool wantsCut = false;     // (drawn mid crossfade: its cut pipelines made though it cuts nothing)
			Texture* maps[5] = { nullptr, nullptr, nullptr, nullptr, nullptr }; uint32 mapRevisions[5] = { 0, 0, 0, 0, 0 };     // (colour, normal, metal-rough, emission, height)
			gfx::Pipeline pipeline, pipelineInstanced; gfx::BindGroup group; gfx::Texture baseMap;
			uint32 textureEpoch = 0;     // (SceneRenderer::textureEpoch when its group and baseMap were made: a texture forgotten since, made again)
			bool texturesPending = false;     // (its group or baseMap made with a fallback for a map not made yet: made again once it is)
			gfx::Pipeline plain, plainInstanced, cut, cutInstanced;     // (alpha-cut: its one-draw pipelines, and its cut's; `pipeline` its light at the cut's depth)
			bool instanced = false;
			Matrix lastModel; uint64 modelFrame = ~0ull;      // (its matrix, and the frame it was drawn with it)
			bool skinnedPipeline = false;
		};
		std::unordered_map<RenderingMesh*, MeshDraw> meshDraws;     // (its nodes stay where they are: RenderingMesh::drawRecord points at them)
		uint32 meshDrawGeneration = 0;     // (unique to this renderer and this table: a mesh's drawRecord from another, or from before a clear, is not used)
		static uint32 NewDrawGeneration();
		// (a material's surface by the source's hash, known while its surface is unchanged: not hashed a draw)
		struct SurfaceRef { uint32 revision = 0xFFFFFFFFu; uint64 hash = 0; CustomSurface* surface = nullptr; uint32 materialId = 0xFFFFFFFFu; };
		std::unordered_map<CustomShaderMaterial*, SurfaceRef> surfaceRefs;

		Vec3 ambient = Vec3(0.03f, 0.03f, 0.035f), background = Vec3(0.f, 0.f, 0.f);
		f32 exposure = 1.f;
		f64 cpuMs = 0.0, waitMs = 0.0, acquireMs = 0.0, gatherMs = 0.0, submitMs = 0.0, presentMs = 0.0;
	};

}

#endif /* PYROS_RENDERER_SCENERENDERER_H */
