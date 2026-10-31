# The device layer (step 1 of docs/renderer-audit.md)

**Status (2026-10-30):** done, and the only path. The old renderer (IRenderer, Forward/Deferred, the GL, Vulkan and
Metal `IRenderDevice` backends, the post-effects chain) has been removed, and "Device 2" / "renderer 2" lost their
number: the device layer is `include/Pyros3D/Rendering/Device` (namespace `p3d::gfx`), the renderer
`include/Pyros3D/Rendering/Renderer` (`p3d::SceneRenderer`). The log below is the history of how it got here - it
says "the old renderer" and `PYROS_RENDERER=1/2` where those still existed.

Draft for review, as first written: the interface is `include/Pyros3D/Rendering/Device/RenderDevice.h`.

## What changes, in one table

| Today (`IRenderDevice`) | the device layer |
|---|---|
| State setters (`SetBlendFunction`, `SetCullFaceMode`, ...) read at draw time | One `GraphicsPipelineDesc` with all of it, made into a pipeline ahead of use |
| Pipelines and shader variants created on the draw path (hangs, stutters) | `CreatePipeline` returns at once, compiles on the device's workers; `PipelineReady` asked before a draw |
| `BindTextureToTarget` + uniform blocks per draw, descriptor writes per draw | A material's resources in one bind group made at load; a draw's own data is an offset into a per-frame buffer (bindless where the backend has it) |
| Render passes implied by `BindFramebuffer` and draw-buffer calls | `BeginPass(PassDesc)`: targets, load and store said outright (Vulkan dynamic rendering, Metal descriptors) |
| Barriers guessed by the backend | Barriers given (from the frame graph, step 2) |
| One thread records; `ThreadedRenderDevice` drains before 22 kinds of call | Command lists recorded on any thread, submitted in order; no read-back on the hot path |
| SPIR-V compiled at run time from GLSL, per variant | SPIR-V built at build time; the run time only loads it |
| Forward-Z depth | Reversed-Z (far is 0): depth precision for an 8 km map |

## WebGPU is mandatory: its core is the baseline

Every feature is designed on what core WebGPU has, and the native backends add what they have (`Caps`):

| Not in core WebGPU | How it is done everywhere |
|---|---|
| Push constants | Per-draw data in one dynamic uniform/storage buffer, an offset a draw |
| Bindless textures | A bind group a material, made at load (bindless only as a native fast path) |
| Indirect draw count | Culled draws written as zero-instance draws in a fixed-size bucket |
| Mesh shaders, ray queries | Optional: compute culling and screen-space / probe GI are the baseline |
| BC textures on every GPU | BC where `texture-compression-bc`, ASTC/ETC2 elsewhere, chosen when the game is built |
| GLSL/SPIR-V | WGSL generated from the same source at build time (Tint) |

The browser build runs on WebGPU through Emscripten; Dawn gives the same backend natively, so the web path
is tested on the desktop every day, not only in a browser.

## How it gets in without breaking the games

1. WebGPU (Dawn natively, Emscripten in the browser) and Vulkan backends of the device layer together, against the
   same test scene; Metal next.
2. `IRenderDevice` kept, reimplemented as a thin layer over the device layer, so the renderer, the editor and
   the games keep running while the renderer moves pass by pass onto the new API.
3. A test scene for the device alone: thousands of distinct materials and objects, measured CPU per draw
   and GPU frame time, against today's backend on the same machine. It must be faster before anything moves.
4. Then the frame graph (step 2) is written against the device layer only.

## What it does not change

The scene, components, materials as authored, Lua, the editor and the games' code. Materials become
descriptions the renderer turns into pipelines; what a scene file says stays as it is.

## Open questions for Duarte

- Minimum hardware: Vulkan 1.3 and Metal 3 (2019+ GPUs, Apple silicon). Is anything older a target?
- **WebGPU is mandatory** (decided by Duarte): a first backend, its core the baseline.
- OpenGL: proposed to drop (WebGPU covers the web and the older machines); Duarte's to decide.

## Measured (2026-10-27, Apple M3, macOS, `tools/device_test`)

Every number from the frame profile the test writes (`PYROS_PROFILE_LOG`, one line a frame, the engine's
format), GPU times from timestamp queries. Window 320x180 so the frame is not limited by pixels.

**Same scene on today's backend and on the device layer** (10,000 cubes, a draw each, 64 textured materials):

| | today's backend (PyrosPlayer, forward, no auto-instancing) | the device layer, WebGPU (Dawn) |
|---|---|---|
| CPU a frame, whole process, all threads | 9.97 ms | 5.91 ms |
| pipelines compiled inside a frame | can happen (the hangs) | none: `CreatePipeline` returns in 0.1 ms, compiles on workers |

(Today's figure includes the engine's scene update of 10,000 objects: a whole-program comparison.)

**the device layer on Metal** (2026-10-29, `src/Pyros3D/Rendering/Device/MetalDevice.mm`, `PYROS_BACKEND=metal`): the same scene,
the same SPIR-V as Vulkan's turned into the Metal shading language by SPIRV-Cross when a shader is made; pipelines
compiled by Metal's own threads (CreatePipeline 1.6 ms, none waited for); the draw data a shared ring bound at an offset.
10,000 draws, 0 skipped: whole process 5.31 ms of CPU a frame (WebGPU 5.91, today's backend 9.97); record 3.58 ms,
submit 0.06 ms, GPU 0.67 ms. `PYROS_SHOT=<png>` saves the 120th frame from the GPU (checked: the cubes, their 64
materials, depth and light as on the other backends). Record is the slowest of the three - a draw is three Metal
calls (two buffer offsets and the draw); next: the draw's data as setVertexBytes, or one argument buffer a pass.

**the device layer, the frame in parts** (10,000 draws, medians):

| | WebGPU (Dawn) | Vulkan (MoltenVK) |
|---|---|---|
| record | 0.50 ms | 0.61 ms |
| submit (Dawn / MoltenVK encoding Metal) | 3.08 ms | 14.8 ms (includes waiting for the window's image) |
| GPU, the scene pass | 1.11 ms | 2.24 ms |
| acquire the window's image | 12.1 ms (waiting) | 0.01 ms (MoltenVK waits inside submit instead) |

The frame rate on macOS is the window compositor's (about 60 a second even with immediate present):
the renderer's own cost is the record + submit + GPU lines.

Both backends draw the same picture, pixel for pixel (sky 177,203,230; ground 166,169,181 on both),
after two fixes the comparison found: the Vulkan swapchain had taken the Display P3 colour space (the
picture encoded twice), and the winding convention (a turned-over viewport keeps Vulkan's winding as
WebGPU's - nothing to swap).

**Recording on several threads** (20,000 draws):

| | 1 thread | 4 threads, a pass each |
|---|---|---|
| WebGPU record | 0.90 ms | 1.60 ms |
| Vulkan record | 1.20 ms | 2.75 ms |
| GPU (WebGPU / Vulkan) | 2.1 / 4.3 ms | 3.8 / 9.5 ms |

Slower on this machine, for reasons of this machine: Dawn's thread-safe device is one lock round every
call, MoltenVK encodes Metal at submit on one thread, and a tile-based GPU stores and reloads the whole
frame at every pass. So: parallel recording goes **inside one pass** (Vulkan secondary command buffers,
WebGPU render bundles), and it is to be measured on native Vulkan (the RTX 5090) before it is used.
One thread records 20,000 draws in under a millisecond here.

## No shim: the renderer, native on the device layer (decided 2026-10-29)

An IRenderDevice-over-Device-2 shim was designed and dropped the same day - Duarte: "No... we need good practices,
no emulations" and "I don't want any excuses from old code ruining this one". Emulating the old GL-shaped device
(bind-to-unit textures, loose uniforms, UBOs by binding number) on the device layer would have carried the old design into
the new one.

**The renderer** (include/Pyros3D/Rendering/Renderer, src/Pyros3D/Rendering/Renderer) is written on the device layer and
nothing of the old renderer: it reads the scene as data (transforms, the meshes' vertex arrays, material
parameters, lights) and owns its GPU side. Its shaders are new (resources/shaders): set 0 the draw's data, set 1
the frame's data by dynamic offset plus separate textures/samplers; GGX + Lambert in linear HDR; reversed Z with
an infinite far plane; ACES tonemap into an sRGB backbuffer. They are compiled to SPIR-V when the engine is built
and embedded (cmake/PyrosRenderer.cmake) - nothing compiles a shader at run time.

Defined on the way: a light's intensity is what a white diffuse surface it lights head-on gives back (illuminance
= pi x intensity). Fixed in the device layer on the way: the Metal backbuffer is sRGB like the other two backends';
ResizeSurface added; RGB32F vertex format (Unknown no longer stands for three floats); Vulkan bind groups write
storage buffers as storage buffers; dynamic offsets.

Running: `PYROS_RENDERER=2 ./DemoLauncher` on the Metal build - the window is the device layer's, the old device is never
made. Not yet split: the asset classes (GeometryBuffer, Texture, Shader) still make their own GPU objects when a
scene loads; under the renderer they are given the null device. Splitting their CPU data from their GPU objects is
the next piece of step 1, so nothing of the old path runs at all.

First result, demo 0 (Rotating Cube), Metal, M3, 1280x720, 60 Hz, medians:

| | CPU per frame | GPU per frame | passes |
|---|---|---|---|
| old renderer (Forward + exposure meter/adapt + output) | 0.565 ms | 6.565 ms | 4 |
| the renderer (scene HDR + tonemap) | 0.174 ms | 0.244 ms | 2 |

Not the same work: the old frame has auto exposure, the renderer has not yet. The GPU figure at this load moves with
the M3's clock (0.2-3.4 ms frame to frame).

### 2026-10-29, later: no old code in the path, textures, lights

- **The old device is switched off** under the renderer (`SetLegacyDeviceEnabled(false)`): no old device exists, and
  every call that still reaches `GetActiveRenderDevice()` is counted by its caller (`LegacyDeviceReport()`).
  The asset classes keep their data on the CPU and make no GPU objects then: GeometryBuffer (its bytes; Map()
  gives them), Texture (decoded pixels by face and mip - `GetCpuPixels`, `GetCpuRevision`), FrameBuffer (the
  description of its attachments), Shader (no program), GenericShaderMaterial (no PyrosShader program),
  CustomShaderMaterial (no extra-uniform reflection), ParticleSystem (CPU simulation). An old renderer something
  still builds gets the counted inert device, never a GL one.
- **Sweep of all 44 demos** (`PYROS_RENDERER=2`, 9 s each): all run; **43 make zero calls into the old device**,
  at load and per frame. Demo 27 (Neon Pulse) builds and drives an old DeferredRenderer from its own Lua - it
  needs the renderer's Lua API.
- **Material textures**: base colour and emission (sRGB), normal (derivative tangent frame - no tangent
  attribute), metal/rough (glTF channels); mips built on the CPU, sRGB ones averaged in linear light; one bind
  group per distinct texture set.
- **Point and spot lights**: a storage buffer of the frame's lights (dynamic offset), inverse square windowed at
  the radius (the engine's LinearFalloffIntensity model), spot cones smoothed between inner and outer.
- The renderer's CPU per frame across the demos: 0.09-0.40 ms, 1.3-1.4 ms at 800-1000 draws (demos 11, 12, 29).

Not yet: shadows, sky and environment lighting, custom materials (demos 9, 10, 19, 23, 25, 26 draw nothing -
CustomShaderMaterial/terrain: step 4 ports them to the new shader model), auto exposure, transparency sorting,
the frame graph, Vulkan/WebGPU builds of the renderer.

### 2026-10-29, later still: shadows, sky, MSAA

- **Sun shadows**: four cascades in one 4096 depth atlas (a quadrant each, one pass); stable fitting - each
  slice's bounding sphere, its centre snapped to whole texels (no shimmer); practical splits (lambda 0.8); depth
  from the nearest caster to the farthest along the light; reversed Z with a negative slope-scaled bias, a
  1.5-texel normal offset; 16-tap rotated Poisson disc of hardware compares; the last tenth of each cascade blends
  into the next. Casters culled per cascade by their bounding spheres.
- **Physical sky** (Hillaire 2020: Rayleigh, Mie, ozone): transmittance table once, sky-view table each frame,
  the sky drawn behind everything with the sun's disc; the sky projected to 9 spherical harmonics each frame,
  lighting surfaces (irradiance for the diffuse, radiance toward the reflection with Karis's split-sum fit for
  the specular) in place of the flat ambient. The sky and the surfaces share one sun: the light's radiance at
  the camera, divided by the air's transmittance there, is the sun above the air. On for `"sky": true` or the
  Sky effect (the courtyard demos now say so).
- **4x MSAA** on the scene pass, color and depth transient (memoryless on Apple GPUs), resolved into the HDR
  target: the courtyard's GPU time unchanged (1.38-1.41 ms).
- Sweep: all 44 run; 43 make no old-device call.

### 2026-10-30: demo 4's ceiling shadow, ambient occlusion, per-pass GPU times

- **Demo 4's ceiling shadow was wrong** (Duarte): only a triangle of it showed. Cause: the cascades' depth range
  comes from the casters' bounding spheres, and every primitive's geometry reported a sphere of radius 0 -
  `PrimitiveGeometry::CalculateBounding()` was empty (the shapes kept their bounds on the Primitive, not on its
  geometry) - so the ceiling's nearer half fell in front of the shadow map's near plane and cast nothing. Fixed
  twice over: the renderer takes each mesh's sphere from its own vertices at upload, and the engine's primitive
  geometry now computes its bounds. Checked against the geometry: the expected shadow's corners projected through
  the renderer's own view-projection land where the shadow is.
- **GTAO** (Jimenez 2016) at half resolution from a 1x depth prepass, blurred both ways with a depth-aware
  weight; it scales the sky's diffuse light, and Lagarde's specular occlusion its reflection. Found and fixed on
  the way, against a Python run of the same maths on an analytic plane (an open plane must give 1): the horizon
  signs; the slice axis not normalised; samples taken in uv (not isotropic - off the slice's plane) instead of
  pixels; samples in the pixel's own texel counted as a horizon straight up; a half-resolution pixel's centre on
  a texel boundary, where floor() went either way column by column.
- Debug views: `PYROS_RENDERER_DEBUG=cascades` (tinted by cascade), `ao` (the AO alone - tonemapped, so 1.0 shows
  as 0.80), `ao-off`.
- **Per-pass GPU times** in Metal device: counter samples at stage boundaries, each pass's time from the later
  of its own start and the previous pass's end (a vertex stage starts early and waits on the pass before).
  Courtyard: SkyView 0.31, SkySH 0.03 (was 0.7-1.6: nine threads looping 512 samples - now 9x16 partial sums and
  a 9x1 sum), SunShadow 0.45 (16-bit atlas, was ~0.75+), DepthPrepass 0.02, GTAO 0.43, blurs 0.05 each, Scene
  0.57, Tonemap 0.12 ms - about 2.3 ms. D16 added to the device layer (all three backends).

### 2026-10-30, later: bloom and auto exposure

- **Bloom** (Jimenez 2014): six halving levels in R11G11B10F, 13-tap downsamples (the first with Karis's
  average, and its light capped at 32 so the sun's disc does not spread over the frame), tent upsamples added on
  the way back; mixed into the scene at 3% (light spread, not added). ~0.25 ms.
- **Auto exposure**: log2 luminance at 64x64 (centre-weighted; empty pixels - a cleared background - left out),
  summed down to 1x1, adapted from the last frame's (a 1x1 pair) toward middle grey, faster to light than to dark,
  within -5..+2 stops of 1. ~0.04 ms. A demo whose config says `Output` with `uAuto: 0` keeps a fixed exposure
  of 2^uExposure; otherwise uExposure is the compensation.
- Sweep: all 44 run; 43 make no old-device call.

### 2026-10-30, later: LOD, culling, instancing, alpha-cut shadows, the scripts' renderer

- **A visibility step each frame** (Gather): every mesh at the level of detail its distance picks (the scene
  lists a component's first level; another level is drawn from the component's own list - the LOD demo drew
  nothing past level 0 before), its world sphere (from its own vertices), frustum-culled against the camera's
  planes (no far plane - reversed infinite Z); opaque sorted by pipeline, material, mesh; transparent back to front.
  Every pass draws from it. LOD and culling can be turned off (`renderer:disableLOD()`, `deactivateCulling()`).
- **Instancing**: a per-instance vertex stream (matrix + tint, 80 bytes), uploaded when the component's new
  `instanceRevision` changes; drawn `InstancesToDraw()` instances (the density LOD); a sphere round all its
  instances for culling. Shader permutations are now data of the build: `file@A,B` compiles `file` with -DA -DB.
- **Alpha-cut shadows**: casters with an alpha-tested base colour map cut by its alpha in the shadow pass
  (blades of grass, not their cards). Casters are drawn only into the cascades their shadow can reach (its size
  times the sun's slant).
- **Material colour**: applied only with ShaderUsage::Color (a textured material without it is its map alone -
  the grass was discarded whole by its alpha test).
- **The scripts' `renderer`** is the renderer (a Lua usertype with what it does: background, ambient, LOD, culling,
  sky, exposure). Scripts that build scenes behind `if not renderer` now run: LOD (10,000 teapots, 100 lights),
  the grass field (169,400 blades), both islands - and Neon Pulse no longer builds an old renderer.
- **Sweep: all 44 run; all 44 make no call into the old device.** Still black: particles (9, 10) and the custom
  material (25, 26).
- Per-pass GPU times: the first pass is timed from the frame's command buffer start (it took the wait at the
  frame's start before). Passes that run side by side (the shadow pass beside the sky's) overlap, and one of them
  then shows ~0 - the numbers are approximate there.
- Heaviest pass now: the grass's shadows (~6 ms, alpha-tested blades) - for the GPU-driven step.

### 2026-10-30, later: particles, custom materials (step 4 begins) - every demo draws

- **Particles**: camera-facing quads from the vertex index, the CPU simulation's live particles streamed each frame
  through a ring that grows to fit (demo 10: two systems of 505,004); the size/colour ramps and ease modes, fades
  and softness (against the prepass depth) as the scene data says; lit ones take the sky's and the sun's light
  (Henyey-Greenstein), additive ones glow. Drawn after the sky - and transparent surfaces now are too (the sky,
  drawn where the depth is still far, painted over them before). A million particles costs ~38 ms of GPU (overdraw
  in the MSAA target): a half-resolution particle target is the next step for that.
- **Custom materials - shaders as data**: a CustomShaderMaterial is drawn by its *surface*
  (`SetSurfaceSource`): `void Surface(inout SurfaceData s)` filling colour, normal, roughness, metallic, emission,
  occlusion, alpha, with `//@param` / `//@texture` lines; the renderer compiles it once (by its hash, shaderc + the
  SPIR-V caches) into surface.frag against its own sources embedded at build time
  (cmake/EmbedShaderSources.cmake), and lights, shadows, AO-occludes it like every surface. Parameters are written
  each frame into a ring (a fourth dynamic offset); textures bind by the material's AddSampler() names.
  - The Material Editor's codegen emits it (MaterialCodegen BuildSurface, from the same TemplateInputs as the old
    template; the node helpers now shared, not copied), written beside each generated shader as `<file>.surface`
    (the Material Editor, terrain splat and cave shaders) and loaded by the file constructor; saved in scenes as
    "surface".
  - `tools/migrate_material_surfaces.py` gives materials generated before it their surface, once, from what the
    codegen wrote (scenes' embedded shaders, .glsl files on disk). Run on the demos: AmbientBugRepro.
  - Hand-written custom shaders (NatureLab's Ocean, Erangel's Lake/terrain...) are ported to surfaces one by one.
- **Sweep: all 44 demos draw, all 44 make no call into the old device.**

### 2026-10-30, later: the games on the renderer - Erangel runs

- **The player** (PyrosPlayer, Metal build) draws with the renderer when the window is the device layer's (`PYROS_RENDERER=2`):
  no old renderer, post chain or UI renderer is made; the scene's ambient, background, sky (an enabled Sky effect)
  and exposure (its Output effect) configure the renderer; scripts get it as `renderer` (RendererLua.h, shared
  with the DemoLauncher). Not yet: the HUD (UIRenderer's 2D) - the renderer has no 2D pass yet.
- **Shader globals** moved to their own store (Rendering/ShaderGlobals.h) - both renderers read it, the renderer in
  its frame data (`uGlobals` in a surface).
- **Geometry**: a mesh packed for the GPU (terrain tiles: position, normal, uv, tangent, bitangent interleaved) is
  read from its buffer's CPU bytes by each attribute's offset and the stride; a primitive's own arrays (a terrain's
  distant pieces) too.
- **Custom surfaces** grew: a `void Vertex(inout VertexData v)` stage of their own (P3D_STAGE_VERTEX/FRAGMENT
  guard one source), the sky-view table (now absolute radiance) for sharp sky reflections on every smooth surface,
  the opaque scene's depth; their textures bound as stored (a surface decodes its colours itself). Materials made
  from a shared shader + SetShaderFile load their surface too. `tools/check_material_surfaces.py` compiles every
  surface as the renderer does: 41 of 41.
- **Erangel's sea, ported** (assets/materials/Water.glsl.surface): its waves, swell, wash, ripples, river, foam,
  depth colour, ice kept; its painted sky and glints dropped - the renderer reflects the real sky and the sun off it
  (a dielectric, F0 0.02). The sheet reaches 40 km (the horizon from flying height).
- Erangel's Island on the renderer: terrain (splat surface), rocks, buildings, trees, the helicopter, the sea, the
  physical sky. Still to port: Lake.glsl, Glass.glsl (868 window meshes), the HUD.

## The renderer: the UI, text in the world, the glass and the far sea (2026-10-30)

- Only `RenderLayer::World` meshes enter the 3D gather; the UI's are drawn by `SceneRenderer::RecordUI` after the
  tonemap, into the sRGB backbuffer: every `UICanvas` on the scene, solved at the window's size, its batched draw
  list in order, its clip rectangles as scissors, flat (`ui.vert/.frag`: unlit, alpha-blended). Text takes its
  coverage from the font atlas (a smoothstep on the distance for SDF fonts) and each character's colour from the
  normals Text.cpp packs it into; batched quads take their `aColor` (a per-mesh colour stream, white where a mesh
  has none). One- to three-channel CPU pictures (a font's atlas) are widened to RGBA8 on upload.
- Text in the world (a TextRendering material on a World mesh - Neon Pulse's titles) is drawn with the same
  shaders into the scene's HDR target, depth-tested, after the transparent surfaces; it casts no shadow.
- Erangel: `Glass.glsl.surface` (a smooth dielectric with no diffuse: the reflection of the real sky and sun, alpha
  the share the pane keeps back) and `Lake.glsl.surface` (the sea's surface with `WATER_BOUNDED`).
- The far sea's arcs round the eye were moire: at least 6 wave trains were kept where one pixel holds hundreds of
  metres, and the footprint was the derivatives' geometric mean (far too small at a glancing look). Now the long
  side of the footprint sets the count, which falls to none, the last train fading in by its fraction, and the
  slope left out becomes roughness. The shore foam's cells and web run only within 21 m of the sand. Sea cost on
  Island: Scene pass ~10 ms -> ~8-10 ms with the extra roughness (M3), from 15+ before the foam gate.
- `PYROS_RENDERER_STATS=1` (player): CPU/GPU/per-pass every 120 frames. `PYROS_RENDERER_SKIP=<text>`: custom materials whose
  shader file contains it are not drawn (to measure one).
- `tools/check_material_surfaces.py` also compiles each surface's `Vertex()` stage.

## The renderer on WebGPU (2026-10-30)

- The renderer builds with Metal, WebGPU, or both (`cmake/PyrosRenderer.cmake`, `cmake/PyrosWebGPU.cmake`); with both,
  `PYROS_GFX_BACKEND=webgpu` picks WebGPU (Dawn, on Metal underneath here). All 44 demos and Erangel render on it with
  Dawn's validation on (`PYROS_WEBGPU_VALIDATION=1`) and no error; the demos' frames match Metal's to within 1/255 on
  average (2.6/255 where water or grass animate).
- Shaders stay SPIR-V everywhere. The WebGPU device turns SPIR-V into WGSL itself (`SpirvToWgsl.cpp`, Tint's SPIR-V
  reader and WGSL writer, with WGSL's derivative-uniformity rule turned off by its own directive for the materials'
  surfaces); the renderer and the surfaces compiled at run time do not know which device they are on.
- Dawn is built from source (`.fetchcontent-cache/dawn-src`, `dawn-build`) with Tint's SPIR-V reader, so one Tint
  serves Dawn and the converter: a Dawn release bundles a Tint without the reader.
- What WebGPU asked of the renderer, done for every backend: lookup tables, AO and sky reads at an explicit level
  (`textureLod`, as WGSL wants off uniform control flow); depth read texel-exact (`texelFetch`); `ShadowTexture` (a
  compared depth texture) apart from `DepthTexture` (a depth texture read as values); lights and particles uploaded
  (`Upload`) instead of written through `Map` (WebGPU has no persistently mapped memory); group 0 bound for
  pipelines without per-draw data; bloom R11G11B10F where `Caps().rg11b10Target`, else RGBA16F. The device asks for
  the adapter's limits (a terrain surface samples 17 textures), float32-filterable and RG11B10 rendering.
- Pass times: Dawn's timestamps on Metal include the wait on the previous pass, as Metal's counters do - each pass is
  counted from the later of its start and the previous pass's end. Demo 4: WebGPU 5.3 ms, Metal 4.3 ms.
- `PYROS_SHOT=<file.png>`: the 120th frame read back, on WebGPU as on Metal.
- `PYROS_LUA_SEED=<n>`: scripts' `math.random` from a fixed start, so a game's captures can be compared.
- Next: the browser (Emscripten, emdawnwebgpu, Tint and glslang in WebAssembly for the surfaces compiled at run time).

## The renderer in the browser (2026-10-30)

- `emcmake cmake -S . -B build_web_r2 -DPYROS_WEB_WEBGPU=ON` builds the web player on WebGPU: the renderer on the
  WebGPU device through emdawnwebgpu (built locally from the Dawn source: `.fetchcontent-cache/tint-wasm`,
  target `emdawnwebgpu_pkg`), Asyncify for the device's one-time adapter/device wait, no WebGL context on the canvas
  (`SDL2Context`, `SDL2CONTEXT_WEBGPU`), no present (the browser shows the canvas when the frame returns).
- The page turns no SPIR-V into WGSL and compiles no GLSL. The renderer's shaders are WGSL made at build time by the
  native `pyros_spirv2wgsl` (`PYROS_SPIRV2WGSL`, default `build_ed_mtl/pyros_spirv2wgsl`) and embedded beside their
  SPIR-V (`R2_WGSL`); the materials' surfaces come from the game's `surfaces/` folder, which a native run of the
  player fills (`SceneRenderer::SetSurfaceCache` - SPIR-V and WGSL by the hash of the whole text compiled).
- The renderer has its own GLSL compiler for the surfaces (`GlslToSpirv.cpp`, glslang's C interface), not the old
  renderer's shaderc-based one.
- `tools/pack_web_game.py <game> <site> build_web_r2/player` packs a game as Build Game does (now with `surfaces/`).
  NeonPulse and Ember run in Chrome on it. The fetched Lua of the web and iOS builds is compiled with
  `LUA_COMPAT_5_3`, as the desktop's is (Ember's `math.pow`).
- Old shader files are not read with the old device off (`CustomShaderMaterial`), so a shipped the renderer game needs
  no engine `shaders/` folder.

## The renderer: point and spot light shadows (2026-10-30)

- One shadow atlas, 8192 x 4096 D16: the sun's four cascades in its left half (as before, a quadrant of it each), the
  point and spot lights' 512 tiles in its right half (64 of them). A spot light takes a tile, a point light six - a
  face of its cube each, chosen in the shader by the direction's major axis - so every backend samples one 2D depth
  texture, and no binding was added (the materials' surfaces are as they were).
- The lights that cast shadows (`castingShadows` in a scene, `enableShadows` in Lua) get tiles nearest the camera
  first, while tiles last (`WriteLights`). Each tile: a reversed-Z perspective from the light's position to its radius
  (90 degrees and a few texels for a cube face, the outer cone and a little for a spot); its casters culled against
  its frustum; drawn in the same pass as the cascades, a viewport each, with the cascades' caster pipelines.
- The lookup (`LocalShadow`, shadows.glsl): the receiver pushed along its normal by a texel at its distance, a
  rotated 16-tap Poisson disc of hardware compares kept inside the tile. A light's first tile rides in its
  `cosInner.y` (< 0: none).
- Demos with them: Rotating Cube (Lighting + Shadows), Volumetric Lights, Shadow Matrix, Two Point Shadows, Cornell
  Box; the sun's demos are unchanged; WebGPU validation-clean.

## The renderer: draws batched, the scene walk cached (2026-10-30)

- Automatic instancing in every pass: draws of the same mesh with the same pipeline and material one after another
  (the opaque list is sorted that way; the shadow casters get their own order, by kind, alpha map and mesh) are one
  instanced draw through the instanced pipeline variant, their matrices in a per-frame ring (`BatchWrite`, sized at
  each frame's start from what the last one wanted; a batch without room is drawn one by one). Transparent surfaces
  keep their back-to-front order and are not batched. Demos 11 and 12: 3024 and 2456 draws -> 27 and 34; Erangel's
  frame: ~3600 -> ~1100 draws.
- `Gather` keeps what each mesh is drawn with from frame to frame (`MeshDraw`: its casts, GPU mesh, surface,
  pipelines, bind group), redone only when its material, geometry, textures, transparency or cull change; a custom
  material's surface hash is kept by its surface revision (it hashed the whole source per draw per frame); a geometry
  rebuilt each frame (a HUD text) is written into its buffers, not made anew (costly on WebGPU).
- The depth prepass skips surfaces whose vertex stage moves them (the sea's sheet: its prepass depth was wrong).
- Erangel, renderer CPU per frame: Metal ~6 ms (was 8-14), WebGPU 5.5-7.5 ms (was 13-22; Dawn's submit 0.3-1.2 ms
  from 2-7). The player prints gather / submit / present with `PYROS_RENDERER_STATS=1`.
- Erangel's water: its trains' directions from a table (48 trig calls a pixel fewer), the wind's ripple from the
  material's wave normal map (two drifting mip-filtered samples, in place of 9 more trains), caustics only where the
  bed shows, the river's light only in rivers.

## The renderer: clouds (2026-10-30)

- A slab of clouds 1.5-3.5 km up (`clouds.glsl`, `clouds.frag`), marched at half resolution through a tiling 64^3
  noise made at start-up (r: Perlin fbm lifted by Worley, rank-equalised so a cover of c is about c of a level of the
  slab; gba: Worley at 4, 8, 16 cells to wear it away): the base shape cut by the cover, eroded by the detail (wisps
  below, billows above), each step lit by the sun through six growing steps toward it (single and multiple
  scattering terms, a two-lobe phase, a powder term) and by the sky itself (the sky-view table, above and at the
  horizon); faded into the sky's colour with distance. Each frame a new jitter (golden-ratio steps), blended into the
  last frame's clouds reprojected at their own distance (temporal accumulation: no march noise). ~0.5 ms on an M3.
- The sky pass composites them (sky x transmittance + clouds; the sun's disc behind them too).
- `clouds.frag@DOME`: the sky-view table's directions with the clouds in them (192 x 108, 16 steps, ~0.1 ms) - what
  the surfaces mirror and what the sky's light (its SH) is summed from: reflections show the clouds, overcast dims
  and whitens the ambient.
- Their shadow (`cloudshadow.frag`): a 256^2 map over 16 km of ground round the camera (snapped to its texels), what
  of the sun gets through the slab; the surfaces' sun light dimmed by it (binding 15 - the materials' own parameters
  and textures moved to 16 and 17+). `PYROS_RENDERER_DEBUG=cloudshadow` shows it.
- Settings from the scene's Sky effect, as the old one had them: `uCloudCover` (0.52), `uCloudSteps` (40),
  `uCloudShadow` (0.45); `renderer:setClouds(cover, steps, shadow)` in Lua. A scene's cover is of one level of the
  slab raised to 1.6 (0.52: scattered clouds with sky between).
- The Metal device uploads 3D textures by depth slice.

## The renderer on Vulkan (2026-10-30)

- A Vulkan build of the engine (`PYROS_GRAPHICS=Vulkan`, `build_ed_vk`) has the renderer on Vulkan the device layer
  (`VulkanDevice.cpp`; MoltenVK here, the drivers' own on Windows and Linux): `PYROS_RENDERER=2` makes the SDL Vulkan
  window's the device layer (a Metal layer on macOS, the HWND on Windows, the X11 window on Linux) and switches the old device
  off; DemoLauncher and the player take it as they take Metal's. Its shaders are the SPIR-V as built.
- What it lacked, done: multisampled targets resolved at a pass's end (dynamic rendering's resolve: the scene was
  black); 3D textures uploaded by depth slice (the clouds' noise); a texture sampled before anything is drawn into it
  put in the layout a read wants; the platform's surface function through the instance (not whatever the engine's
  volk was built with); the validation layer asked for only where the loader has it (`PYROS_VK_VALIDATION=1`,
  `VK_LAYER_PATH`); `PYROS_SHOT` captures; VMA's implementation left to the old device in the same library.
- Destroyed buffers and textures are freed once the frame that last used them is done (its fence), as Metal and
  WebGPU keep them alive by themselves: a ring grown mid-run was freed under the GPU. All 44 demos and Erangel on
  Vulkan the device layer with the validation layer: no message; frames within 1.9/255 of Metal's.

## The renderer: screen-space reflections (2026-10-30)

- Glossy surfaces (roughness under 0.4: the sea, polished floors, wet stone) march their reflected ray across the
  screen in the surface shader (`ScreenReflection`, surface.frag): from a little off the surface (it is in the depth
  it is marched against), 48 steps along the projected line with perspective-correct depth, against the depth
  prepass; a hit where the ray goes behind the picture between one step and the next (and not far behind a thing),
  refined by halving; its colour from last frame's picture (`hdrPrevious`, a copy of the resolved HDR each frame).
  How sure: faded at the screen's edges, for rays toward the eye, with roughness and toward the ray's end; where
  unsure, the sky's reflection (with its clouds).
- Settings: the scene's SSR effect (`uMaxDistance` its reach); `renderer:setReflections(on, reach)` in Lua; on by
  default in DemoLauncher. ~1.3 ms on a screen of polished floor (M3).
- Bindings: 16 last frame's picture; 17-19 kept for the renderer's own; a material surface's parameters at 20 and its
  textures from 21.

## The renderer: temporal anti-aliasing (2026-10-30)

- Each frame drawn a sub-pixel off (the projection jittered, Halton 2, 3 over 8 frames; culling, the sky and the
  reprojection keep the matrix without it) and resolved after the scene pass (`taa.frag`, before bloom): last frame's
  result sampled sharp (Catmull-Rom, five bilinear taps) where this pixel's surface was then, clamped to this frame's
  3x3 neighbourhood by its spread in YCoCg, blended ~1/10 in (more where things move fast), weighted by brightness,
  then lightly sharpened by the neighbourhood's contrast; copied back into the HDR target, so bloom, exposure, the
  tonemap and the reflections' copy read it as before. MSAA stays.
- Where a surface was: the depth prepass writes each surface's move on screen (`velocity.frag`, RG16F: its object's
  matrix this frame and last - kept per mesh in `MeshDraw` - through the views without their jitter), the nearest in
  each 3x3 followed; what the prepass does not draw (the sky, the sea's moving sheet, see-through things) goes by the
  depth. A thing that moves with the camera (a helicopter, a weapon) does not smear. A batch carries no last-frame
  matrices: what moved is drawn alone.
- On unless turned off: `SetTemporalAA`, `PYROS_RENDERER_TAA=0`. ~0.3 ms (courtyard) - 1.2 ms (Erangel) on an M3.
- Surfaces that move their own vertices (the sea's sheet following the eye) are in the depth prepass now, through their
  own vertex stage (`CustomKey::blend == kPrepassBlend`: their pipeline with `novelocity.frag`, RG16F, single-sampled;
  their group with a stand-in depth texture where the prepass's own would be): the temporal resolve reprojects the sea
  by its own depth, and it has ambient occlusion and a depth the reflections march against. WebGPU and Vulkan
  validation-clean.

## The renderer: skinning, overlay UI (2026-10-30)

- Skinned meshes are posed: each mesh's bones (`RenderingMesh::BonesToDraw`, as the animation system leaves them) go
  into this frame's slot of one bone palette (a storage buffer, 8192 bones a frame; standard binding 17 for surfaces,
  2 for the depth prepass, 3 for the shadow casters - in every group of those passes, so groups stay shared); a
  skinned mesh's vertices carry their four bones and weights (`aBonesID`, `aBonesWeight`) as a stream of their own
  (vertex binding 2); `skinning.glsl` poses them in `surface.vert@SKINNED`, `depth.vert@SKINNED` (its move on screen
  posed with this frame's bones at both ends) and `shadow.vert@SKINNED` (and `,ALPHA`). Skinned draws are not
  batched. Custom surfaces too: every custom pipeline's layout and group carry the palette (17); on a skinned mesh
  its pipeline key is skinned - its own vertex stage compiled a third time with SKINNED (posed, then its `Vertex()`
  moves the posed world position), or `surface.vert@SKINNED` where it has none.
- WebGPU's vertex buffer layouts are laid out by binding number (a slot between, as the skinned pipelines' 1 between 0
  and the bones' 2, unused: step mode undefined, no attributes) - by their place in the list, a skinned pipeline's
  bones sat in slot 1 and no skinned draw was valid on WebGPU.
- Erangel's title character stands in its idle pose (was in its bind pose, arms out); the IK demo's figure is posed.
- The player draws its overlay scene's UI over the renderer's frame, and a scene without a camera still draws (its UI).
- `PYROS_SHOT_FRAME=<n>`: the capture's frame (120 unless set), on every device.
- Scene changes under the renderer: the player applies a script's `loadScene` / overlay changes after a renderer frame
  too (its frame returned before them: a game never left its first scene), and `SceneRenderer::ForgetScene()` lets go
  of what is kept by the old scene's objects' addresses before the new one loads (DemoLauncher on a demo switch too) -
  a new object at a freed address took the old one's GPU mesh, texture or surface (Erangel's island after its title:
  a brown plane and garbled HUD text).
- Half-resolution particles (on by default; `SetHalfResolutionParticles`, `PYROS_RENDERER_HALF_PARTICLES=0`): particles are
  drawn after the scene pass into a half-size RGBA16F target (no depth target, no MSAA), `particle.frag@HALF` hiding
  them behind the full-size prepass depth and fading them softly against it; alpha ones blend "over", additive ones add
  light and leave the cover; `particlecomposite.frag` lays the target over the HDR picture (premultiplied: one, one
  minus alpha) before TAA. The million-particle demo: GPU 42.9 -> 27.3 ms on an M3; light ones unchanged within noise.

## The renderer: the frame graph (2026-10-30)

`Renderer/FrameGraph.h`: Render() adds the frame's passes - SkyTables, Clouds, CloudShadow, Shadows, DepthAndAO,
Scene, HalfParticles, TemporalAA, BloomAndExposure, Tonemap (with the UI), KeepPicture - each with the textures it
writes (and how: colour, depth, copy; `discard` where what was there is not wanted) and the ones it reads, and a
function that records it; `Execute()` records them in order. No pass says what state another left a texture in: the
graph keeps every texture's state (pass to pass, frame to frame) and makes each barrier from it. Between passes a
texture rests sampleable (ShaderRead) - its readers bind it by group; within a pass of several steps (the AO blur's two
ways, the bloom chain, the TAA's copy back) `Use()` moves it from where it is. A pass whose writes are all transient
(`Transient()`: the velocity, the AO's middle target, the half-size particles) and that no later recorded pass reads is
not recorded - none today, as Render() leaves out what it does not need; the graph says what it culled
(DemoLauncher's stats line: passes, recorded, barriers). The 29 barriers written by hand, each with a `from` the
writer assumed, are gone. Not yet: transient textures sharing memory (the device has no aliasing), passes reordered
or on other queues.

## The renderer: parallax, the tangent frame's sign (2026-10-30)

- Parallax occlusion mapping: a generic material's displacement map (`SetDisplacementMap`, r: how deep - white the
  deepest, as the old renderer read it) is a fifth map of the surface group (standard binding 18); `skin.y` of the
  draw data its depth (`displacementHeight`, texcoord units). `surface.frag`'s `ParallaxUV` marches the view ray into
  it in tangent space - 8 to 32 layers, more the more grazing - and takes the crossing between the last two; every map
  is then read there with the surface's own gradients (`textureGrad`: the loop's are not continuous).
- The derivative tangent frame (`TangentFrame`, Schüler) takes the determinant's sign: his formula drops it, right
  where the screen's y goes up (OpenGL); on Metal, Vulkan and WebGPU it goes down and the frame was turned over -
  every normal map lit inside out, the parallax demo's bricks sunk in instead of standing out.

## The renderer: skyboxes (2026-10-30)

A generic material with a six-face cubemap (`SetSkyboxMap`) makes its mesh the scene's background: Gather keeps the
cubemap (`MeshDraw::skybox`, found once, not each frame) and does not draw the mesh - no cube to follow the camera.
`SkyboxFor` uploads the faces as a cube texture (+x -x +y -y +z -z, sRGB); `skybox.frag` draws it behind everything
(the far plane, where the depth is still the clear's) by the eye's direction from the view and projection, in place
of the physical sky. And it lights the scene as the physical sky would: `skyboxtable.frag` fills the sky-view table
from it - all round (`FrameData.ambient.w`: `SkyViewUV` by the absolute azimuth; the physical sky's table is a half,
mirrored about the sun, which a skybox is not) - and the sky's harmonics are summed from that: the surfaces mirror it
and are lit by it from all round, the particles too. The volumetric clouds and their shadow are off under a skybox.
The Skybox demo was flat grey (the renderer drew nothing of it); Island (SSR Water) drew its skybox cube as a white
slab, and now its sea mirrors the sunset.
- SSR at grazing: `ScreenReflection` takes the view's angle to the surface and gives way to the sky's reflection over
  its last few degrees (NoV 0.06 -> 0.01). There the ray skims the surface and finds the surface itself - the next
  wave - so the far sea mirrored its own last picture at a Fresnel near 1, plus the sun's highlight, each frame: a
  feedback that grew to a white line along the horizon (Island (SSR Water), once its skybox lit it). The title's
  lagoon keeps its reflections.

## The renderer is the default (2026-10-30)

The player and DemoLauncher are drawn by the renderer wherever the build has it (Metal, Vulkan, WebGPU);
`PYROS_RENDERER=1` brings the old renderer back, `=2` forces the renderer. `examples/WindowManagers/RendererChoice.h`
decides for the window contexts, which the editor shares: each application says its default before its window
(`SetRenderer2ByDefault` - player/src/main.cpp, examples/MainProgram.cpp for DemoLauncher). The editor stays on the
old renderer until its viewport, previews, thumbnails and gizmos are drawn by the renderer; then the old renderers and
devices can go. A game built with build_game runs the player: the renderer.

## Toward the editor on the renderer (2026-10-30)

What the editor needs of the renderer and the device layer before its views can move (the map of its rendering: the main Scene
View, the camera preview, material / animation / 2D-character previews, thumbnails, impostor bakes, picking, gizmos and
debug lines, the axis helper, the Render Targets tab - each an old renderer today):

- `View::target` / `View::hostFrame`: the renderer draws a view into a texture of the device's `BackbufferFormat()`
  (left sampleable), and leaves the device's frame to the host - several views a frame, then the host's UI, then one
  present. One SceneRenderer a view (its targets, histories and frame slots are its own).
- `ImGuiRenderer`: Dear ImGui drawn on the device layer - its 1.92 texture protocol (the font atlas made and grown as it asks),
  an ImTextureID is a device layer texture id (`ImGuiRenderer::Id(view.target)` shows a view). DemoLauncher's panels are
  drawn by it under the renderer (they were not drawn at all).
- One backbuffer a frame: `AcquireBackbuffer()` gives the frame's again (Metal asked for a second drawable, Vulkan
  acquired a second image and threw its contents away, WebGPU made a new view).
- Vulkan: the frame's lists go to the queue at once at its present or end (`Flush`) - one wait on the image, one
  signal of its end, one fence; several submits a frame broke all three. A mid-frame readback sends them first
  (`SubmitPending`, waiting on the image there, once).
- `Device::ReadTexture`: a texture's pixels back (made with TexCopySrc), waiting for the GPU - picking, thumbnails,
  bakes, screenshots. `PYROS_DEMO_READBACK=<file.png>`: DemoLauncher draws its 120th frame into a texture and reads it
  back, a test of both.
- Orthographic views (`View::orthographic`, `orthoHeight`, `farZ`): reversed-Z orthographic; `FrameData.projection.w`
  says which, and the shaders' depth to distance, view point and projection go through `common.glsl`'s
  `DepthDistance`, `ViewPointAt`, `ProjectView`, `ToEye` (GTAO, the AO blur, soft particles, SSR - whose ray depth is
  linear there, not 1/w - the sun's cascades). `PYROS_DEMO_ORTHO=<height>` views a demo so.

Still to do for the editor: its frame loop on these (BeginFrame, its views, ImGui, Present), a debug-line pass (the
gizmo, light and camera gizmos, physics and terrain overlays), the grid and icon billboards against the scene's depth,
picking (ID pass + ReadTexture, or on the CPU), the previews and bakes, a public texture bridge for its icons and
thumbnails; then the old renderers, devices and ImGui backends can go.
- The editor on the renderer (PYROS_RENDERER=2 while the port is under way; the editor's default stays the old one):
  its ImGui by ImGuiRenderer (Editor::Init, DrawUI, Draw - BeginFrame, the panels, one ImGui pass over the backbuffer,
  Present, EndFrame); each document's Scene View drawn by a SceneRenderer of its own into a texture of the viewport's
  size (`SceneEditor::RenderViewport`: the editor camera's perspective, or its orthographic / 2D view), shown as the
  viewport image; the scene's settings as they are edited (`p3d::ApplySceneSettings` - SceneSettings.h, one function
  the player's games use too: ambient, background, Sky and its clouds, SSR, Output's exposure). Metal and Vulkan.
  Not yet: the grid, gizmos, debug lines and billboards over it, picking, the camera / material / animation / 2D
  previews, thumbnails, bakes - the old renderer's code still runs under it, its calls counted and dropped.
- Overlays (`SceneRenderer::AddOverlay`): lines or triangles in world space, colours as written, drawn over the next
  frame's tonemapped picture by the view without its jitter (`overlay.vert/frag`), hidden behind the scene where asked
  (against the prepass depth, a give of 0.3% of the distance). The editor's grid (depth-tested, the old one's look) and
  everything its DebugRenderer batches - the transform gizmo, light and camera gizmos, physics, terrain, navmesh and
  cell overlays - come through it (`DebugRenderer::TakeBatches`; a point as a small cross).
- Picking (`SceneRenderer::Pick`): the pixel's ray against the meshes the last frame drew - bounding spheres, then
  their triangles (Moller-Trumbore) read from the geometry as MeshFor reads it (`SourceOf`, `PrimitiveArrays`,
  `IndicesOf`) - on the CPU; the editor's click on the renderer goes through it.
- Caches keyed by an object's address check its identity too: `IGeometry::GetInstanceID()`, `Texture::GetInstanceID()`
  (new: an atomic counter), `IMaterial::GetInternalID()` - in the GPU meshes, textures, mesh-draw records, custom
  surfaces and their groups. Selecting in the editor made and freed objects, and a new one at a freed one's address
  was drawn with the old one's mesh (the ground a strip, the ramps other shapes); a game making and freeing objects
  at run time could have done the same.
- The editor's camera preview (a scene camera selected: Properties) on the renderer: a SceneRenderer and texture of its
  own, the camera's perspective or orthographic projection, the editor's helpers hidden while it draws. The agent's
  screenshots read the renderer's textures back (`SceneEditor::ScreenshotOf`: the Scene View live; offscreen, the camera
  preview where there is one) - `driver.py smoke` passes with PYROS_RENDERER=2.
- The Material Editor on the renderer: its preview (`MaterialPreview`) a renderer of its own drawing its sphere and
  light into its texture every frame; a custom material's surface from the same codegen as the asset's `.surface`
  (`BuildSurface`, the graph's or the text's), set on the preview and - applying - on the live material, after
  `SceneRenderer::CheckSurface` compiled it as the renderer will (its glslang log the editor's error; `SurfaceTexts`
  builds the texts for both, so they cannot drift); `RecompileFromDisk` reads the `.surface` again. No old program is
  compiled ("Shader compile/link failed" was the old device's, switched off).
- The editor's other views on the renderer, through one helper (`editor/src/editor/RendererView.h`: a SceneRenderer and
  texture of its own, a DebugRenderer's batch over it): the Animation Editor's preview (its skeleton, gizmos and floor
  grid as the overlay), the 2D character preview (orthographic, full ambient, exposure 1), the asset browser's model
  thumbnails (drawn, read back with ReadTexture, written as before - the renderer's caches let go after each). Images
  the editor shows of texture assets (thumbnails, icons, sprite sheets, material slots) go through
  `EditorImGuiTexture`: on the renderer the device layer's copy of the texture (`SceneRenderer::GpuTexture`, sRGB).
- Three the device layer / the renderer bugs those found:
  - Vulkan's descriptor pool was made with `poolSizeCount = 5` of six sizes: no plain storage buffers (the bone palette)
    - MoltenVK gave null sets for them under load, and a frame bound one (a crash at submit).
  - Vulkan's handle pool had no notion of a dead entry: `Get` of a removed id gave an empty record (a null set bound),
    and a second `Remove` put the id on the free list twice - two resources then shared it. It keeps which are alive.
    Bind groups are freed with the frame's other resources once its GPU work is done (they were freed at once).
  - `SceneRenderer::EnsureTargets` (a new size: an editor panel resized, a window) remade the surface and custom groups
    but left each mesh's copy of its old group - the next frame drew with groups let go. The copies are cleared.
- Bakes on the renderer: `SetToneCurve(false)` (the picture as lit, exposed and clamped: no filmic curve, no bloom -
  tonemap.frag reads a negative exposure so) for pictures read back as colour maps; `SetDebugView(6|7|8)` (or
  PYROS_RENDERER_DEBUG=albedo|normal|depth) for a surface's own outputs - its unlit albedo, its view-space normal, its depth
  across the view - so that, the curve off, each byte read back is that value. The impostor bake (two passes, on black
  and on white, for the alpha) and the octahedral impostor bake (albedo, normal and depth per view, the crown's
  occlusion from the depth as before) use them; the octahedral one now has all its views (the old renderer's left the
  top rows empty).

## The renderer draws 2D (2026-10-30)

A 2D scene (`twoD`: Sprite2D, Layer2D parallax, Character2D, 2D lights) on the renderer, as the old renderer drew it:
- `Gather` flushes the sprites and 2D lines first (`Sprite2D::FlushScene`, `Line2D::FlushScene`, as IRenderer did - the
  sprites' quads are made then) and hands over automatic levels of detail.
- A sprite's quads live in one interleaved buffer whose attributes declare no count: `SourceOf` takes it from the
  buffer's size. An animating sprite rewrites the bytes in place: `GeometryBuffer::cpuRevision` (Init, Update, Unmap
  bump it) and `SceneRenderer::ContentRevision` key the GPU mesh, checked each frame.
- A generic material's own light, as the old shader read its flags: unlit without Diffuse, PBR or cel shading;
  Lighting2D - distance alone, no angle; VertexColor - its colours per vertex (`surface.vert@VERTEX_COLOR`, the tint
  multiplied); its blend - added (Src_Alpha, One), premultiplied (One, One-minus), or over.
- The see-through in the old renderer's order: by sort order, then by depth from the eye (along the view when
  orthographic), equal ones later-in-the-scene first (its stable sort, drawn backwards) - the layering a 2D scene was
  authored against.
- A 2D scene's picture as painted: a fixed exposure, no filmic curve (`ApplySceneSettings(..., twoD)`).
- The player's orthographic camera (a 2D scene's own view) is the renderer's orthographic view. A 2D game plays on
  the renderer (Showcase2D: the hero animating, physics, coins, parallax, the camera following).
(The torch glow and the 2D occluder shadows: done since - see "2D shadows, blending and the ambient colour".)

## The editor on the renderer by default (2026-10-30)

PyrosBuilder, the player and DemoLauncher are all drawn by the renderer where the build has it (Metal, Vulkan; WebGPU in
the browser); `PYROS_RENDERER=1` brings the old renderer back for any of them. The editor's Render Targets tab lists
every the renderer alive (`SceneRenderer::Live`, named: the Scene View per document, the camera, material, animation and
2D previews, the thumbnails and bakes) and their targets as pictures (`DebugTargets`). Its object icons are ImGui's
glyphs (the old renderer's billboard copies of them are not drawn). Not on the renderer yet: the old renderer's
reflection probes, irradiance probe bake and DDGI volumes (the renderer lights from its sky's harmonics and its
screen-space reflections instead), 2D occluder shadows.

## 2D shadows, blending and the ambient colour (2026-10-30)

- **2D occluder shadows.** `Occluder2D::PublishSceneOccluders` keeps what it published (`PublishedSegments`, within the
  64-segment budget). `SceneRenderer::WriteLights` puts those segments after the point and spot lights in the same
  buffer, with their count in the header's second word. `surface.frag`'s `Shadow2D` (Lighting2D mode) counts the
  segments crossed between the point and each light: two or more crossings means shadow. One crossing is the caster's
  own edge on the way out. The test is the old shader's (half-open along the segment), and the shapes match it pixel
  for pixel. The player publishes before each the renderer frame; the editor already did.
- **Blended means blending or transparent** (`Blended()` in SceneRenderer.cpp), as the old renderer's material state
  decides: a sprite with an alpha-cut texture has blending on without being transparent, and it was drawn as an opaque
  quad.
- **The ambient colour is sRGB**, like the background colour next to it (`ApplySceneSettings`), as the old renderer's
  `ScaleAmbient` takes it. Raw, a 0.2 ambient lit a 2D scene 2.4x too bright. Measured against the old renderer on a
  2D shadow scene: within 2/255.

## Erangel in the browser: its terrain, and the renderer's levels of detail (2026-10-30)

- Erangel's title runs in Chrome on WebGPU (its whole 600 MB packed, as Build Game packs it today). Its terrain was
  missing there: 2 of its 16 tiles drawn, the water over the rest. Every tile loaded, but the scene listed the meshes
  of only 2. The scene lists a component's meshes **of the level the old renderer last chose** (`UpdateLOD`), and
  the renderer walked that list. It now takes the components with levels from the scene's components
  (`GetRenderingComponents`) and chooses each one's level itself, so no other renderer's state decides what it draws.
- The level is chosen as the old renderer chooses it: the distance to the component's LOD sphere
  (`RenderingComponent::LODDistanceSQR`, now shared by both), not to its origin - a tile's origin is a corner, and a
  camera standing on it got a coarse level - and `LevelAtDistanceSQR` (no remembered answer: `GetLODByDistance` returns
  the old renderer's level when the distance has not changed).
- The renderer gives a model without levels its automatic ones (`TryAutomaticLODs`) the first time it sees it, as the
  old renderer does. It never asked, so automatic levels were made only when an old renderer also ran.
- The web page passes its `PYROS_*` URL parameters to the game as its environment (`?PYROS_LOG_LEVEL=3`,
  `?PYROS_RENDERER_STATS=1`): the switches a desktop run is given, for a build nothing else reaches.
- Measured for a streaming loader (not built yet): Erangel's title scene references 193 MB of its files, Island
  319 MB, Desert 186 MB, all of them and the scripts 347 MB of the 600 MB packed - the rest unreferenced copies.

## Web builds: identical files stored once, surfaces compiled, the renderer's template (2026-10-30)

- `game.data` holds each distinct file once: a file whose bytes are already packed points at that copy (Build Game:
  size and two independent 64-bit hashes; `tools/pack_web_game.py`: size and SHA-256). The page reads every file by its
  own offset and size, so it is unchanged. Erangel: 600.6 MB -> 417.6 MB.
- Build Game for the web compiles every `<shader>.surface` the game ships into its `surfaces/`
  (`SceneRenderer::WriteSurfaceCache`: the same texts and cache names the renderer reads, `SurfaceCacheName`), so a
  game's custom materials draw in a browser without a native run first. An editor without the WGSL converter warns.
- Build Game's web template is the renderer's WebGPU player first (`build_web_r2/player`), then the WebGL ones.
- A build without threads (the browser's) makes no automatic levels of detail: they are made on a thread of their
  own, and the renderer now asks for them - its first browser run threw "thread constructor failed".

## Web builds in two parts: the first scene's files first (2026-10-30)

- A web game is packed in two parts when the second is at least a fifth of it (Build Game's `PackWebGame` and
  `tools/pack_web_game.py`, the same rules): `game.data` holds what the startup scene reads, and `game.more.data`
  everything else. The first part is the scene's file, the files it names, the files those name, and so on, never
  into another scene's file. It also takes what every script names, including a bare file name a script completes
  at run time (`"heli_body.p3dm"` after its `"models/"`: any file whose path ends so), every script, shader, surface
  and font (the engine opens its own fonts by name), and any file of 256 KB or less. An entry in the second part says
  `"part": 1`, and the index names `more` and `moreSize`.
- The page starts the game from the first part, then fetches the second, with `/.pyros-files-coming` in the file
  system until it is written. The player starts no other scene while that file is there
  (`ApplyPendingSceneLoadIfAny`), so a scene's files are present when it loads. Tested: a scene asking for another at
  once, the second part held back 15 s - it waited, then loaded.
- Erangel: 183.7 MB before its title, 233.9 MB while it runs (was 600.6 MB before it, then 417.6 MB before it once the
  duplicates went). The title scene draws in Chrome with no file missing.
- Not certain: a file a script names only through a computed string (no literal part of its path) and opens in the
  first scene would not be in the first part. Erangel's title has none.

## The renderer: reflection probes (2026-10-30)

- A scene's `ReflectionProbe`s are seen by the renderer itself (`SceneRenderer::CaptureProbe`). A renderer of its own
  (`probeRenderer`: no probes of its own, no temporal AA, no screen-space reflections, this renderer's sky, clouds,
  ambient and surface cache) draws the probe's six 128 x 128 views, one view a frame. A renderer's frame data is its
  frame's: six views in one frame were all drawn with the last one's camera. Its linear picture, before exposure, is
  copied into this renderer's six faces (`View::hdrCopy`). A view with a surface whose pipeline is not ready yet is
  drawn again the next frame.
- `probefilter.frag` filters the six faces into the probe's block of the probe atlas (RGBA16F, 4 x 4 blocks of
  256 x 272): the old renderer's layout, longitude across from -Z and six roughnesses 256 wide halving to 32, one under
  the other, each a GGX lobe summed over the faces (N = V = R). The atlas is made the first time a scene has a probe;
  every surface group names it (binding 19), so the targets and groups are made again then.
- The probes' boxes follow the lights and the 2D casters in the lights buffer (count: the header's third word), the
  smallest box first. `surface.frag`'s `ProbeReflection` takes the first box holding the point, box-projects the
  reflected ray (where it leaves the box, from where the probe was seen), and fades over the probe's `blend` at its
  faces. With a sky, the probe replaces the sky's reflection under the screen-space one; without a sky - an
  interior, which had no reflection at all - it is the reflection.
- A probe is seen again when `Capture()` is asked (`GetCaptureRevision`) or when it has moved (an editor's drag).
  The editor no longer runs the old renderer's `ReflectionProbes::Update` under the renderer. The Render Targets tab
  shows the atlas.
- Tested: a closed room with a mirror floor, on Metal, Vulkan (validation clean) and Dawn WebGPU (validation clean),
  the three alike. The first frames after a rebuild draw nothing until the pipelines are made (Vulkan and Dawn
  compile them cold): a capture at frame 200 then is black - wait for them, it is not the probes.

## The renderer: DDGI - a scene's solved light (2026-10-30)

- A DDGI scene (ambient mode 3) is lit by its volume on the renderer too. `SolveSceneGI` (SceneGI.cpp) reads the solve
  kept in `<scene>.gi`, else solves on the CPU (`BakeSceneGI`) and keeps it. The cache format moved out of the old
  renderer into `ReadSceneGICache` / `WriteSceneGICache`, and both renderers use them. The player (`GISettings`, one
  function for both renderers) and the editor (`BakeDDGI`) hand the volume to the renderer:
  `SceneRenderer::SetGlobalIllumination`, held, not owned.
- Uploaded as the old renderer had it: irradiance (RGBA16F, filtered), visibility moments (RG32F - squared distances
  past a half float's range - read by texel with manual bilinear filtering: `BindingKind::UnfilteredTexture2D`, new,
  WebGPU's unfilterable float, since 32-bit filtering is a feature not every device has), the probes' offsets
  (RGBA32F), and the reflections (RGBA16F, every roughness level's tiles). Bindings 21-24; a custom surface's own
  textures moved to 28 and up (24-27 kept for the renderer). The grid follows the reflection probes in the lights
  buffer, two entries, with the header's fourth word 1.
- `surface.frag`'s `DDGIIrradiance` and `DDGIReflection` are the old shader's `SampleDDGI` and `SampleDDGIRadiance`:
  the eight probes round the point (looked up a quarter spacing off the surface), each weighted by a Chebyshev test
  against what it sees that way and by facing the point. The irradiance replaces the sky's or the flat ambient; the
  reflection replaces the sky's, and a reflection probe and the screen-space reflection go over it.
- A dynamic volume is refreshed each frame on the CPU (`UpdateSceneGI`, 2 ms a frame, the old renderer's CPU ceiling;
  the scene's rays refit or rebuilt as things move) and written into its textures in place (`GlobalIlluminationChanged`:
  a volume of the same shape is uploaded, not made anew). The light follows a lamp that moves.
- Tested: Radiance's Cornell box (294 probes, dynamic) in the player and the editor, on Metal, Vulkan and Dawn
  WebGPU (validation clean); its look matches the old renderer's - the walls' colour bled onto the white ones, the
  mirror balls reflecting the room.

## The renderer: exposure for dim interiors, and the background as set (2026-10-30)

- Auto exposure (`adapt.frag`) brightened a scene by two stops at most: a room lit by one lamp - Radiance's Cornell
  box, a point light of 1 at 4 m and no ambient - stayed a near-black box. It now adapts in full for two stops, then
  60% of each stop more, up to six in all: a dim room comes up to be seen and still reads as dim.
- With no sky, the exposure meters only what was drawn (`luminance.frag`: the prepass depth, 0 where nothing was),
  and the tonemap shows the cleared background as it was set, unexposed (`tonemap.frag`). It is a backdrop's colour,
  not light the scene is exposed for: metered, it pulled a dim room's exposure up; exposed, it went grey around the
  room and a teal sea went pale. Scenes with a sky or a skybox meter and expose everything, as before.
- What changed in the demos: their backgrounds show as their scenes set them (demo 23's sea a light blue where
  exposure had made it teal; NeonPulse's navy; the teapots' grey); the Cornell box is lit; white objects alone on a
  backdrop (demo 18's monkeys) read a little greyer, metered on themselves; the particles demo is brighter.
