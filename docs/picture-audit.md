# Pyros3D: everything that decides the picture, audited (2026-10-28)

> Historical: an audit of the old renderer (IRenderer, Forward/Deferred), removed on 2026-10-30. The renderer it
> led to is `include/Pyros3D/Rendering/Renderer` - see docs/device-design.md.

Six areas, read in the code: materials and shading, shadows, indirect light (GI, AO, reflections), sky and
post-processing, vegetation / terrain / LOD, textures and import. Each finding gives where it is, what it
does on screen, and what AAA engines do. **[checked]** = re-read in the code for this document;
**[V]** = read in the code during the audit; **[I]** = inferred from the code, not measured.

The device layer (docs/device-design.md) does not appear here: it changes how commands reach the GPU, not
the picture. It stays, underneath, for the hangs and stutters.

## The short version

The picture is wrong at the level of design, not in one place. In order of how much of the screen it affects:

1. **The engine lights in gamma space.** Colour textures are uploaded as UNORM, never sRGB, on Vulkan,
   Metal and GL, and the shader uses the texel as albedo; the game's effect chain has no encode at the end.
   Every lighting sum, blend, filter and mip runs on gamma-encoded numbers. **[checked]**
2. **The image transform crushes and clips.** The grade inside SkyEffect does contrast in linear light round
   0.42: with the scenes' 1.25 every value under 0.084 becomes black. A per-channel shoulder then caps
   everything under 1.0, so bloom (threshold 1.6) never fires. This soft clip is the de-facto tonemapper;
   the real Tonemap effect is not in the games' chains. **[checked]**
3. **Shading is internally inconsistent.** Deferred reflections combine a view-space normal with a
   world-space view vector; ambient uses the vertex normal; point/spot lights fall off linearly; G-buffer
   channels are shared between unrelated features. **[checked]**
4. **Indirect light is never occluded at scale.** One global SH sky; a 0.5 m SSAO is the only occlusion,
   and it also darkens direct sun. Under trees and inside houses stays sky-lit. **[checked in part]**
5. **Shadows end at 150 m, foliage stops casting at 60 m (by 32 m blocks), far trees never cast.** **[checked]**
6. **Vegetation pops.** Dithered fades only exist in alpha-cut materials; elsewhere instance counts are cut
   per block. **[checked]**

Fixing (1) changes every pixel and means retuning every light, sky and grade in every scene, because they
were all tuned by eye in the wrong space.

---

## 1. Colour pipeline and post-processing

| # | Finding | Where | On screen | AAA |
|---|---|---|---|---|
| 1.1 | Colour textures never decoded from sRGB; no output encode in the game chain | VulkanRenderDevice.cpp:5656 (RGBA→R8G8B8A8_UNORM), MetalRenderDevice.mm:3254, BC1/BC3 UNORM; PyrosShader.glsl:2002-2006 **[checked]** | Lighting falloff, terminators, colour mixing, fog, filtering all wrong; materials never respond like real surfaces | sRGB formats by texture role, linear lighting, one output transform |
| 1.2 | Contrast in linear light round 0.42, clamped at 0 | SkyEffect.cpp:423,428 **[checked]** | Darkest third of the image goes pure black: shadows, foliage interiors, night | Grade in log / ACEScct round 0.18, or an S-curve that never clips |
| 1.3 | Per-channel shoulder caps output under 1.0 = the real tonemapper | SkyEffect.cpp:424-427 **[checked]** | Bright colours drift yellow/cyan then flatten; no HDR | Hue-preserving filmic / ACES / AgX as the last scene-referred step |
| 1.4 | Bloom after the shoulder, threshold 1.6 | BloomEffect.cpp:78-80; Island.json **[checked]** | No glow on sun, highlights, emissives | Bloom on scene-linear HDR before the tonemap, small energy, no hard threshold |
| 1.5 | Effect order is whatever a scene lists | PostEffectChain.cpp:236-424 **[V]** | Smoke after haze and grade; no guaranteed exposure→fog→bloom→tonemap→grade | Fixed pipeline order |
| 1.6 | Exposure fixed (with a sunset multiplier); AutoExposure ±2 EV, mean-log, unused | SkyEffect.cpp:420; AutoExposureEffect.cpp **[V]** | Day/night/interior never right together | Histogram exposure, percentile rejection, ±10 EV, physical sun/sky units |
| 1.7 | Upscalers (MetalFX, FSR) get the finished graded frame | PostEffectsManager.cpp:1057 **[V]** | Grain, weather, bloom accumulated/ghosted | Upscale scene-linear HDR, then post at output res |
| 1.8 | No dither before 8-bit output | grep; GLRenderDevice.cpp:95 **[V]** | Banding in sky, haze, fog | Blue-noise dither or 10-bit |
| 1.9 | No grading LUT; Tonemap effect uses pow 2.2 (double encode if used on an sRGB target) | TonemapEffect.cpp:68 **[V/I]** | No artist grades | 3D LUT after tonemap, piecewise sRGB |
| 1.10 | TAA: 5-tap plus clamp, no depth/velocity rejection, no reconstruction filter, current weight 0.45 when moving | AntiAliasingEffects.cpp:324,336,375,383 **[V]** | Ghosting on disocclusion, shimmer in motion | 3x3 clip in YCoCg, depth + velocity rejection, filtered current sample |
| 1.11 | No mip LOD bias under TAA / upscaling | samplers, grep **[V]** | Softer textures than authored | log2(render/output) − ~0.5 |
| 1.12 | Sharpening and FSR1 fed linear colour | SharpUpscaleEffect.cpp, FsrEffect.cpp **[V]** | Weak on highlights, strong in shadows | Perceptual input |
| 1.13 | DOF built into Sky, fixed 22 m focus, 8 taps; motion blur without neighbour-max | SkyEffect.cpp:229-246; MotionBlurEffect.cpp:179-200 **[V]** | Smeared far field; silhouettes don't blur | CoC with near/far, bokeh; tile/neighbour-max blur |

## 2. Sky, atmosphere, clouds

| # | Finding | Where | On screen | AAA |
|---|---|---|---|---|
| 2.1 | Sky is a hand-tuned zenith/horizon gradient | SkyEffect.cpp:197-221 **[V]** | Painted, flat sky; sunsets wrong | Hillaire 2020 (transmittance, multi-scatter, sky-view, aerial-perspective LUTs) |
| 2.2 | Haze is one constant colour mixed in, not tied to sky or sun; max 0.92 | SkyEffect.cpp:258,336-337 **[V]** | Grey wash; seam where land meets sky [I] | Aerial perspective from the same atmosphere model |
| 2.3 | Volumetric light is a separate fog with its own colour | VolumetricLightEffect.cpp:133 **[V]** | Doubled, mismatched fog if both used | One participating-media model |
| 2.4 | Sky brightness not in physical units; sun disc ×6 | SkyEffect.cpp:212 **[I]** | Sky, IBL and sun disagree | Physical radiance, exposure handles range |
| 2.5 | Clouds: value noise, 4 light steps, constant ambient, 32 steps to 24 km; shadows from a different (flat) layer; bilinear half-res upsample | SkyEffect.cpp:117-186,294,327-335 **[V]** | Blobby, flat bases, horizon banding; shadows don't match clouds | Perlin-Worley 3D noise, multi-scatter octaves, reprojected quarter-res march, shadows from the same clouds |
| 2.6 | Stars: step hash on a plane | SkyEffect.cpp:216-218 **[V]** | Aliased, shimmering | Star map with extinction |

## 3. Materials and shading

| # | Finding | Where | On screen | AAA |
|---|---|---|---|---|
| 3.1 | Deferred reflections: view-space normal with world-space view vector into Env/Probe/DDGI specular | PyrosShader.glsl:2034 vs 2486-2497 **[checked]** | Reflections slide/rotate with the camera, wrong Fresnel; forward differs | One space throughout |
| 3.2 | Point/spot falloff linear `1 − d/R` | PyrosShader.glsl:690; secondpassPoint.glsl:193; secondpassSpot.glsl:140 **[checked]** | Flat light pools with a hard edge | 1/d² with a windowed cutoff |
| 3.3 | Ambient uses the vertex normal, not the mapped one | PyrosShader.glsl:1888,1909,1913 **[checked]** | Detail vanishes in shade | Irradiance with the per-pixel normal |
| 3.4 | Leaf light-through per vertex, unshadowed, deferred only | PyrosShader.glsl:466-476, 2470 **[checked]** | Crowns glow inside; no dark interiors; forward foliage gets none | Two-sided foliage model, shadowed transmission |
| 3.5 | G-buffer channels shared: SSS colour = F0 tint, SSS strength = SSR flag; indirect baked into alphas | PyrosShader.glsl:2537-2545 **[checked]** | Features exclude each other; no screen-space indirect possible | G-buffer laid out by meaning, octahedral normals |
| 3.6 | No multiscatter energy compensation; diffuse ambient not reduced by (1−F) | PyrosShader.glsl:869-904, 2377 **[V]** | Rough metals too dark; bright rims | Kulla-Conty / Fdez-Agüera |
| 3.7 | No minimum roughness, no specular AA | PyrosShader.glsl:836, grep **[V]** | Sparkles, shimmer on glossy normal-mapped surfaces | Clamp ~0.045, Kaplanyan/Toksvig |
| 3.8 | Tangents per triangle (last wins), not MikkTSpace; no handedness; ModelMatrix not inverse-transpose; no green flip | Primitive.cpp:26-39; PyrosShader.glsl:567-571 **[V]** | Seams, faceted bumps, baked maps wrong, inside-out on mirrored UVs | MikkTSpace, tangent.w, per-pixel bitangent |
| 3.9 | Parallax: variant without bump doesn't compile; UVs outside [0,1] discarded; derivatives inside a loop | PyrosShader.glsl:1063,1095,1993,2024-2031 **[V]** | Holes in tiled materials, shimmer | POM with explicit gradients |
| 3.10 | AO map channel ignored (ORM R unused); no clear coat, sheen, anisotropy, detail maps | PyrosShader.glsl:123, 2113-2119 **[V]** | Baked AO lost; limited materials | Full material model |
| 3.11 | Subsurface = wrap lighting + back lobe | PyrosShader.glsl:992-1005 **[V]** | Skin, leaves don't read | Profile-based SSS |
| 3.12 | Custom (Material Editor) materials: emissive put into albedo in deferred, AO multiplied into albedo (darkens direct), no IBL specular | MaterialCodegen.cpp:890,931-932 **[V]** | Emissive lit by lights; dead metals and wet ground | Emission separate, AO on indirect only, full IBL |
| 3.13 | Forward non-PBR path: unnormalised Blinn-Phong; specular sum added into diffuse; an uninitialised variable | PyrosShader.glsl:827,2172,2264; lighting.glsl:335 **[V]** | Forward and deferred differ for the same material | One lighting model |
| 3.14 | ~30 #define flags compiled on the draw path; G-buffer sibling built synchronously | GenericShaderMaterial.cpp:49-111,458 **[V]** | First-use hitches; fragile combinations | Materials as data, permutations compiled at build time |

## 4. Indirect light: sky, GI, AO, reflections

| # | Finding | Where | On screen | AAA |
|---|---|---|---|---|
| 4.1 | No large-scale sky occlusion: one global SH, unoccluded sky specular | PyrosShader.glsl:1892-1909 **[V]** | Under trees, alleys, interiors stay sky-lit | Sky visibility (DFAO / baked / bent normals) |
| 4.2 | SSAO multiplies direct sun by default | DeferredRenderer.cpp:413; secondpassDirectional.glsl:586 **[checked]** | Dirty halos in sunlit crevices and grass bases | AO on indirect only |
| 4.3 | AO also darkens emission and reflections; no specular occlusion | secondpassAmbient.glsl:98 **[V]** | Glow dims in corners; reflections scaled by diffuse AO | Emission after AO; Lagarde / GTSO specular occlusion |
| 4.4 | SSAO: 16 samples, 0.5 m, binary votes, strength 2, depth-only blur, no multi-bounce | deferredSSAO.glsl:117-162; deferredSSAOBlur.glsl:61-68 **[V]** | Grey noisy AO, halos | GTAO, multibounce, temporal denoise |
| 4.5 | DDGI default 6×4×6 probes over the whole scene; normal-only offset; back-face hits lit; azimuth-only ray rotation; fixed hysteresis | SceneGI.h:60-63 **[checked]**; PyrosShader.glsl:1705-1709; ddgi_update.glsl:117-123,289 **[V]** | GI a smudge, leaks, lag | 1–3 m camera-centred cascades, RTXGI bias, back-face handling, random rotation |
| 4.6 | With an environment map, DDGI specular off and the open sky replaces it | PyrosShader.glsl:1811,1519 **[V]** | Indoor floors reflect the sky | Sky specular occluded by GI visibility |
| 4.7 | Environment prefilter is a Gaussian; 512×256 equirect mirror level fetched without mips; BRDF LUT single-scatter | EnvironmentMap.cpp:39-45,121-123; BRDFLut.cpp **[V]** | Blurry mirrors, no GGX tails, zenith pinch | GGX importance-sampled cube, roughness mips |
| 4.8 | SH probe grid sampled once per object | IRenderer.h:209-223 **[V]** | A whole floor/terrain gets one ambient | Per-pixel |
| 4.9 | Reflection probes: first box wins, no blending, max 16 | PyrosShader.glsl:1564-1587 **[V]** | Seams and pops between rooms | Blended probes, normalised by local irradiance |
| 4.10 | SSR blended over a frame that already has specular; "mirror floor" 0.55; opt-in only; cut at roughness 0.6; deferred only | lastPass.glsl:142,376,488,541,543 **[V]** | Double reflections, smeared gloss, missing reflections | SSR replaces indirect specular under a confidence mask |
| 4.11 | Forward has no AO and no SSR | ForwardRenderer **[V]** | Same scene looks different per renderer | Same features both paths |

## 5. Shadows

| # | Finding | Where | On screen | AAA |
|---|---|---|---|---|
| 5.1 | Past the last cascade: fully lit (150 m on "high") | secondpassDirectional.glsl:321; PyrosShader.glsl:1273 **[checked]** | Far trees, buildings, ground fully sunlit; a fade band | Far cascade / cached far map / distance-field shadows |
| 5.2 | Foliage casts only within 60 m, switched per 32 m block | Foliage.h:57; Foliage.cpp:371 **[checked]** | "Trees shadowed only near"; shadow patches pop | Cast to the end of the cascades, dithered fade |
| 5.3 | Far-forest impostors never cast | FarForest.cpp:425 **[checked]** | No shadow under the far forest | Impostors that cast |
| 5.4 | Last cascade ~300 m wide on 2048 texels (~15 cm a texel); 2×2 atlas wastes a quarter with 3 cascades | DirectionalLight.cpp:187,255 **[V/I]** | Blobby mid-distance shadows | 4+ cascades, better distribution, VSM-like density |
| 5.5 | Cascade box clamped to scene extent changes texel size | DirectionalLight.cpp:423-429 **[V]** | Shimmer near map edges | Constant texel size |
| 5.6 | Moving sun rebuilds the light rotation every frame; cascades can hold different sun directions | DirectionalLight.cpp:276; IRenderer.cpp:4252-4258 **[V]** | Crawling edges with time of day; doubled shadows in blend bands | Quantised sun direction, all cascades together |
| 5.7 | Far terrain casts from camera-LOD geometry | IRenderer.cpp:3591,3652-3681 **[I]** | Flickering mountain shadows | Fixed terrain shadow proxy / heightfield shadow |
| 5.8 | Wind casters frozen in the kept cache | IRenderer.cpp:2512 **[V/I]** | Foliage shadows shake at patch seams | Animated casters only in the dynamic layer |
| 5.9 | No blocker search (no contact hardening) without RT; fixed per-pixel disc rotation; no receiver-plane bias | PyrosShader.glsl:1212,1245-1253 **[V]** | Uniformly soft shadows; grain TAA can't average | PCSS, temporally rotated kernel |
| 5.10 | Alpha-tested casters: plain discard with implicit mips; LOD dither drawn into the map | PyrosShader.glsl:2415-2432 **[V/I]** | Leaves vanish from far shadows; stippled shadows | Mip-aware alpha test |
| 5.11 | RT shadows: casters near reach removed from maps but faded from rays; foliage/characters excluded; no denoise | DeferredRenderer.cpp:2483-2495; secondpassDirectional.glsl:419 **[V]** | Missing shadows near RT edge | Consistent hand-off, denoiser |
| 5.12 | Contact shadows off by default, crude, directional only | DirectionalLight.h:243; secondpassDirectional.glsl:470-497 **[V]** | Objects float | Screen-space contact shadows on |
| 5.13 | Point cubes store non-linear depth with a fixed pull bias; seams | PyrosShader.glsl:1301-1342,2445 **[V]** | Acne/peter-panning far from the light | Linear distance, border-aware filtering |

## 6. Vegetation, terrain, LOD, streaming

| # | Finding | Where | On screen | AAA |
|---|---|---|---|---|
| 6.1 | Dithered fade only in alpha-cut instanced materials | PyrosShader.glsl:2423 **[checked]** | Grass tufts, far canopies, trunks pop | Dithered clip in every foliage material |
| 6.2 | CPU path fades by cutting the instance count per block | Foliage.cpp:366-368 **[checked]** | Bushes vanish along square edges | Per-instance dithered fade |
| 6.3 | GPU per-plant path only for single-submesh, same-material, non-shadowing layers | Foliage.cpp:250-252; InstanceCullGroup **[V]** | Real trees fall back to block LOD and pop | Multi-section per-instance LOD |
| 6.4 | Block LOD switches a whole 32 m block; time-based 0.25 s crossfade with a static dither | Foliage.cpp:276-287; RenderingComponent.cpp:379,650-656 **[V]** | Waves of trees swapping | Per-instance screen-size LOD, distance dither moving with TAA |
| 6.5 | Default reach 60/120 m; bushes have nothing from ~120 m to 700 m | Foliage.h:57; FarForest.cpp:252 **[V]** | Bushes appear only close | Impostors / clusters to the cull distance |
| 6.6 | Impostors are fixed crossed cards with bent normals, no atlas, no shadows | FarForest.cpp:58-99,425 **[V]** | Flat far trees, silhouette change at swap | Octahedral impostors (albedo, normal, depth) |
| 6.7 | Hard alpha test, no alpha-to-coverage; coverage-preserving mips only in BC path, cut fixed at 0.45 | PyrosShader.glsl:2430; BlockCompress.cpp:300-345 **[V]** | Aliased, thinning foliage | A2C / hashed alpha, cutoff-aware mips |
| 6.8 | Grass blades 1.8 cm wide, no minimum pixel width | GrassTuft.h **[V]** | Sub-pixel blades alias and crawl | Minimum pixel width, fade to ground colour |
| 6.9 | Velocity pass ignores wind | PyrosShader.glsl:510-518; VelocityRenderer.cpp:146-178 **[V]** | TAA ghosting on foliage | Wind in the velocity pass |
| 6.10 | Grass colour not linked to the ground | Foliage.cpp:131-142 **[V]** | Grass floats on the terrain | Ground colour via virtual texture |
| 6.11 | Terrain LOD is level switching, no morph; stand-in → tile swap instant with a colour seam | Heightfield.cpp:135-290; TerrainComponent.cpp:784-869,1125-1200 **[V]** | Popping, colour jump at load radius | CDLOD / clipmaps, far colour baked from the same material |
| 6.12 | Terrain layers: no normal/roughness maps, relief from dFdx (gone by 70 m), single-axis cliff projection | TerrainTools.cpp:91-99,229,318,327 **[V]** | Flat, faceted ground | Layer normal maps, blended triplanar |
| 6.13 | Tile instantiation is one indivisible main-thread step; up to 48 stand-in tiles built per frame | TerrainComponent.cpp:359-407,37; AssetStreamer.cpp:301 **[V]** | Stutter flying into new areas | Budgeted steps, off-thread builds |

## 7. Textures and import

| # | Finding | Where | On screen | AAA |
|---|---|---|---|---|
| 7.1 | (= 1.1) no sRGB formats | **[checked]** | | |
| 7.2 | Mips box-filtered on raw (gamma) bytes | BlockCompress.cpp EncodeMipChain; vkCmdBlitImage **[V]** | Muddy, dark distant detail | Kaiser/Lanczos in linear |
| 7.3 | Normal-map mips not renormalised; no roughness from variance | BlockCompress.cpp **[V]** | Distant sparkle, too glossy | Renormalise, Toksvig into roughness |
| 7.4 | Format guessed from pixels (BC1/BC3/BC5 only, no BC7/BC4); normals usually BC1 | BlockCompress.cpp ChooseFormat, :378 **[V]** | Blocky normals, bleeding packed maps | Format by role: BC7 / BC5 / BC4 |
| 7.5 | FixFacing negates bitangent on re-wind and guesses facing (default on) | AssimpModelImporter.cpp:82,136-141 **[V]** | Inverted bumps, mis-flipped foliage | Import as authored |
| 7.6 | Height maps loaded as normal maps; flat normals generated | AssimpModelImporter.cpp:563-570 **[V]** | Garbage shading, facets | Convert height→normal, smooth normals |
| 7.7 | Anisotropy 8x, fine | IRenderDevice.cpp:43-49 **[V]** | — | 16x by quality |

---

## What to redo, in order

Each step is built in the engine, measured (frame time, GPU per pass), and shown before/after in the engine's
own test scenes. Nothing is claimed beyond what the frames show.

1. **Colour pipeline** (1.1–1.12, 7.2–7.4): linear workflow (sRGB by texture role, linear mips, BC7/BC5/BC4),
   a fixed post order — exposure (histogram) → fog → bloom → tonemap (filmic, hue-preserving) → grade LUT →
   dither — and the grade taken out of SkyEffect. Then every scene's lights and sky retuned. Everything after
   depends on this.
2. **Material system, redone** (3.x): materials as data compiled at build time; one lighting model for forward,
   deferred and the editor; one coordinate space; G-buffer laid out by meaning; inverse-square lights;
   multiscatter; specular AA; MikkTSpace import; AO maps; a real foliage model with shadowed transmission;
   terrain layers with normal maps.
3. **Indirect light** (4.x): sky visibility at scale, GTAO on indirect only with specular occlusion, DDGI in
   camera-centred cascades with proper bias, GGX prefiltered environment, SSR replacing indirect specular,
   blended probes.
4. **Shadows** (5.x): coverage to the horizon (far cascade / distance fields / terrain heightfield shadow),
   foliage casting to the end with dithered fades, stable sun, cache without wind casters, PCSS, mip-aware
   alpha, contact shadows on.
5. **Atmosphere** (2.x): physically based sky and aerial perspective, clouds from the same model with
   matching shadows, physical units with the exposure from step 1.
6. **Vegetation and terrain** (6.x): per-instance dithered LOD in every foliage material, octahedral impostors
   that cast, a mid tier for shrubs, grass width and ground colour, terrain morphing, streaming in budgeted
   steps off the main thread.
7. **TAA and upscaling** (1.7, 1.10, 1.11, 6.9): depth and velocity rejection, wind velocity, LOD bias,
   upscaling before post.

---

## Progress

### Step 1, colour pipeline (2026-10-28) - in the engine, Vulkan and Metal

What changed:
- **Linear lighting.** Every colour that comes in is decoded from sRGB once, where it enters:
  colour and emission maps, material colours, vertex and instance colours, particles, Material Editor
  colour nodes, parameters and painted texture nodes (data maps by name stay as they are), terrain
  albedo, light colours (`ILightComponent::GetLightRadiance`), ambient colours, background clear,
  emission, the GI ray scene's light and albedo. `include/Pyros3D/Rendering/ColourSpace.h`,
  `p3d_ToLinear()` in the shaders.
- **One output transform at the end of every frame** (`OutputTransformEffect.cpp`,
  `PostEffectsManager::GetOutputTransform`): exposure (stops), contrast about middle grey, saturation,
  a filmic curve (ACES fitted; "neutral" and none also available) into half floats, then the display
  effects (Grade, Vignette), then the exact sRGB encode with a triangular dither. A scene sets it
  with an `Output` entry; defaults are filmic +1 stop for 3D, no curve for 2D.
- **Fixed frame order** whatever a chain lists: scene effects (SSAO/SSR, smoke, assets, Sky,
  volumetric light, DOF, motion blur, auto exposure, bloom), TAA, output curve, display effects,
  encode, FXAA/SMAA, upscaling. Tonemap and GammaEncode in old scenes are not run.
- **Half-float effect targets** (`IEffect.cpp`): the frame between effects was 8-bit, clipped to 0..1
  after the first effect.
- **Sky effect**: its grade (exposure, saturation, contrast in linear light round 0.42) and the
  shoulder that was the frame's tonemapper are gone; its colours, tuned encoded, are applied in
  encoded space and decoded back.
- **UI** drawn after the transform keeps its colours as authored (`ShaderUsage::DisplayReferred`).
- **Colour-texture mips** averaged as light (texture cache 005).
- Editor previews (material, animation, 2D character, camera, thumbnails, impostor bake) render
  through the transform into their own textures; the impostor bake encodes without a curve.

Seen (frames in the session's scratchpad `cp/`): Cornell box reads as lit, with saturated walls and
rolled-off highlights; Vulkan and Metal give the same picture.

Known limits, still to do:
- Filtering between texels happens on encoded values (the decode is in the shader, not an sRGB
  texture format); mips are right.
- Exposure is a fixed number per scene; the auto-exposure effect is still mean-log, ±2 EV.
- Temporal upscalers still receive the encoded frame (step 7).
- Editor helper overlay is drawn with decoded colours (greys slightly darker).
- Every scene's lights, ambient and sky values were tuned in gamma space and need retuning.
- GPU per-pass timestamps on the M3 are not reliable (sum exceeds the frame); passes to be measured
  with the repeat method.

### Step 2, materials and shading (2026-10-28) - first part

- **One lighting model** - `resources/shaders/p3d_lighting.glsl`, included by PyrosShader.glsl and the
  three deferred light passes, and copied into every Material Editor shader by the codegen (generated
  shaders are rewritten when a project opens). GGX, height-correlated Smith, Schlick, multiscatter
  energy compensation (3.6), a roughness floor and specular anti-aliasing (3.7), inverse-square falloff
  windowed at the radius (3.2), the Fresnel-weighted ambient diffuse (3.6), specular occlusion.
- **Classic Diffuse materials** lit by the same GGX in forward as in deferred (3.13).
- **Ambient on the mapped normal** (3.3); **deferred reflections in world space** (3.1).
- **Leaf light-through** now goes through the shadowed subsurface path, forward and deferred (3.4).
- **SSAO off direct light** by default (4.2).
- **Material Editor materials**: emission and occlusion out of the albedo, the ambient weighted (3.12).
- **Normals**: the cofactor normal matrix (non-uniform scale), skinning bends the tangent frame,
  tangents orthogonalised, bitangent from the handedness (3.8, shader side).

Measured: Cornell box, forward against deferred on Metal - back wall 163,166,134 / 162,165,133; left
wall 139,0,4 / 140,0,4; floor 116,119,80 / 117,118,80 (within 1-3 of 255 everywhere sampled).

Still open in this step: the G-buffer laid out by meaning, with the indirect light computed after it
(so AO, specular occlusion and SSR combine properly - also the base of step 3); materials compiled at
build time; an AO map slot; reflections for Material Editor materials; MikkTSpace at import.
Point-lit scenes (MetroStation) are 5-10x darker under the inverse square: their light intensities need
retuning.

### Steps 3-7, first parts (2026-10-28)

- **Indirect (3)**: SSAO replaced by GTAO (horizon-based, analytic visibility, falloff at the radius) with
  the multi-bounce response applied in the ambient passes by albedo.
- **Shadows (4)**: foliage casts out to the sun's shadow reach (`shadowDistance` 0, the new default -
  Island's saved 0 meant only the viewer's own block cast); leaves past the cascades take a canopy's
  average self-shadow (0.45) instead of full sun; a caster's shadow level chosen by the cascade's texel,
  not the camera's level (no change of shadow as the view moves); the sun direction quantised to 0.05
  degrees for the maps; shadow-pass alpha sampled two mips finer.
- **Atmosphere (5)**: `uAtmosphere` in the Sky effect - Rayleigh, Mie (uAirHaze), ozone on a round
  planet, single scattering marched with a multiple-scattering share; the sky, the aerial perspective
  over the land and the haze colour for clouds all from it.
- **Vegetation (6)**: per-plant dithered fade for every vegetation material (not only cut-outs); the
  CPU thinning path dissolves the plant at the edge of a block's count (`SetInstanceKeep`).
- **TAA (7)**: 3x3 neighbourhood, clip toward the box centre, lower new-frame share in motion; material
  textures sampled half a mip sharper.

### Before and after, measured (Metal, editor viewport, `PYROS_GPU_SPLIT=1` exact GPU time)

Baseline: the last commit (36bf14a2) built in a worktree; same scenes, same cameras.

| scene | before | after |
|---|---|---|
| MetroStation (lamps retuned x5.8 for the inverse square) | 15.2 ms | 17.4 ms (first run; varies a few ms) |
| Radiance Cornell | 10.5 ms | 5.4 ms |
| NatureLab, open view | 26.9 ms | 27.1 ms |

Seen: the Cornell box's metal spheres reflect the room (they were nearly black: reflections in the wrong
space); the metro has pools of lamp light and glossy floor reflections instead of one washed brightness;
the mountains have a deeper sky and more contrast in shade. Frames: the session scratchpad's `ab/`.
In both builds most of NatureLab's time is its first shadow cascade (~15 ms).

### Step 8: octahedral impostors in use, and the eye

- **Impostor quad**: the corners were laid out in view space and came out with only the lower half on
  screen; now built in world space from the camera's right and up vectors, with the atlas's v flipped to
  match the bake. An impostor is lit as leaves (light through it, canopy self-shadow past the cascades).
- **Every NatureLab tree prefab (42)** baked (8x8 views, 256 px each) and given a far level at 250 m: the
  impostor on its leaves part, nothing on its wood (`{"kind":"none"}`, a new empty renderable for a level
  that draws nothing). Prefab backups: the session scratchpad's `prefab_backup/`.
- **Exposure follows the light** in every 3D scene: AutoExposureEffect's meter and adapt passes run before
  the output curve. The adapt pass now meters as engines do: a histogram of log luminance, the mean
  between its 50th and 95th percentiles (a bright sky in a corner or a lamp no longer swings it), eased
  over time, quicker into the light. The curve brings that to middle grey within +/-10 stops, +1 stop
  (the ACES toe). 2D scenes keep a fixed exposure. `Output`'s `uAuto` 0 turns it off for a scene.

| scene (exact GPU time) | last commit | step 7 | now |
|---|---|---|---|
| NatureLab, open view | 26.9 ms | 29.8 ms | 23.6 ms |
| NatureLab, second view | - | 27.1 ms | 20.0 ms |
| Radiance Cornell | 10.5 ms | 5.4 ms | 4.3 ms |
| MetroStation | 15.2 ms | 17.4 ms | 10.0 ms |

(The Metro and Cornell drops were not targeted at anything in this step; runs vary by a few ms, so those
two are worth re-measuring before being counted.)

### Step 8, continued: impostors matched to the trees, and what the measurements were hiding

- **Impostor bake**: radius from the tree's farthest vertex, not its box corner (birch 10.4 -> 8.0: each
  view spends ~70% more of its pixels on the tree); each view's depth gives a crown occlusion (how many
  leaves round a point stand in front of it) stored in the normal atlas's alpha.
- **Impostor shading**: that occlusion is the leaf's sun visibility (`p3d_SunOpen`, carried to the deferred
  directional pass in the G-buffer reflectivity alpha, which subsurface surfaces do not use) and a mild
  ambient occlusion; light-through 0.45 as the leaves' own material. Sampled with explicit gradients
  (neighbouring pixels pick different views; the hardware derivatives across that jump read a mip so coarse
  the crown blurred and lost its cut-out) and alpha scaled by mip level so a far crown keeps its cover.
- **Measured against the real birch** (fixed exposure, one session): same size and silhouette at 30, 80 and
  200 m; dark interior and colour close in both renderers. All 42 prefabs re-baked.
- **CPU, 6-7 ms a frame in the editor**: `TerrainEditor::HeightAt` re-walked the whole scene for its tile
  list every frame, because the list was cached for one scene per thread and the editor asks in two (the
  scene and a preview). Now cached per scene: `Occlusion.Update` 5-7 ms -> 0.004 ms, `GBuffer.Visible`
  5.4 -> 0.3 ms.
- **Profiler**: exact-split mode now runs compute alone and lists it; a `GPU.BusyTotalMs`/`GPU.Frames`
  counter pair gives GPU time over a window. Caveat found: in split mode a pass right after a long CPU gap
  runs at a lowered GPU clock - NatureLab's "cascade 0 = 11 ms" was that (skipping every sun caster left it
  unchanged). Frame rate over a window is the trustworthy number; screen sharing on this Mac adds noise.
- **Metro**: clipped pixels 15.5% (last commit) -> 1.1% (auto exposure), repeatable.

### Step 9: where the forest's frame went

Method: features switched off one at a time, frame rate over 6 s windows, alternating pairs (this Mac
throttles under sustained load - identical runs drift from ~31 to ~22 fps - and screen sharing is on).

- Post effects (GTAO, SSR, Sky, Bloom): no measurable cost each. Trees: ~12 ms of a 32 ms frame. Moving
  the impostor switch to 120 m or even 20 m: no gain - the trees' cost was not their triangles.
- **The cost: trees' shadows redrawn every frame.** The sun's shadow cache keeps still casters in maps it
  reuses, but anything with wind counted as moving in every cascade (`ShadowCacheWindFrom` default -1),
  so ~2 300 trees' shadow meshes (560k triangles, cut out) were drawn every frame into the far cascades.
- **Fix: `DirectionalLight::kWindFromAuto`, the new default** - swaying casters are kept in cascades whose
  texel is >= 5 cm (NatureLab: cascades 2 and 3, 6.6 and 23 cm). Their shadows stop swaying there; a tree's
  shadow that far off is small on screen and its sway (up to wind x height, 0.7 m at a 14 m crown top) a
  pixel or two. Games that want every shadow to sway set -1. Measured: 41.2 -> 29.7 ms, 44.8 -> 34.2 ms
  (alternating pairs). Exact GPU total for the view: 27.6 -> 16.0 ms.
- **Dead trees vanished past 250 m** (bake script put "nothing" on their only part): fixed, they use their
  impostor.
- **Impostors as shadow casters (two triangles a tree, cut out by the atlas view from the sun): written
  but OFF** (`PYROS_SHADOW_IMPOSTORS=1` / editor `set_render_switch`). The quad reaches the shadow pass with
  the right numbers and shape but its cut-out fails - a solid square in the cache's kept map, nothing when
  drawn directly. Not needed for the budget after the fix above; left for later.
- `set_render_switch` editor command: flip a renderer switch at run time, to compare in one session.
- Still open: the editor frame is ~26 ms with the GPU at ~16 ms - the rest is the editor (its UI, other
  renderers, pacing) and screen sharing; forward is ~20% brighter than deferred on the real leaves; long
  shadow streaks over the sea seen from a high camera (present before these changes).
- **Forward vs deferred leaves** (fixed exposure, AO off, one session): with sun shadows off the two match
  to 1-3%; with them on forward's crown is 27% (30 m) to 68% (80 m) brighter. Forcing deferred's leaf
  shadow factor to 1 makes it look like forward: forward's leaves barely receive their crown's own shadow,
  deferred's do - deferred (the default) is the more correct one; forward's leaf shadow lookup is the open
  item. Along the way: the leaves' light-through read the sun from a raw global that held whatever unmarked
  light some renderer last named (a preview's, or none) - now through `GetSunHint`, the marked sun first;
  deferred looks a leaf's shadow up off the card's own facing (from the depth buffer) toward the sun.

### Step 10: forward and deferred agree on leaves; a crown's own shadow at distance

- **Leaves never received shadows in forward**: the leaf materials lacked `DirectionalShadow` (the wood had
  it); deferred lights every pixel in its light pass, so only forward showed it (crowns +27-68%). Now every
  lit generic material (Diffuse or PBR) receives the sun's shadow unless it says `"receiveShadows": false`.
  Forward vs deferred crown (fixed exposure, AO off): 0.181 vs 0.190 at 30 m, 0.065 vs 0.076 at 80 m.
- Foliage that casts with its drawn meshes casts at the level it is drawn at (not one chosen by cascade
  texel): a leaf otherwise sat inside another level's crown. (Measured on a single tree without a shadow
  mesh: the crown's lit share holds from 30 to 140 m, 0.15-0.19.)
- **Open - a crown darkens with distance** (deferred and forward alike): 0.19 at 30 m -> 0.08 past 65 m,
  against 0.36 at every distance with shadows off. Debug view: past ~50 m nearly every leaf's shadow lookup
  returns 0. Cause: the trees cast with a dedicated shadow mesh (detail 2, sprays x2) whose coarser, larger
  cards fill in solid in the coarse cascades and envelop the drawn crown. A detail-0 shadow mesh fixes the
  curve on a single tree but casts nothing in the forest (batched trees: its pieces stand for the batched
  level-0 meshes) - not yet understood; the prefabs keep the detail-2 mesh until it is.
- Detail-0 leaf shadow mesh in the forest, ruled out as causes of its missing shadows: renderable sharing
  (distinct key), auto-instancing (`PYROS_AUTO_INSTANCING=0`), GPU culling (`PYROS_GPU_CULL=0`), the shadow
  cache, ray-traced shadows, the cut-out's mips. One spruce alone with it casts at 30, 100 and 160 m; in the
  forest its casters are counted as drawn (8.5M cut triangles) yet nothing lands under the trees - next to
  look at: per-draw state reuse when thousands of casters share one geometry.
- **Cut-out textures with soft edges** (the spruces' `bough.png`: 84% hard alpha) failed the cut-out test
  (90%) and got plainly averaged mips - their needles faded out of far levels and coarse shadow maps. Test
  now 80%; texture cache version bumped to P3DTC006 so the mips are rebuilt.

### Step 11: terrain rock, and the haze that was exposure

- **Triplanar rock** (engine terrain template `TerrainTools.cpp` and NatureLab's copy): the rock layer is read
  from all three sides, blended by the normal to the fourth power, each with its own gradients - no seam where
  the old switch at 53 degrees flipped from one side to the other. GBuffer.Terrain 1.8 -> 2.1 ms (open view).
- **Runnels across each face**: the two "water down the rock" modulations read a texture at a fixed world axis,
  so a face running along that axis had them stretched into wide pale bands; now measured along the face
  itself, and softened (they read as painted lines at full strength).
- **The "haze" on near terrain was exposure, not air** (air dims 100 m by ~0.2%): the physical sky came out
  2-3x darker than sunlit rock (real clear skies are about as bright as albedo-0.15 rock in sun), so the eye's
  exposure, metering a dark sky, lifted the land into flat grey. Sky's sun factor recalibrated (6 -> 14: what
  single scattering leaves out - light scattered again and sent up by the ground). Cliff, open view, linear:

| | cliff mean | contrast (std/mean) | darkest 5% | sky |
|---|---|---|---|---|
| last commit | 0.113 | 0.70 | 0.044 | 0.306 |
| before this step | 0.195 | 0.41 | 0.104 | 0.086 |
| now | 0.119 | 0.49 | 0.058 | 0.172 |

### Step 12: a crown's light at every distance

- **Why crowns darkened with distance** (debug view of the shadow factor): a tree's leaf cards are smaller
  than a texel of the coarse cascades (6.6 and 23 cm) and drop out of them; the only shadow mesh that casts
  there is a coarse one (detail 2, sprays x2) whose solid cards envelop the drawn crown, so in those cascades
  every leaf sat in its own tree's shade. No one shadow mesh can be right in every cascade.
- **Fix (deferred and forward alike)**: where the cascade's texel is coarser than a leaf (2 -> 6 cm ramp), a
  leaf takes the share a crown lets through on average (0.45, as past the maps already), and still the map's
  shadow of whatever stands further off than a crown (looked up again lifted 8 m toward the sun: a hill, a
  neighbouring tree). Same for the light through the leaf. Seen: the test birch lit the same at 30, 80 and
  140 m in both renderers (was a dark mass past ~65 m); the forest keeps its trees' shadows on the ground.
- Tree meshes made for a shadow (`"forShadow": true`) are their own object in the tree cache.

### Step 13: where the editor's frame went (CPU)

With the machine quiet the forest's GPU frame is 14.5 ms (exact), Cornell 4.9, Metro 9.1 - but the editor
ran at ~36 fps. Not waiting on the GPU (PYROS_FRAME_WAIT_TRACE: steady frames under 4 ms of wait).
The profiler capped a frame at 64 scopes and cut the end off (`kMaxScopes` now 256): `Editor.DrawUI` was
20-25 ms, ~13 ms of it unscoped. `sample` (1 ms) put the main thread's time in:

- the light gizmos and viewport icons collecting every object of the scene and type-checking every
  component each frame - now `SceneGraph::CollectGameObjectsRecursive` keeps its answer until any object
  tree changes (`GameObject::TreeVersion`, bumped by add / remove / reorder / destroy of an object that was
  in a tree; scratch objects the renderer makes each frame do not), and the icons keep their short list of
  cameras and lights until the registry changes. (The 2D systems call the same collection every frame, in
  the game too.)
- the hierarchy walking the whole registry once per parent drawn (quadratic) and building every row:
  now a children index kept while nothing changes, and closed rows out of sight are only their height.

| (forest view, editor) | before | after |
|---|---|---|
| Editor.DrawUI | 20-25 ms | 8.0 ms |
| Editor.Hierarchy | 5.5 ms | 1.45 ms |

(and the light gizmos: their lights listed once while no object tree changes - adding or removing a component
now moves `TreeVersion` too - with the few rendering components whose normals are shown added each frame.)
After these, `sample` has the editor's main thread waiting a third of the frame for a free frame slot: the
editor is now bound by the GPU, not its own CPU work.

The frame total in the same run was still ~28 ms with the machine loaded (load average 6-12: another
session's window tests, media analysis) - to be measured again on a quiet machine.

### Step 14: the ambient light from the physical sky

- `PhysicalSkySH` (new, `Rendering/GI/PhysicalSky`): the Sky effect's air model ported to C++ (same constants
  and calibration) - the sky above, and below the horizon the ground's light from the sun and the sky on
  albedo 0.15 - projected into L2 spherical harmonics (768 directions of equal solid angle a hemisphere).
- `IRenderer::SetSkyAmbient`: each frame, ambient mode 2 with that SH, made again only when the sun moves a
  third of a degree or its strength or the sky's settings change. Set by `PostEffectChain::ApplySkyAmbient`
  wherever a view's chain is built (editor, player): on when the chain has a Sky effect with its physical air
  and `uSkyLight` (new Sky parameter, default 1) is not 0.
- NatureLab: the sky's average light comes out (0.23, 0.38, 0.62) linear against the scene's hand-set sky
  colour (0.18, 0.34, 0.71) - the same picture today, but the shade now follows the sun and the air (dusk,
  haze) instead of staying one colour.
- **Impostor shadows: not needed as things stand** - NatureLab's cascades end at 240 m and the impostors start
  at 250 m, so no impostor stands where a shadow is drawn. The caster path stays off (its forced-on bug -
  a square in the kept map - is recorded above) until a scene draws shadows past its impostor distance.
- **Sea shadow streaks: not a defect** - seen from ordinary viewpoints the sea has none; the streaks came from
  a camera that stood half inside the terrain.
- Measuring caveat: another session on this machine tiles windows (yabai) - the editor's viewport is resized
  (as small as 290x111) and its window covered, and macOS throttles a covered window: frame rates and
  screenshots taken while it runs are not comparable.

### Step 15: volumetric clouds by default

The Sky effect's volumetric clouds (a slab of 3D noise marched at half resolution, lit by the sun through
themselves) are now its default (`uCloudVolume` 1; the chain's half-resolution pass follows the same default;
0 keeps the flat layer). Cost measured: +3.7 ms at 2954x814 (2.4 Mpx), ~+1 ms at 536x439. Seen: cumulus
with lit tops and shaded bases behind the ridges, where the flat layer drew thin streaks.

### Step 16: the frame budget at real resolutions

The editor's viewport is small (536x439, 0.24 Mpx): there the forest is 13-14 ms of GPU with the volumetric
clouds (+0.7 ms, measured alternately in one session) - under 16 ms. At 2.4 Mpx it was ~35 ms, post effects
alone ~10 ms: at 1440p a scene like it does not fit 16 ms at the window's size. The engine already had what
games do about it (render scale, FSR 1 / a sharpening upscaler, dynamic resolution toward a frame rate), but a
game that said nothing ran at the window's size with no target.
- **Default now: dynamic resolution toward 60 fps**, the frame drawn down to half the window's size at most
  and upscaled (FSR 1), for a project with no renderScale / autoRenderScaleFps / upscaleQuality of its own;
  `"upscaleQuality": "native"` keeps the old behaviour. Not yet measured in a running game this session (the
  player cannot be driven by the agent; the editor's viewport does not use it).

### Step 17: the sky's cost

Measured at one viewport (1112x671, 0.75 Mpx), post passes now named after their effects in the profiler
(they were "Pass0".."Pass11", every one past the twelfth lumped as "Pass11"); `set_window_size` editor
command added (a window manager may still override it - measurements are taken only at a checked size).
- The sky's ground pixels each marched the horizon's haze colour (8 steps) for nothing - it is used only by
  the clouds and the rain's veil: made only where it is wanted.
- The sky's light now made at half resolution, with the clouds, by the pass that marches them (for every
  texel, so a pixel of sky beside a ridge reads sky whichever side the texel fell); full resolution samples it.
- The sun's light through the air in closed form (the column above times Kasten & Young's air mass) instead of
  a 4-step march at every step of every ray; PhysicalSky.cpp follows (its twin).
- Exposure meter 64x36 -> 32x18 (the adapting pass reads every cell in one pixel: 0.49 -> 0.14-0.17 ms).

| Sky effect, 0.75 Mpx | before | after |
|---|---|---|
| full-resolution pass | 2.39 ms | 0.85 ms |
| half-resolution pass (clouds, now the sky too) | 2.18 ms | 2.49 ms |
| total | 4.57 ms | 3.34 ms |

The picture: sky mean colour within one 8-bit level, ridges clean (compared in one capture).
- Terrain (engine template and NatureLab's): the rock's six triplanar reads skipped where rock has no weight
  (explicit gradients, taken outside the branch). Saving not yet measured - the window manager resized the
  viewport during the run.

### Step 18: the packaged game at 1080p (measured)

NatureLab packaged (scratch copy; the scene files the packaging re-saved were restored from a backup; the
game's saved quality setting changed only for the run and restored), 1920x1080 window (not high-DPI on macOS:
2.07 Mpx drawn), default spawn view, 35-45 s runs, PYROS_PROFILE_LOG:

| preset (Nature.lua's own) | frame (paced) | frame (uncapped) | GPU passes, exact sum |
|---|---|---|---|
| full (scale 1.0, 4 cascades to 240 m) | ~40 ms median (script: 18-19 fps) | 35.5 ms | ~21 ms |
| mac (scale 0.6, 2 cascades to 70 m, LOD 0.42) | 29.4 ms (34 fps) | 42.3 ms | 11.6 ms |

- The script turns the engine's dynamic resolution off and applies its own preset - the new engine default
  (step 16) applies only to games that do not choose; not exercised here.
- The frame waits 16-29 ms a frame for the GPU (`Player.BeginFrame`), yet the passes the GPU runs add up to
  12-21 ms, and the game's own command buffers are reported busy 48-55 ms a frame: their GPU time includes
  time the GPU spent on other processes. Screen sharing was running throughout (screensharingd ~49% CPU,
  WindowServer ~27%: the whole screen captured and encoded continuously), as was another session's window
  tiling. Wall-clock frame times on this machine are therefore not the engine's alone; the exact pass sums are
  the engine's own cost: within the 16 ms budget on the "mac" preset at 1080p, ~1.3x over on "full".
- To measure the wall clock honestly: the same runs with screen sharing off.
- Where the "full" preset's geometry goes (PYROS_TRIS_DUMP, packaged game, default view): 5.25 M triangles,
  395 draws a frame - ~2.5 M of it trees within 70 m at their full level (GPU-culled batches, ~10 k
  triangles a tree), 295 k the sea plane (one mesh, every frame), 282 k terrain, 450 k the trees' far
  levels. These are the project's budgets (Nature.lua's presets, the prefabs' LOD distances, the sea mesh), not
  an engine defect: the passes' costs per triangle and per pixel are in the range expected on an M3.
- Tried the trees' first level of detail at 40 m instead of 70 m by editing the packaged copy's prefabs: the
  triangle dump came out identical (5254 k vs 5255 k) - a packaged game does not read its prefab files at run
  time, so that A/B compared the same content, and its 4-5 ms difference (42.3 vs 47.8, 34.1 vs 38.3 ms) is
  run-to-run noise. Lesson: single runs of this game vary by ~5 ms; a change needs alternating repeats. To test
  a prefab change the game must be packaged again with it.
- Packaged again with the trees' first level at 40 m (project restored right after): 5233 k -> 4550 k triangles,
  390 -> 369 draws, frame 39.0 -> 38.1 ms in the one clean pair (later pairs were disrupted: load average 7.6).
  Triangles are not what bounds this frame; the change is not kept (the project's prefabs are as authored).

### Step 19: contact-hardening sun shadows - tried, removed (an earlier claim here was wrong)

Tried: the sun's penumbra from the caster-receiver gap (five taps, each asked again 1, 4 and 16 m toward the
sun; width = gap x 0.0093, capped by the light's softness), in both renderers. The first before/after showed the
birch's shadow keeping its crown's structure - but its two frames were at different viewport sizes (yabai's
tiling resizes the editor between launches), and the viewport decides which cascade the ground falls in: in a
coarse one the leaf cards merge into a blob, whatever the filter. At one forced size (1600x1000, two alternating
repeats) on and off differ by a mean of 2.6 levels - less than two identical runs differ (4.2: the wind). No
visible effect, extra taps: removed. Also tried and removed: the shadow map sampled with hardware linear
comparison (one fetch per disc tap instead of four) - no measurable change in the lighting pass.

Lesson for every comparison here: force the window size (`set_window_size`) before a shot, or the cascades move.

- **Dynamic resolution, exercised**: the packaged copy's Nature.lua with its two lines that switch it off removed
  (the copy only; the project's script is untouched): the engine took the scale from 100% to its 50% floor
  within the first seconds - the default works. At half resolution the frame was still 26-47 ms, with the
  machine's load average at 7.3 and several VMs running (VMPal): the frame was not bound by its pixels, and
  wall-clock numbers taken while other workloads share the GPU are not the engine's alone.
- With yabai paused (the user's go-ahead; restarted after) - the tiler's scripts ran on every new window and
  resized it during runs - the packaged game, uncapped, alternating, two repeats each:

| preset | GPU cull on | GPU cull off |
|---|---|---|
| full | 54.9, 64.4 ms | 53.1, 62.3 ms |
| mac  | 42.9, 39.9 ms | 38.1, 37.0 ms |

  GPU culling off was faster in all four pairs (1.8-4.8 ms): in this scene the cull's compute costs more than it
  hides. The totals themselves are worse than earlier runs (35 / 29 ms) - the load average was 14 when the runs
  began - so the wall clock is still not the engine's alone; the exact pass sums (21 ms full, 11.6 ms mac) are.
  Not changed: GPU culling's default (the evidence is one scene); worth a look at what its dispatches cost.

### Step 20: rock contrast against photographs - cliff-scale relief

Reference: two sunlit rock faces from Wikimedia Commons (Bunnell Point granite dome; Stålstuten cliff), rock-only
crops, sRGB decoded to linear luminance: contrast (std/mean) 0.53-0.69, darkest 5% at 0.04-0.31 of the mean.
NatureLab, same measure, open cliff view (viewport 1038x617 in both frames), before: near face 0.25 / 0.65,
upper mountain 0.24 / 0.58 - flat, and with no dark at all. Why: the rock picture has no height (its alpha is its
brightness, correlation 1.00), and the only relief was 2.8 cm high and gone by 70 m - a cliff was lit as one sheet.

Now (NatureLab's nature_ground.glsl and the engine's natural-ground template, TerrainTools.cpp): relief at the
size of a cliff on steep snow-free rock - gradient noise with exact slopes in the face's own two directions, ribs
~11 m across and 8 m deep, flutes ~3 m and 2 m, ~1 m and 0.2 m; the finer two skipped where under two pixels.
After: near face 0.34 / 0.48, upper mountain 0.27 / 0.54 - closer, still short of the photographs (what is left
is cast shadow from real overhangs, which a heightfield cannot have). Terrain pass, same viewport, alternating:
2.69, 2.62 ms before; 2.62, 2.70 ms after - no measurable cost.

Tried and dropped on the way: slopes from the 8-bit macro map (terraced: 45% of neighbouring texels equal);
ridged noise (its creases follow the noise's wandering zero line - marble veins); painted joint lines (+0.3-0.5
ms, contrast +0.02). The text-shader codegen gained an optional `functions` argument (file scope, ahead of
main) for the template's noise helpers; a terrain generated from the template compiles and renders.

### Step 21: NatureLab's terrain shadow baked

The 183k-triangle terrain was drawn into the sun's shadow cascades every frame. NatureLab's Nature.json now
has `"terrainShadows": {"baked": true, "resolution": 1024, "reach": 1200}` (one line added by hand: the editor's
own save also expanded its materials 2 -> 77 and wrote out every default - not kept). Packaged game, exact
times, two alternating repeats: cascade 1 4.3 -> 3.1 ms (cascade 0 unchanged at ~5 ms: not the terrain).
Picture, play mode, same game camera: live vs baked differ by 14.9 / 13.8 levels (mountains / forest), two
live plays by 16.1 / 13.8 - the bake is within play-to-play variation. Backup: scratchpad Nature.before_bake.json.

### Step 22: what cascade 0 costs (open; measurement pending a quiet machine)

Cascade 0, packaged game: 46 casters, 24k triangles, ~5 ms; cascade 1: 168 casters, 140k triangles, ~3 ms -
the cost is per texel of the map, not per triangle: cascade 0's texels are the finest, every leaf card covers
many of them, and cut-out leaves are shaded layer by layer. Not the kept map's copy (cascades 1-3 copy theirs
too and cost 0.3-0.4 ms). Prepared, off by default: `PYROS_SHADOW_SORT=1` draws the sun's casters nearest the
sun first, so a fragment behind leaves already drawn is rejected before shading. To measure (exact times,
alternating, `PYROS_SHADOW_KINDS=1` for the alpha/solid split) once the machine is quiet; kept only if it pays.

### Step 23: the frame, measured properly (MacBook Air M3, fanless)

Earlier wall-clock numbers mixed three things this machine does: yabai tiles the game window (1952x2073,
about twice 1080p, once its queue is moving), a fanless M3 slows itself under sustained load (the same package
ran 30-40 ms in the morning and ~60 ms after hours of benchmarks), and another session's window hung yabai's
queue. Measured now with the window held at 1920x1080 (a temporary yabai rule, removed after), 20 minutes'
cool-down first, the packaged game with today's engine, terrain shader and baked terrain shadow, uncapped:

| preset | cool, 20 s | sustained, 5 min (per 30 s) |
|---|---|---|
| full (100% scale) | 48.3 ms, 21 fps | 45, 38, 40, 47, 47, 46, 55, 57, 58, 52 ms - settles ~52-57 ms, 18 fps |
| mac (60% scale)   | 23.4 ms, 43 fps | 24, 24, 24, 26, 26, 31, 36, 35, 30, 29 ms - settles ~30-35 ms, 29-33 fps |

Same window, cool, "mac": morning package 43.0 ms, today's 38.4 ms - today's changes cost nothing.
Not at 16 ms on this machine: "mac" needs about half its GPU work gone, "full" about two thirds.
The figure a player of this Air sees is the sustained one.
- Shadow sort, exact times, alternating with cool-downs: cascade 0 came out either ~4.4 ms or ~1.2 ms whatever
  the sort - first read as a hidden state worth 3 ms; it is the window (yabai's rule had been removed): the
  ~1.2 ms runs have every screen pass ~1.8x dearer (lighting 9.4 vs 5.2 ms, sky 4.8 vs 2.6), the window tiled
  tall, and the camera's different frustum puts a sparser patch of forest in cascade 0. At 1080p cascade 0 is
  ~4.4 ms; the sort had no fair pair there. PYROS_SHADOW_SORT stays off, untested at 1080p.

### Step 24: the picture at full size - two retractions

Looked at at 2312x1371 (the earlier crops were ~1000 px wide), step 20's cliff relief read as marbled wood
grain on the near face, not rock: its contrast number improved and the picture got worse. Reverted everywhere
(NatureLab's nature_ground.glsl from its backup, the template in TerrainTools.cpp, and the codegen's
`functions` argument, which nothing else used). Also retracted: the Cornell box's mirror spheres are dark in
the middle because they reflect the box's open front and the scene's near-black background (0.02) - correct,
not a bug.

### Step 25: sharpness, and the editor's load time

- **Sharpness.** NatureLab's FXAA blurs everywhere contrast is high - foliage, grass, rock: fine detail
  (mean Laplacian of a palm/mountain crop, same view and size) 30.3 with no anti-aliasing, 17.2 with FXAA,
  11.0 with TAA. The anti-aliasing stage now ends with contrast-adaptive sharpening (CAS, the existing
  SharpUpscaleEffect at 1:1) after FXAA (0.6), SMAA (0.35) or TAA (0.8); `PYROS_SHARPEN=0..1` sets it, 0 off.
  SMAA + sharpening: 27.6, edges still smooth. (NatureLab's project still says "fxaa"; SMAA is the better
  pick - the user's call.)
- **Editor load.** Opening NatureLab took 25.7 s and loading its scene 27.6 s (7354 objects; Erangel 1.4 s).
  Profiled: SceneObjects::SetName gathered every other object's name each time it named one, and then
  counted a shared name's suffixes up from (1) - quadratic twice over. Now a count of names in use
  (nameUse, kept at every insert and removal) and the next suffix to try per name (nextSuffix).
  NatureLab: open 2.2 s, load 3.2 s; Erangel 0.6 / 0.9 s; names still unique (0 duplicates of 7354).
  Smoke test passes. Also: the texture cache's version bump (P3DTC006) makes each project's first load
  after it recompress its textures (Erangel's first load 6.2 s, then 1.5 s).
- **Demos.** All 44 in the launcher start and keep running, no crash, no engine error (only macOS's
  input-method "mach port" message on a killed window). Their pictures were not checked one by one.

### Step 26: the demos - what the picture work broke, and the first fix

tools/VisualCheck against tests/visual/metal (baselines of 2026-10-23): 42 of 44 demos differed.
- **Over-exposure (fixed, not yet re-verified by a full run).** Every frame went through the new output
  transform with the eye's exposure on; a demo of lit shapes on black (the manifest's "tonemap" ones)
  was metered as a dark scene and lifted to white - the parallax bricks clipped to magenta and white.
  PostEffectsManager::outputChosen: until a scene's chain has the physical Sky or an "Output" entry
  (or an "Output" parameter is set), the frame is put out as before the transform existed - the
  old TonemapEffect's filmic curve at its fixed exposure, or a plain encode; uAuto off. From the frames
  re-captured before the run was stopped: 01, 02, 03, 06, 07 back to their baselines.
- **Darker shading, deeper colours (open, the user's decision).** ColourSpace.h decodes picked colours
  (material, light, ambient, background) and colour pictures from sRGB before lighting; the demos were
  tuned with them taken as they are. Shaded sides darker, salmon -> red, the volumetric lights nearly
  gone; possibly also the point and spot lights missing in 30-33 (not checked without opening windows).
- 00 and 19 came out blank: their capture never resized the window ("[check] resized" missing from the
  log) - the harness, not the demo, as far as the log says; to recheck.
- The verification run was stopped: it opens 44 windows on the user's screen. Ask before running it.

### Step 27: the demos, physical everywhere - and the post-effects pipeline

- **Every demo's frame now goes through the output transform** (render_host.lua builds the
  PostEffectsManager with no effects too). A demo with an empty chain used to draw straight to the
  screen: lit in linear light, its colour textures decoded from sRGB and never encoded back - the
  island at half its brightness. The workaround of the step before (picked colours re-encoded,
  `Linear()` in scripts) is undone: every demo has its original colours again. The point and spot
  intensities stay rescaled for the inverse-square falloff (`LinearFalloffIntensity`, distances to
  each scene's floor).
- Cubemaps (skybox, environment, refraction) decoded like every colour picture: Skybox 19.4 -> 5.5.
- Forward SSAO (SSAOEffect.cpp) is the deferred one's GTAO now; identical on Metal and Vulkan.
- Volumetric clouds: the half-resolution sky read through a 3x3 tent - the per-pixel jitter of the
  march stood along every cloud edge as streaks, with no TAA to average it.
- **TAA: tried, removed, and what really limits it.** A 3x3 Blackman-Harris rebuild of the current frame
  about this frame's jitter came out softer with every sign (NatureLab's cliff 11.0-12.1 against 13.7
  for the old resolve, 14.5 with no AA, 20.6 SMAA + sharpening). The old resolve keeps still surfaces
  about as sharp as no AA. What it smears is the foliage: the velocity pass draws only objects that
  moved, so a tree swaying in the wind (VERTEXWIND) has the camera's motion only, and its history is
  clipped away - smudged, darkened fronds. (The velocity code also places instanced meshes by
  uModelMatrix, not the instance's ModelMatrix - harmless while instances are never drawn there.) The fix
  is motion written by the geometry pass itself, wind included - a G-buffer change on every backend; open.
  Until then SMAA + sharpening is the sharper choice, and TAA is not made the default.
  (The sharpness score counts no-AA's sparkle as detail: still, textured regions only.)
- **Physical bloom** (BloomEffect.cpp): a threshold below zero - the default for a Bloom added from now
  on - takes all the light, Karis-averages the first step only, six levels down to 1/64 for the wide
  veil, and mixes 4% x intensity in (no light added). Scenes' thresholds were in scene light before
  exposure: the whole sky at one exposure, nothing at the next. The projects' saved Bloom settings
  keep the threshold bloom until changed. NatureLab's beach at midday, both blooms: the same frame within
  dither - the physical one lifts the darkest 5% of the tree line from 7 to 9 and takes 219 -> 217 off the
  brightest; daylight is where it should be subtle. The sun in view, lamps at night are where they part.

### Step 28: three faults the before/after sheets showed

- **Deferred ambient rounded away in the demos.** The G-buffer carries albedo x ambient in the alphas of
  the albedo, specular and normal targets; the demo host made the first two 8-bit. In linear light a dim
  ambient is a fraction of one step: red and green rounded to zero and Volumetric Lights' grey floor came
  out navy (2,0,46), only the normal target's float alpha keeping the blue. render_host.lua now makes them
  RGBA16F, as PyrosPlayer always has (games were not affected). Volumetric Lights 13.3 -> 3.9.
- **The physical sky's ground was lit fourteen times too dimly.** Its sunlight was taken as uSunTo.w x pi;
  the air is lit by airScatter's sunE (pi x 14 x uAirSunPower). A Lambert ground of albedo 0.15 now gets
  albedo / pi of that: sunlit earth under the clouds, not near black. Volumetric Clouds 72 -> 55.
- **GI ignored the lights' intensity.** SceneGI took each light's colour alone; the bounce never followed
  the intensity (Cornell Box: the same frame with the lamp at 13.5, 0.6 or off). It now takes the light's
  radiance (decoded colour x intensity; a sun's tint by the hour), and both tracers (DDGIVolume.cpp,
  ddgi_update.glsl) use the direct light's p3d_Falloff instead of (1 - d/R)^2. Spot lights reach the GI
  with their radius (they had none: a bounce as strong at any distance). Cornell lamp 3.0: 9.5 -> 6.9;
  its contact shadows are fainter than before - the inverse-square bounce fills them.

### Step 29: where NatureLab's frame goes (editor and packaged game, 1920x1080)

- Editor, play, one session: everything 48.5 ms; no sun shadows 28.5 (shadows ~20 ms); no trees 26.1 (~22 ms);
  MetalFX at 0.67 / 0.59 / 0.5 of the pixels 39.3 / 37.9 / 38.8 - the frame is the forest's geometry (G-buffer
  and four cascades), not its pixels. (Abl_NoFoliage and Abl_NoSea are stale: slower than the full scene.)
- The scene's quality switches (setLODScale, setFoliageQuality, setRenderScale) exist only in PyrosPlayer;
  in the editor Nature.lua skips them silently - an editor run of nature_test.txt measures cascades and the
  sea alone, the rest drift. Copies of Nature.json without Nature.lua run without the scene's set-up.
- Shadow impostors do not apply here: the cascades end at 240 m, the impostors begin at 250 m.
- Packaged game, today's engine, SMAA, "full", alternating PYROS_SHADOW_SORT off/on, 25 s each: 39.6 / 38.8,
  42.8 / 53.5, 80.3 / 58.5 ms - the first pair (cool) is within noise; within three minutes the fanless Air
  doubles every frame time. The sort is not worth turning on. Cool, "full" is 39.6 ms today (48.3 on step 23).

### Step 30: demos tuned in gamma space, given their exposure

The demos' lights were tuned when the engine shaded in gamma space; lit in linear light and encoded at the
end, the same numbers come out brighter in the midtones (Shadow Matrix +45%, Two Point Shadows +38%, Two
Directional Shadows +26%, Metro +27%, PBR Spheres +21%; Neon Pulse -13%), measured on the displayed picture of
what is lit. Each of these has a fixed exposure in the manifest - an "Output" entry with the eye off and its
old curve kept (the old tonemap's for a 'tonemap' demo, a plain clip otherwise), the same on every backend:
PBR Spheres -0.61, SSAO (Deferred) -0.3, Motion Blur -0.19, Neon Pulse +0.43, Metro -0.5, Shadow Matrix -1.07 /
-1.06, Two Point Shadows -1.0, Two Directional Shadows -0.73 stops. PBR Spheres and the four Two Point / Two
Directional demos match their references again; the rest match in brightness, their shading falling off more
softly than in gamma space. Metal and Vulkan frames: 40 of 42 within 1/255 on average (Particles (GPU) 1.4,
Ray-Traced Reflections 1.3).

### Step 31: a surface that mirrors the sky itself, the frame's own ambient, the eye's metering, a boat in the sea

- **Reflections that replace the sky, not lie over it** (lastPass.glsl, DeferredRenderer.cpp). NatureLab's sea
  puts the sky it mirrors into the G-buffer's ambient (so it shows where the march finds nothing); the
  screen-space pass then blended what it found at the Fresnel share over a colour that already held the sky at
  that share - the hull came out as a faint tint on bright water (the share actually laid on was ~0.25). A
  surface now marks itself (FragData_pbr.b = 129/255, SKY_MIRROR_TAG; its alpha the share it mirrored); where
  the march finds the scene, the pass takes that sky out (SkySeen: the ambient as the materials have it, its
  horizon band put back, as Ocean.glsl reckons it) and puts the scene in. Never more taken out than the
  surface's colour holds, and no pixel more than 0.6 mirror (a wave's back seen edge-on had Fresnel near one and
  stood out as a dark speck). PYROS_SSR_DEBUG=7 shows the marked surfaces, 9 / 10 what is kept and the sky
  taken out, 11 the lit frame twenty times brighter.
- **The ambient a frame drew with** (IRenderer::PreRender, drawnAmbientMode / drawnAmbientSH): the editor sets
  its ambient mode again mid-frame (ApplyEnvironment), after the sky's harmonics had lit the G-buffer, so a
  pass after it read the gradient instead. The last pass reads what the materials were lit with.
- **Metering** (AutoExposureEffect.cpp): the mean between the 20th and 90th percentiles of the centre-weighted
  log luminance, not the 50th and 95th. A view half sky metered the sky alone; everything under it went to the
  ACES toe and came out (0,0,0) - a backlit ship, its sails and the palms were silhouettes cut from black paper
  (their light was there: ~5% of the sunlit, as a shade under a clear sky is). Scenes with the eye on meter
  lower than before; their references change.
- **NatureLab's boat** (Ocean.glsl p3d_wake / p3d_wakeV, one function in both stages): the water heaps up at the
  bow (0.55 m at the pinnace's Froude number), sinks along the sides amidships, rises behind the transom, and
  makes Kelvin's wake (transverse waves k = g/U^2, diverging crests along the arms, their slopes kept under a
  breaking wave's); foam as coverage - the bow crest breaking and peeling back along the hull, a rim at the
  waterline (along the hull only), the churned trail spreading and feathered, the arms' crests - and a calm lane
  for nine lengths behind (the small waves flattened, the water glassier), milky turquoise water under the white.
- **Foam as a web** (p3d_foamPattern / p3d_foamTex): warped ridged noise at three sizes, fine ones faded under a
  pixel; coverage opens holes in it. The shore's cracked plates (Voronoi cells - crazy paving up close), the
  crests' and the surf's foam and the boat's all draw it.
- The editor's script store is `~/Library/Application Support/Pyros3D/Editor/`; Nature.lua reads nature_cam.txt
  ("x height z yaw pitch", or "boat ahead side height pitch" riding with the boat) for a test's camera.
- **The eye does not make up all of a dim view** (OutputTransformEffect.cpp): below a sunlit day's metered
  luminance (about 2^-2.5 here) half a stop is kept for each stop the view falls, up to 2.5. NatureLab's lower
  half measured 0.150 at 11 h and 0.043 at 17.9 h (PYROS_FRAME_LUMA) - made up in full, the evening came out as
  bright as noon, bleached blue-white; now it is a stop darker than noon, and a night is 2.5 stops darker.
  Noon is unchanged.
- **Bow spray** (nature_boat.lua): two particle emitters at the bow's shoulders, moved with it each frame, throwing
  a white mist out and up from the bow crest (world-space particles, soft against the water, gravity bringing
  them back), in gusts as the bow meets the waves and with its speed. The puff is the examples' smoke.png.
- **Lit particles** (particleSystem.glsl, ParticleSystem::SetLit, Lua `ps:setLit(true[, g])`): off by default -
  every demo's particles draw as before. On, each sprite takes the scene's sun with Henyey-Greenstein's phase
  (g 0.6: droplets scatter onward) and the sky's light: backlit spray glows white against the hull, where unlit
  it was a constant grey whatever the light. The boat's spray uses it.
- **TAA's reactive mask** (TAAResolveEffect::SetReactiveSource / UseReactive; the deferred renderer hands it its
  material target each frame): a pixel whose surface mirrors the sky itself - water, marked 129/255 - clips its
  history at 0.75 sigma instead of 1.25 and takes at least 40% of the new frame. The velocity pass cannot follow
  waves, and under TAA the sea had been smeared into a grey haze. NatureLab, Laplacian sharpness (cliff / trees /
  sand): SMAA + CAS 41.7 / 49.7 / 39.6, TAA before 26.8 / 28.6 / 18.0, TAA now 34.5 / 36.9 / 25.2 - the water's
  waves come back and SMAA's speckle on it goes, but still surfaces stay softer than SMAA's; SMAA stays the default.
- **Ashvale's sea, lake and rivers** regenerated from today's Ocean.glsl by projects/Ashvale/tools/build_water.py
  (shader() only; the meshes and pictures untouched). The generator now also maps the depth picture's metres a
  pixel into the gradients (the shore's distance), the camera-height test, and gives the lake a fifth of the wind
  sea and the rivers a twentieth, with no shore swell. The rivers render with it (aerial view); a camera on the
  sea's coast through ashvale_bench.txt "view" first showed sand: the hook's y is absolute and its angles degrees,
  and the camera given y 4 and radians stood 26 m under the sea, looking at its bed. Asked properly (y 36) the sea
  renders with the new water - swell, clear shallows, the coast. It also shows the Coast Ruin (2550, 3150) standing
  in some 7 m of sea: placed on ground at 23 m, under the 30 m sea. Moved to (2550, 3040), ground ~40 m
  (tools/world.py), the world rebuilt (build_all.sh, 4 min) and split again (split_world.py, 124 cells): it stands on
  the shore above the beach. The world before is kept in the session scratchpad (ashvale_backup). The old shaders are kept beside the scratch files.

### Step 32: clarity, as Duarte means it

- **Erangel rendered at 42-59% and stretched.** Its project.json asked for an automatic render scale (60 fps target,
  down to 0.42) with the spatial "sharp" upscaler - in the editor as in the game: every frame an oil-painting
  smear, whatever the anti-aliasing. Set to native (autoRenderScaleFps 0, renderScale 1, upscaleQuality native).
  The packaged game will no longer hold 60 fps on the MacBook Air; his rule is fidelity first.
- **Terrain and rock shaders read their pictures as light.** Projects generated before the engine's terrain template
  decoded sRGB kept the old shader: Erangel's splat_terrain / cave_rock, rock / rock_desert, NatureLab's
  nature_ground, Ashvale's ground and distant, Miramar's and ErangelCarTest's splat and cave. A mid-green texel of
  0.45 lit as 0.45 of the light came out at 0.70 on the screen: the ground pale and chalky, the decoded grass
  blades on it dark blots. Each now decodes its layers (p3d_DecodeSRGB / p3d_TerrainToLinear). Backups in the
  session scratchpad (decode_backup/).
- **Erangel's windows**: assets/materials/Glass.glsl (Fresnel sky and sun mirror, dark room behind) replaces the flat
  unlit tint openings.lua put in every pane, and every window gets joinery - frame, mullion, transom, sill, lintel -
  which stays when the glass breaks.
- **Erangel's house walls**: tools/house_facade.py finds a model's outside wall triangles by ray tests (cornices and
  bands left as they are) and writes them 1.5 cm proud as brick (ground storey) and plaster meshes with metric UVs and
  Poly Haven CC0 scans (red_brick_03, plastered_wall_04, brightened in light); the three house prefabs carry them.

### Step 33: ShrinePath, and two engine faults it found

- **projects/ShrinePath**: a showcase after the reference Duarte sent - a flagstone path through a close cedar wood
  (320 trees from TreeMesh with the cedar bark scan), a torii, stairs between stone walls up to a platform and a
  small shrine, stone lanterns with warm lights, ferns, nettles, grass, mossy rocks, roots and stumps (Poly Haven
  CC0), a low sun through haze. Ground and stonework from tools/build_shrine.py (glTF -> AssimpImporter -> .p3dm,
  each mesh about its own middle and placed by parts.lua); the scene script grows the rest. The camera walks the
  path; shrine_cam.txt holds a view.
- **Deferred crash on a scene whose sun casts nothing**: DeferredRenderer read the sun's cascade matrices whether or
  not the shadow pass had made them (empty vector -> SIGSEGV loading a fresh scene). Guarded.
- **AssimpImporter's inside-out guess** (FixFacing) flips a submesh's normals when most point toward its box's
  middle, and then its triangles to match. A flat or open mesh - a floor tile, a shed - fails it: the floor's
  normals were turned to the ground and the floor culled from above (every draw made, no pixel). Hand-made meshes
  are converted with PYROS_IMPORT_AS_AUTHORED=1 (the opt-out the foliage cards already use). And the guess now
  asks only a closed mesh (FixFacing: corners welded by place, an edge only one triangle has counts as open; more
  than one in a hundred and the normals are kept as written) - ShrinePath's floor and shrine convert unflipped
  without the opt-out.
- **ShrinePath graded to the reference's key**: an Output entry (eye on, -0.7 stop from the default, contrast 1.1,
  saturation 0.92). Mean luminance 0.14 / 0.18 on the two test views, against 0.16-0.21 on the reference's frames
  (it was ~0.5): a forest's low key, the sky and the sunlit floor the only bright things.
- **ShrinePath / Bamboo.json** (tools/build_bamboo.py): the reference's set, its way - a round plaza of concentric
  granite slab rings and a paved way through, walls of bamboo (520 clumps of four made meshes, ~7,000 noded tapering
  stalks with leaf sprays), ferns and grass at the edges only, a grey sky, the sun a tenth of its weight, and DDGI
  (768 probes, GPU, 33 ms solve) for the light coming down between the stalks. All in the scene file - no script
  builds it - so the solve sees everything. Mean luminance 0.23 against the reference's 0.16-0.21.
  (Rings wound clockwise were culled: every quad counter-clockwise from above.)
- Bamboo grove, second pass: a drawn cane (pale green-yellow, fibres, node bands) and drawn bamboo leaf sprays in place
  of a veneer scan and bush leaves; light cooled and lowered (GI sky 1.7-2.15, sun 0.22, exposure -0.25, contrast
  1.12): mean 0.17 against the reference's 0.16-0.21; twelve wooden lanterns with warm lights along the way; 1,400
  fallen leaves lying on the stone (single drawn leaves, bamboo_fallen.png); haze 0.008 a metre.
- Bamboo grove, final pass this session: the stalks' faces were wound inside out (drawn from within: black with
  green rims, orange under the lanterns) - fixed, and the cane picture's compensating brightening undone; per-clump
  cane tints; a figure (Erangel's player, its idle clip); the Sky's physical air at its thickest (uHaze applies only
  in rain with the physical air on); and a low VolumetricLight mist (0.02/m, thinning 0.25/m, forward glow 0.7) with
  the grey sun behind the way - the far end glows and shafts stand between the stalks, as in the reference.

## 34. An FFT sea (after Ultra Water)

- `OceanSim` (Rendering/Water): Tessendorf FFT, four cascades (600, 85, 12, 2.1 m) at 128x128, JONSWAP wind sea plus swell,
  cos-2s spread, gravity-capillary dispersion for the smallest, time looped at 300 s. Design from Ultra Water (MIT,
  THIRD_PARTY_NOTICES.md); done on the CPU, a cascade a job, a frame ahead: 2.3 ms of worker time, 0.17 ms on the
  calling thread (at 256x256 it was 14.6 ms of worker time and slowed the renderer's own parallel loops).
- Energy: the reference draws sqrt(2 E dk^2) per wave, eight times the spectrum's variance (heights 2.8x). Normalised:
  summed mean square slope 0.033 at 7 m/s against Cox-Munk's 0.039.
- Lua: `OceanSim.new()`, `:update(dt)`, `:bind(customMaterial)` (uOceanD0..3, uOceanS0..3), `:setWind`, `:setSwell`,
  `:heightAt(x, z, w0, w1, w2)` (choppy offset inverted), `:getStats(c)`, `:getLastWorkMs()`.
- NatureLab: Ocean.glsl reads it when uGlobals[12].x is set - vertices displaced band-limited to the grid spacing,
  slopes and slope variance from all four cascades, roughness = variance + Cox-Munk remainder + geometric specular AA,
  Lagarde Fresnel, the full wave normal into the G-buffer (it was flattened to half), Poisson sun glitter as the
  difference from the renderer's smooth lobe (uGlobals[9]: the sun's light). The boat floats on `heightAt`.
  `nature_fft.txt` "0" brings back the Gerstner sea.
- Second pass: the sheet is Ultra Water's camera-centred polar grid (tools/build_seagrid.py: 384 spokes, square
  cells to 3 km, a skirt to 60 km; uGlobals[12].w) instead of the remapped plane, whose cells were three times too
  big and drew the crests as facets; the transform vectorised (real and imaginary apart, butterflies along whole
  rows, rows done as columns of the transpose) so 256x256 is back at the CPU cost 128x128 had; the 12 m cascade
  shades the pixels but no longer lifts the sheet (it creased it like foil); whitecaps from the summed Jacobian,
  read a mip softer and held by the simulation's turbulence, drawn with Ultra Water's baked lace
  (textures/foam_lace.png - R lace, G billow, BA its gradient) eroding with age. Bay sea 7.5 m/s, choppiness 0.85.
  Frame time with it on and off, interleaved on a warm Air: 46.6 / 43.1 ms on, 48.9 / 54.3 ms off - no cost
  that shows.
- A note on the 'open' test view: at x = 520 it sat 8 m outside the sea's depth picture, where the old shore swell
  reads a bogus distance to shore from the clamped edge and crumples the water - moved to (700, 560).
- Shore breakers (Ultra Water's shoreMap.js and shoreWaves): tools/build_shore.py + shore_bake.cpp solve the travel time
  of a shallow-water front from the coastline over the sea's depth (fast sweeping), its direction, the ray tube's
  shoaling and the coast's signed distance, into shore_travel.png (tau in two bytes, direction) and shore_shoal.png;
  Ocean.glsl's p3d_shoreWaves runs the two trains on it (Green's law, break at 0.42 depth, peeling, plunging lean,
  lip glow) in both stages, the whitewater on a flow map at the front's speed and the surf's lace drifting in. It
  replaces the Gerstner shore swell where the FFT sea is on. Sets 1 m (uGlobals[7]; nature_surf.txt), a lee shore
  keeps half the swell: this beach is steep, its surf zone a few metres wide.
- Engine: a scene sampler with "data": true loads as written - its own texture, never block-compressed (Texture::
  IsDataMap, saved back by the serializer). Every custom material's sampler had been BC1/BC3-compressed with the
  colour maps: sea_depth.png's depth was read in 4x4 blocks all along (the "steps of the depth picture's squares"
  the old shader's comments work round), and the shore maps' two-byte travel time came back as a staircase.
- Not yet ported: planar reflection, the GPU FFT.

## 35. Bamboo: the far ferns and stalks, and TAA on a still picture

- Measured properly: the camera held still, every newly rendered frame against the one before (high-passed): TAA
  0.68 against SMAA's 0.41 - the resolve added shimmer of its own. (The earlier "static" test took screenshots 0.8 s
  apart and counted everything that changed in between.)
- The far ferns were black with magenta, blue and green specks, in either AA: build_bamboo.py gave every level of
  detail a scene material, and a level given one is drawn with it instead of the model's own maps
  (SceneSerializer: "ownMaterials": true keeps them). Material 1 has no picture: the ferns past 10 m and the stalks
  past 18 m (material 0: flat, no cane) were drawn with nothing bound. The builder and the scene now say ownMaterials.
- TAA resolve: the current frame reconstructed at the pixel's own centre - its nine texels in a Blackman-Harris
  window about where the unjittered centre fell (uParams.zw, the jitter in pixels) - instead of read at the jittered
  point; the variance clip widened to 2.25 sigma where nothing moves under the pixel (1.25 moving, 0.75 on water);
  the new frame's share 0.04 standing still (was 0.08).
- Still camera now: TAA 0.45 (sky line 0.53, ground 0.36) against SMAA 0.39 (0.41, 0.37). What is left is the
  alpha-tested leaf cards against the sky. Walk: about 52 ms a frame on a warm Air (it was 46 before the far ferns
  were textured).
- Tried and taken out: a dithered alpha test under TAA (the cut-out's edge coverage over the pixel where its alpha
  crosses the cutoff, drawn under a screen dither). With the jitter it was noise, not an average: still camera
  0.51 against 0.45 without it (sky line 0.62 against 0.53). The leaf cards' edges stay a hard cut.

## 36. The island in the water

- lastPass.glsl: a surface tagged SKY_MIRROR (the sea) marches its reflection off the flat plane (world up, in view
  space), not off the wave's own normal, and reads the hit nudged by the wave's tilt - Ultra Water's planar
  reflection, done by the SSR march. Marched along a choppy FFT sea's own normals, the rays went every way and found
  the shore only here and there: the island never showed in the water. Now the shore, palms, boat and mountains
  are mirrored, broken up by the waves.
- Ocean.glsl shore breakers: off the sand (depth 0..0.4 m fades them in), their push eased over the last metres,
  a pinch budget of 0.5 (the reference's 0.85 folds on this sheet), the cosine clamped before pow (a hair below zero
  is NaN on Metal), a band limit to the sheet's cells (12-24 a wavelength), and sets back at the reference's 0.55 m
  with steepness 0.5: at 1 m they stood above this steep beach in faceted spikes.

## 37. Bamboo frame time

- Batching was never the problem: PYROS_BATCH_REPORT shows ~2000 drawn, all but the figure (skinned) and a few
  crossfading levels batched.
- Leaves: levels of detail (build_bamboo.py) - every second card 1.3x larger past 22 m, every fourth 1.7x past 50 m,
  ownMaterials (a scene material would drop their picture). GBuffer.CutOut 14.7 -> 11.3..13.5 ms GPU.
  Tried and reverted: one tight quad per drawn leaf (the spray picture is 90% clear) - 16x the triangles, the pass
  went to 24.7 ms: it is bound by vertices as much as by fill.
- Sun shadow cache (IRenderer::RecordSunCache): a shadow proxy (shadowRenderable) was compared by the component's
  level of detail, so every clump that changed level as the camera walked "left" and re-entered the kept map -
  ~4 million texels redrawn a frame. A proxy's level and LOD scale are now left out: ~0.4 million on most frames
  (what is left: clumps crossing the proxy's near distance, where they cast with their own mesh).
- The walk's frame time on the fanless Air stays 41-58 ms between runs (it throttles as it warms); GPU exact total
  34-40 ms. With the camera moving, the cache still shifts its four kept maps every frame (Shadow.Cache ~8-10 ms
  GPU, ~17-20 ms CPU): a cache only a still camera gains from - the next thing to look at.
- The cache off for the walk (bamboo_cache.txt "0"): one pair said 36/45 ms against 68/55 ms; six runs interleaved
  after it said 47-50 ms off against 45-55 ms on - no difference. Left on. On this Air a single pair of runs
  does not tell a change of 10 ms from the machine warming.

## 38. Bamboo plaza: stone, warmth, depth (against Duarte's reference)

- The plaza's slabs were flat quads on one dark, blurry texture with a grid baked in: a painted sheet. Now each
  slab is a block 7 cm thick with a rounded edge (two bevel rings, normals turned from up to out), a 3 cm gap of
  earth round it, a small tilt and sink of its own, its texture turned its own way and carried down the edge
  (not smeared by a planar map), and one of three tones (pale, mid, damp). Rings of 1.1 m, slabs ~1.15 m long,
  as in the reference.
- Stone: Poly Haven `rock_boulder_dry` (CC0), its colour's contrast halved (rock_boulder_dry_soft_diff_2k.jpg -
  the scan's lichen blotches read as dirt on a plaza), tinted cool grey.
- Bamboo: a culm texture baked by tools/bake_culm.py (fibres, the dark band above a node, the waxy bloom under
  the next, yellowed patches, lichen), each internode mapped node to node. Thicker stalks (5.5-11 cm), fewer a
  clump (9-14).
- Depth: clumps deeper in the grove use darker copies of the stalk meshes (x1, x0.7, x0.48 past 3 m and 6.5 m
  from the clearing): the probes are metres apart and cannot see the light thinning between stalks, so the
  tenth row stood as bright as the first - a flat green wall.
- Warmth: sun (1.0, 0.80, 0.56) at 0.9 (was a blue-white 0.22), warm sky for the GI and the ambient, saturation
  1.0 (was 0.82), exposure +0.35.
- Leaves: the cards were a diagram (a straight stick, seven leaves). tools/bake_bamboo_leaves.py draws dense
  sprays - arching twigs, fans of slim leaves, each its own green with a lighter midrib - and two farther
  versions with the leaves run together into masses (alpha spread and re-cut): leaf level 1 uses the mid one,
  level 2 the far one. 14-20 cards a stalk over its top 45%.
- Far flicker (Duarte: "some bambus flicker in the faraway"): drawn leaves under a pixel against the sky, and
  slivers of sky through the grove's last gaps 30 m out. The far masses, and a dark backdrop at x = +-31.5
  behind the grove: the worst flickering pixels in the far grove (still camera, high-passed frame difference,
  top 200) 10.5 -> 2.9.
- Clumps kept 2.6 m from the way and the plaza (a clump's stalks spread ~2.5 m - one stood in the path), ferns
  off the way where it meets the plaza.
- Captures now go through the editor's socket (scratchpad sockshots.py), never the screen.
- Frame time after all of it (walk, fanless Air): median 27.5 ms, p90 36 ms (was 45-55 ms) - fewer, thicker
  stalks a clump (9-14, not 14-20) more than pay for the denser leaf cards.
- Ground: Poly Haven forest_leaves_02 (moss and leaf litter) for the bare red earth between the stones and the
  ferns; stalks a little slimmer (5-9.5 cm) and less yellow.
- Culm normal map (tools/bake_culm.py, bamboo_culm_nor_gl.png): fibre grooves and the node ridges, so the light
  runs along a stalk and catches each node instead of shading a painted tube.

## 39. NatureLab's sea after "the trail is like a sticker ... the water is all fucked up"

- Deep water's colour Ultra Water's in-scatter, 'clear' (0, 0.05, 0.08) - it was (0.006, 0.03, 0.03), lead - and
  its SSS (after Atlas: sunlight through a crest seen against the sun, k1 1.6 h, k2 0.06, 0.05 diffuse) added to
  the indirect channel, under (1 - bed transmittance).
- Foam: with the FFT sea, the old hand-drawn shore sheet and surf layer under Ultra Water's whitewater are off -
  they were the white blanket over the beach. The wake is the whitecaps' eroded lace (world-fixed, drawn out
  along the course) instead of a noise pattern (the sticker). Foam shaded as Ultra Water's: a film tinted by the
  water where thin, white where heaped, the billows' creases darker and their slope bumping the normal.
- The motion: the shore breakers' displacement tore the sheet near the shore into spikes and facets that crawled
  as the eye moved - at any height, up or sideways. Ablated (nature_surf 0 clean; steep 0 and height 0.25 not).
  The breakers are now in the shading only (SHORE_LIFT 0.0 in Ocean.glsl); root cause still open.
- The polar grid: tried snapped and stepped, reverted - Ultra Water's is deliberately not ("displacement is
  band-limited to the local vertex spacing, which is what kills swimming - no clipmap snapping needed"), and a
  snap coarser than the rings by the eye made those pop.
- nature_slow.txt (Nature.lua): the sea and the boat at a share of real time, so socket frames taken ~0.14 s
  apart show the motion at 60 fps (scratchpad seaframes.py with SLOW=0.1).
- Reflections: IRenderer::SetSkyMirrorLevel (Lua setSkyMirrorLevel / clearSkyMirrorLevel; Nature.lua sets 20 m)
  gives the deferred composite (lastPass.glsl uMirrorLevel, std140 296) the still water's height: a sky
  mirror's ray leaves that plane under the pixel (clamped to +-0.8 m, the waves' reach - at the sand the water
  rides up the beach), as Ultra Water's planar reflection reads vWorld.x, 0, vWorld.z. From each wave's own
  height the shore's image jumped wave by wave into zig-zags. Far reflections now continuous; at the sand,
  close to the eye, still faint and broken - open.
- Breakers' tearing, root cause found in part (scratchpad breaker/eval.py: p3d_shoreWaves' height on the CPU with
  the baked maps): the baked front direction flips ~180 degrees between neighbouring texels where two fronts
  meet, and the breakers' height went by it twice - the shore's exposure to the swell, and the switch between the
  map's shoaling and Green's law - jumping along a wiggly line. Now the exposure is from the distance field's
  smooth gradient, the direction falls back to it where the baked one is short (blended), and the shoaling
  blends on the map's own value: the line gone, the field's steepest slope 1.83 -> 0.90. With the lift back on
  the shards were gone but faceted lumps stayed near the boat: SHORE_LIFT stays 0.0 (shading only) for now.
- The lumps left with the lift on, ruled out one by one (boat view, scratchpad boat/lump*): not the band limit,
  not SSAO, not the grid's density (3x denser, the same), not self-shadowing (the sea casts none); half the
  height, smaller. Not the screen-space reflections either (off for the sea, the same). So it is the shape: breakers to
  1.3 m, sharp-crested (P 1.6), the chop on them. SHORE_LIFT stays 0.0 until that shape is softened.
- Softer breakers tried with the lift on (crest P 1.25, cap 0.28 depth + 0.10): still lumpy against the
  shading-only sea. Decided: on this sheet the breakers are drawn by the shading, not the geometry.
- Reflections at the sand: from the plane a ray runs low under the crests in front and was stopped by the water
  itself (rejected as coplanar: the sky instead). Not the clamp (0.35 and 0.8 the same). lastPass now traces
  again from the pixel's own height where the plane's ray found nothing else: the shore's reflection back at
  the sand, the plane's clean one further out. Clamp at +-0.35 m.

## 40. The sea mirrors the sky itself (a sky panorama)

- Why the sea read as grey satin, plastic: it mirrored the sky's spherical harmonics - the sky smoothed to a few
  numbers - and Ocean.glsl greyed that toward the horizon by hand (to 1.4x its luminance at 72%), so every wave
  saw the same grey and the fine cascades' detail had nothing to show. Ultra Water mirrors a sky cube.
- The Sky effect has a third pass (uCloudPass 2, a side pass at a fifth of the frame, HDR): the sky as a
  panorama - round the horizon across, from 3.4 degrees under it to the zenith - its clouds included, written
  as light (skyDecode). PostEffectChain publishes it (IRenderer::SetSkyPanoramaSource; IEffect's destructor
  withdraws it), IRenderer::GetSkyPanorama() hands out its texture.
- CustomShaderMaterial::AddSkyPanoramaSampler(name) (Lua mat:addSkyPanorama): the panorama bound as that
  sampler after the material's own textures, whatever this frame's is (one of its own stands in while none is).
  Lua hasSkyPanorama().
- Ocean.glsl p3d_skyPano(Rd) replaces the harmonics' sky where uGlobals[8].x (Nature.lua sets it from
  hasSkyPanorama()). The golden hour's sea takes the sky's gold and blue and its clouds; midday's, its blue and
  the clouds' white on each wave turned to them.
- Tried first and reverted: reading the sky from the frame in lastPass - the sky is drawn after it (the Sky
  effect fills where the depth is far), so the frame there was only the clear colour.
- Bound by itself: a custom material whose shader names uSkyPanorama gets it (CustomShaderMaterial's
  constructor); with no Sky effect a 1x1 texel of alpha 0 stands in, and the shader keeps the harmonics' sky
  where alpha is 0 - no script hook (Nature.lua's removed). Erangel's Lake.glsl reads it the same way.
- Erangel's Island had a fragment shader failing on Metal ("no matching member function for call to 'sample'",
  then "no matching function for call to 'PointShadowFactor'", the program unlinked - not drawn): five of its
  hand-written shaders (rock, rock_desert, Water, terrain/splat_terrain, terrain/cave_rock) declared
  uPointShadowMaps[4], the engine binds and every one of them reads one. Declared [1]: no errors on load.
  (The editor's own upgrade also rewrote LampGlow/ZoneWall.generated.glsl on load, by design.)
- Erangel's lake (Lake.glsl, scenes/Island.json): no shore-foam sheet for bounded water (a lake has no surf -
  the white line round it was a sticker's outline from above); the plane's sea-side edge 6 m in (z 163, scale
  0.35) - it sliced two low hollows beyond the basin with straight edges. Enlarging the plane instead flooded
  the low grass east of it: tried, reverted.
- Ashvale's sea, lake and river shaders read the sky panorama the same way (no errors on load).
- Ashvale's Mirrormere was not drawn at all (the basin's grass showed, the lake's blue only in specks): the model
  importer recentres a mesh whose middle is far from the origin (AssimpModelImporter's RecenterIfFarFromOrigin),
  and the lake - built where it is in the world, placed at the origin - was moved to y 0 round the world's middle,
  under the ground (the .p3dm held no vertex at 58 m). Ruled out on the way: its shader (the sea's on it the same),
  its give-way discard, culling, the face culled, its depth map's compression. Fixed: Mirrormere placed at its
  middle (2252, 58, 220) in scenes/Ashvale.json; tools/build_water.py now makes it about that middle and records
  lake_pos in water.json, tools/build_scene.py places it there, as the river pieces were. Not re-run: it rewrites
  the three water shaders from NatureLab's Ocean.glsl, whose vertex stage now has the polar grid's if/else that its
  text replacements were not written for - check before the next build.
- Ashvale's two depth pictures carry "data": true (scene and build_scene.py).
- tools/build_water.py: its text replacements fitted to the template's polar-grid if/else (the polar branch off, the old remap's else removed whole for a lake or a river); dry-run checked, not re-run.

## 41. The demos (VisualCheck, Metal): 26 differ from their baselines of 23 October

- None from today's sky panorama (demo 42 the same with it off). Explained: SSAO is GTAO now (open ground 1, no dark
  outlines - weaker, and right); the physical sky; exposure and tone shifts; better GPU particles, Cornell box,
  parallax, Metro. Not accepted - Duarte's call.
- 18 Depth of Field: its script (scripts/dof_setup.lua) set the background to pure red; a dark studio grey now (the
  manifest says the same).
- 42 Volumetric Clouds (open): under the horizon a flat grey-brown band with a hard edge, where the baseline had a
  white haze. Not the floor's size (40 m -> 6 km: the same frame), not uAirHaze (2 -> 6: the same) - what draws there
  takes neither; to trace.
