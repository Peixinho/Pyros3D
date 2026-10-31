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
#include <Pyros3D/Rendering/Renderer/FrameGraph.h>
#include <array>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace p3d {

	class SceneGraph;
	class IGeometry;
	class Texture;
	class RenderingMesh;
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
		void SetTemporalAA(const bool on) { taaOn = on; taaHistory = false; }
		// A cut: what the temporal anti-aliasing kept of the last view is not the next one's
		void ResetHistory() { taaHistory = false; }
		// Particles at half resolution, composited over the picture (on unless turned off; PYROS_RENDERER_HALF_PARTICLES=0 too)
		void SetHalfResolutionParticles(const bool on) { halfParticles = on; }
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
		void RecordHalfParticles(gfx::CommandList* cl, const uint32 frameOffset);
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
		};
		struct PipelineKey
		{
			uint8 cull, blend, instanced = 0, skinned = 0, vertexColor = 0;
			uint8 cut = 0;     // (an alpha-cut surface's two draws: 1 its cut alone, laying down depth; 2 its light at that depth, no cut)
			bool operator<(const PipelineKey &o) const { return cull != o.cull ? cull < o.cull : blend != o.blend ? blend < o.blend : instanced != o.instanced ? instanced < o.instanced
				: skinned != o.skinned ? skinned < o.skinned : vertexColor != o.vertexColor ? vertexColor < o.vertexColor : cut < o.cut; }
		};

		struct TextureGpu
		{
			gfx::Texture texture;
			uint32 revision = 0xFFFFFFFFu;
			uint32 textureId = 0;     // (the texture it was made of: another at a freed one's address is not it)
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
			Matrix previousModel; bool moved = false;            // (its matrix last frame: its move on screen, for the temporal resolve)
			bool skinned = false; uint32 boneBase = 0;           // (posed by its bones: theirs from this frame's palette, from boneBase)
			bool vertexColor = false;                            // (its material's VertexColor: its colours a stream of their own)
			int32 sortOrder = 0;                                 // (its component's: a 2D scene's draw order among the see-through)
			f32 sortDepth = 0.f;                                 // (its owner's place from the eye: along the view looked down orthographic, else squared)
			f32 shape[4] = { 0.f, 0.f, 0.f, 0.f };              // (an instanced set's: grown and faded - instancing.glsl's ShapedInstance)
		};
		// An instanced component's instances on the GPU: matrix and tint each (80 bytes), and the sphere round them all
		struct InstancesGpu
		{
			gfx::Buffer buffer; uint32 revision = 0xFFFFFFFFu, count = 0;
			Vec3 center; f32 radius = 0.f;
		};
		InstancesGpu* InstancesFor(RenderingInstancedComponent* component, MeshGpu* mesh);
		std::map<RenderingInstancedComponent*, InstancesGpu> instanceBuffers;
		void DrawItemGeometry(gfx::CommandList* cl, const DrawItem &it);
		// Materials with a surface of their own (CustomShaderMaterial::SetSurfaceSource): compiled once into surface.frag
		// (by the source's hash), a pipeline for each way it is drawn, a bind group for each material (its textures),
		// its parameters written each frame into a ring (a fourth dynamic offset)
		struct CustomSurface
		{
			gfx::Shader shader, vertex, vertexInstanced, vertexSkinned;     // (its own vertex stage, where its source has a Vertex())
			std::vector<std::pair<std::string, bool> > params;     // (name, vec4)
			std::vector<std::string> textures;
			bool failed = false;
		};
		struct CustomKey
		{
			uint64 surface; uint8 cull, blend, instanced, skinned = 0;
			bool operator<(const CustomKey &o) const { return surface != o.surface ? surface < o.surface : cull != o.cull ? cull < o.cull : blend != o.blend ? blend < o.blend : instanced != o.instanced ? instanced < o.instanced : skinned < o.skinned; }
		};
		struct CustomGroup { gfx::BindGroup group; uint32 revision = 0xFFFFFFFFu; std::vector<uint32_t> textures; uint32 materialId = 0xFFFFFFFFu; };
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
		void RecordUI(gfx::CommandList* cl, SceneGraph* scene, const uint32 width, const uint32 height);
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
		void DrawUIMesh(gfx::CommandList* cl, RenderingMesh* m, const Matrix &transform, const bool inWorld);
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
		void ForgetGpuParticles(GpuParticles &g);
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
		bool RtShadows() const { return rtShadowsOn && !rtRefused; }
		bool RtReflections() const { return rtReflectionsOn && !rtRefused; }
		bool ScreenReflections() const { return reflectionsOn || (rtReflectionsOn && rtRefused); }     // (the screen's: asked for, or traced ones refused)
		bool RtHardware() const { return device && device->Caps().rayQuery && rtShadowRQPipeline; }
		struct RtGeometry { gfx::RayGeometry geometry; uint64 revision = ~0ull; uint32 geometryId = 0; };
		std::unordered_map<IGeometry*, RtGeometry> rtGeometries;
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
		gfx::Texture TextureFor(Texture* texture, const bool srgb, const gfx::Texture fallback);
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
		Vec3 shadowEye, shadowForward; f32 shadowStretch = 1.f;     // (how far a shadow reaches beyond its caster, per its size)
		bool sunShadowOn = false;
		// A point or spot light's shadow tile (a spot one, a point light six - a face of its cube each), this frame's
		struct LocalTile { Matrix view, viewProjection; Vec3 position; f32 tanHalf = 1.f, nearZ = 0.1f, farZ = 1.f; bool ortho = false; f32 halfSize = 0.f; };
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
		gfx::Pipeline bloomDownPipeline, bloomUpPipeline, luminancePipeline, reducePipeline, adaptPipeline;
		gfx::Texture bloom[kBloomLevels], lum64, lum16, lum4, lum1, exposureTex[2];
		gfx::BindGroup bloomDownGroup[kBloomLevels], bloomUpGroup[kBloomLevels], lumGroup, reduceGroup[3], adaptGroup[2], tonemapGroups[2];
		uint32 bloomW[kBloomLevels] = {}, bloomH[kBloomLevels] = {};
		bool autoExposure = true, exposureRestart = true, lodOn = true, cullingOn = true;
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
		void RecordTemporalAA(gfx::CommandList* cl, const uint32 frameOffset, const uint32 slot);     // (hdrPrevious: last frame's picture - what screen-space reflections find)
		// Reflection probes (CaptureProbe): their atlas - a block each, 4 across, each 256x272 (six roughnesses, 256 wide
		// halving to 32, one under the other) - the six views' pictures of the one being seen, and the renderer that sees them
		static const uint32 kProbeColumns = 4, kProbeRows = 4, kProbeBlockWidth = 256, kProbeBlockHeight = 272, kProbeFace = 128;
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
			IMaterial* material = nullptr; IGeometry* geometry = nullptr;
			uint32 materialId = 0xFFFFFFFFu, geometryId = 0;     // (what they were: others at freed ones' addresses are not them)
			uint64 content = ~0ull;     // (its geometry's ContentRevision when its GPU mesh was made)
			bool vertexColor = false; uint8 blend = 1;     // (its material's VertexColor; how it blends, see-through: gfx::Blend)
			uint32 geometryRevision = 0xFFFFFFFFu, surfaceRevision = 0xFFFFFFFFu;
			GenericShaderMaterial* gm = nullptr; CustomShaderMaterial* cm = nullptr;
			MeshGpu* gpu = nullptr; CustomSurface* surface = nullptr; uint64 surfaceHash = 0;
			bool transparent = false, alphaCut = false, movesItself = false; uint32 cull = 0;
			Texture* skybox = nullptr;     // (its material's skybox cubemap: it is the background, not drawn as a mesh)
			Texture* maps[5] = { nullptr, nullptr, nullptr, nullptr, nullptr }; uint32 mapRevisions[5] = { 0, 0, 0, 0, 0 };     // (colour, normal, metal-rough, emission, height)
			gfx::Pipeline pipeline, pipelineInstanced; gfx::BindGroup group; gfx::Texture baseMap;
			gfx::Pipeline plain, plainInstanced, cut, cutInstanced;     // (alpha-cut: its one-draw pipelines, and its cut's; `pipeline` its light at the cut's depth)
			bool instanced = false;
			Matrix lastModel; uint64 modelFrame = ~0ull;      // (its matrix, and the frame it was drawn with it)
			bool skinnedPipeline = false;
		};
		std::unordered_map<RenderingMesh*, MeshDraw> meshDraws;
		// (a material's surface by the source's hash, known while its surface is unchanged: not hashed a draw)
		struct SurfaceRef { uint32 revision = 0xFFFFFFFFu; uint64 hash = 0; CustomSurface* surface = nullptr; uint32 materialId = 0xFFFFFFFFu; };
		std::unordered_map<CustomShaderMaterial*, SurfaceRef> surfaceRefs;

		Vec3 ambient = Vec3(0.03f, 0.03f, 0.035f), background = Vec3(0.f, 0.f, 0.f);
		f32 exposure = 1.f;
		f64 cpuMs = 0.0, waitMs = 0.0, acquireMs = 0.0, gatherMs = 0.0, submitMs = 0.0, presentMs = 0.0;
	};

}

#endif /* PYROS_RENDERER_SCENERENDERER_H */
