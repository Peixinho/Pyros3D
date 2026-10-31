# Renderer audit: what stands between Pyros3D and an AAA picture

> Historical: an audit of the old renderer (IRenderer, Forward/Deferred), removed on 2026-10-30. The renderer it
> led to is `include/Pyros3D/Rendering/Renderer` - see docs/device-design.md.

Written 2026-10-27, after weeks of patching visuals on top of the renderer instead of fixing it.
The conclusion first: **the foundation is the device layer, and it is the wrong shape.**
Everything below it was built by me (Claude) when the Vulkan and Metal backends went in.

## 1. The core problem: an OpenGL-shaped device API, emulated on Vulkan and Metal

`IRenderDevice` (include/Pyros3D/Rendering/Device/IRenderDevice.h, ~180 virtuals) is a GL state machine:
`SetBlendFunction`, `SetDepthTest`, `SetCullFaceMode`, `BindTextureToTarget`, `SetTextureWrapS/T/R`,
`SetTextureMinFilter`, `BindFramebuffer`, `SetDrawBuffers`, `SetVertexAttribute`, `BindUniformBlockIfPresent`.
The Vulkan backend (VulkanRenderDevice.cpp, 10,300 lines) and Metal (MetalRenderDevice.mm, 5,400) emulate it:
they watch the GL-style state, and build what Vulkan/Metal need (pipelines, descriptor sets, render passes,
samplers) lazily, at draw time.

What that causes, every one of them seen in this project:

| Symptom | Where it comes from |
|---|---|
| Frames held up for seconds the first time a material or state is seen (the Windows hangs) | `vkCreateGraphicsPipelines` called from the draw path (VulkanRenderDevice.cpp:3151), shaders compiled on demand (CustomShaderMaterial / GenericShaderMaterial variants) |
| CPU cost per draw: descriptor writes, UBO ring slots, "replace" of uniform blocks | per-draw uniforms (`UboReplace`, `UpdateDescriptorSets` in every frame's profile) |
| The device thread stalls the game | `ThreadedRenderDevice` must `Drain()` before 22 calls that read state back or create objects |
| Every feature a special case | no description of a frame: passes, targets and barriers are implied by the order of calls |
| Hard to add modern techniques (bindless, GPU-driven, async compute) | nothing in the API can say them |

## 2. What is sound and stays

- PBR shading: GGX / Smith / Schlick, prefiltered environment and BRDF table (PyrosShader.glsl).
- Cascaded sun shadows (up to 4), PCF; DDGI volumes, irradiance probes, reflection probes (Rendering/GI).
- ACES / Lottes tonemapping, HDR output; TAA with velocity, history clip, Catmull-Rom; FSR 1/3, MetalFX.
- GPU culling for foliage (InstanceCullGroup), terrain streaming, world streaming.
- The Lua/game layers, networking, physics, editor: untouched by this.

These are reasonable, but they sit on the device layer above, and each was bent to fit it.

## 3. Also wrong, smaller

- The colour grade (exposure, contrast, saturation) lives inside the Sky effect; it belongs in one
  post stage after tonemapping.
- The uber-shader (PyrosShader.glsl, 2,600 lines of `#ifdef`) compiles a variant per option set at
  run time; the option bits were extended for each feature.
- The deferred renderer is one long `RenderScene` (DeferredRenderer.cpp, 2,500 lines) with the pass
  order hard-coded.
- The model importer guessed at normals (`FixFacing`) and turned foliage's to the ground; fixed with
  `PYROS_IMPORT_AS_AUTHORED`, but it is a guess that should not run on authored assets at all.

## 4. The rebuild, in order (each step shown to Duarte before the next)

1. **Explicit device API** (new `RenderDevice2`): pipeline state objects, created from a material's
   description at load, never at draw; bindless textures and buffers; command lists recorded on any thread;
   explicit render passes and barriers. Vulkan and Metal implemented natively; GL as the emulated one
   (the reverse of today). The old API kept as a shim over the new one while the renderer moves across.
2. **Frame graph**: the frame described as passes reading and writing named resources; transient targets,
   barriers and scheduling derived from it. Shadows, G-buffer, lighting, SSAO, sky, translucency, post as nodes.
3. **GPU-driven geometry**: one indirect draw per material bucket, culling and LOD on the GPU for
   everything (not only foliage); meshlets where the backend has them.
4. **Shaders as data**: material permutations enumerated and compiled at build time (the shader cache
   shipped), no run-time compiles.
5. **Lighting and image quality on top**: stable cascades + contact shadows, GTAO, sky and aerial
   perspective, TAA tuned with sharpening, one post grade stage.
6. **Vegetation and terrain systems**: authored normals, two-sided lighting, translucency, octahedral
   impostors, distance thinning; terrain PBR layers with height blending.

Each step comes with a test scene, a frame-time budget (never slower than before), and side-by-side frames
against the reference video, for Duarte to judge.
