# Animation: format redesign, rig assets, and IK

Design doc. **All six phases implemented** (see "Status" below). Written 2026-08-22;
implemented the same day. Read "Things already paid for" before touching any of this -
most of it cost a measurement to find and every item silently produces plausible-looking
wrong output rather than an error.

**Status: DONE.** Phases 1-3 landed as a single `.p3da` v1 bump rather than three separate
ones (they all touch the same header). Phase 4 is a `<model>.rig.json` sidecar, not a block
inside the `.p3dm` - the `.p3dm` is regenerated wholesale by AssimpImporter on every
re-import, which would destroy hand-authored joint limits that cannot be re-derived from the
FBX. Phase 5's solver lives in the engine (`AnimationManager/IKSolver`), Phase 6's component
in `AnimationManager/Components/IKComponent`.

Also fixed along the way, outside the plan: the bind *local* transform was dropped in two
places - `SkeletonAnimationInstance`'s constructor left `boneTransformation` at identity, and
`SkeletonAnimation::Update()` rewrote every bone to identity on any frame with no clip
playing. The second also stomped externally-posed bones, which is why runtime IK could never
have held before Phase 6's ordering hook.

Build/verify with: `cmake --build build_editor --target PyrosBuilder` (OpenGL + Lua,
Debug). That builds `PyrosEngine`, `AssimpImporter` and `PyrosBuilder` together.

---

## What exists today

**Engine**
- `AnimationLoader` (`Utils/ModelLoaders/MultiModelLoader/`) - reads AND writes `.p3da`
  (`Save()` added for the editor; the only other writer is the `AssimpImporter` tool).
- `SkeletonAnimation` / `SkeletonAnimationInstance` (`AnimationManager/`) - clip container
  and per-mesh playback. Blending and per-bone layers are runtime features that already
  work; `SetAnimations()` feeds clips from memory (for previewing unsaved edits);
  `SampleChannel()` is the one shared sampler used by both playback and the editor's scrub;
  the pose API (`SetBoneLocalTransform` / `GetBoneGlobalTransform` / `RefreshSkinning` /
  `ApplyAnimationAtTime` / `ResetToBindPose`) is what any solver should build on.

**Editor** (`editor/src/editor/`)
- `AnimationEditorDocument.{h,cpp}` - the document IS the `.p3da`; clips edited in place,
  written straight back. Undo via whole-clip snapshots.
- `AnimationPreview.{h,cpp}` - private SceneGraph/ForwardRenderer/FBO viewport, skeleton
  drawing, bone picking, libgizmo posing, blend playback.
- `UI/AnimationEditor.{h,cpp}` - toolbar, bone tree, dope sheet, transport, Blend tab.
- Host wiring in `Editor.{h,cpp}`; `.p3da` asset handling in `ProjectManager.{h,cpp}`.
- `pyros3d-mcp-server.py` - MCP tools, including a byte-exact Python `.p3da` reader/writer
  (`_read_p3da` / `_write_p3da`) that works with no editor running. **Keep these in lockstep
  with any format change** - they are a second implementation and will silently diverge.

---

## Current `.p3da` layout (exact)

No magic, no version. The file opens directly with the clip count:

```
int32   clipCount
  int32 nameLen, char[nameLen] name
  int32 channelCount
  f32   duration
  f32   ticksPerSecond
  channelCount x:
    int32 nodeNameLen, char[nodeNameLen] nodeName
    int32 posCount,   posCount   x (f32 time, f32 x, y, z)
    int32 rotCount,   rotCount   x (f32 time, f32 w, x, y, z)   <- Quaternion is w-FIRST
    int32 scaleCount, scaleCount x (f32 time, f32 x, y, z)
```

`Load()` divides every time and the duration by `ticksPerSecond`, then reports
`TicksPerSecond` as 1 - so a loaded clip is already in seconds. `Save()` therefore writes
times verbatim with `ticksPerSecond` 1. Anything authored must follow the same convention.

**Blast radius of a change** (all in this repo, fully enumerable):
`AnimationLoader.cpp` (read+write), `SkeletonAnimation.cpp` (consumer),
`tools/AssimpImporter/src/AssimpAnimationImporter.cpp` (write),
`pyros3d-mcp-server.py` (read+write), the editor document, and
`Utils/Serialization/SceneSerializer.cpp` (references clips **by index** - see Phase 2).

**Existing assets to migrate:** exactly four, all in `examples/assets/` - `walk.p3da`,
`run.p3da`, `alert.p3da`, `Animation.p3da` - plus whatever lives in user projects. The
Python writer round-trips `walk.p3da` byte-identically, so it is the migration tool.

---

## Phase 1 - versioned header (do this first, regardless of what else is chosen)

Add `magic ("P3DA") + uint32 version + clipCount`. Everything else can stay as-is.

Why first: the format is currently unversioned, so *any* later change needs a heuristic to
tell old files from new. Adding the header is still free **today** precisely because there
is no header - the current first word is a small positive clip count, so a magic word is
trivially distinguishable. `Load()` keeps reading headerless files as v0 forever.

- Bump both writers (`AnimationLoader::Save`, the importer) and the Python writer.
- `Load()`: peek the first 4 bytes; if they are not the magic, rewind and parse as v0.
- Acceptance: the four `examples/assets/*.p3da` still load unchanged, and a file saved by
  the editor round-trips through the Python reader (that cross-check caught the
  quaternion field order originally).

Estimated: small. This is the enabling step for everything below.

## Phase 2 - stable clip identity

Today a clip's runtime id **is its array index**, and `SceneSerializer` saves that index and
calls `Play(id)`. Deleting a clip silently renumbers every later clip, so unrelated scenes
start playing the wrong animation. This is real enough that the editor already warns about
it in two places (the Delete Clip popup and the `remove_animation_clip` MCP tool) - those
warnings are papering over a format problem.

- Give each clip a stable name (or GUID) in the file; resolve `Play()` by name with the
  index kept as a fallback for v0 files.
- `SceneSerializer` should save the name and fall back to the index only for old scenes.
- Acceptance: delete clip 0 of a 3-clip file, reload a scene that plays clip 2, and it
  still plays the same animation.

## Phase 3 - per-key interpolation

The visible authoring gap: keys are linear (position/scale `Lerp`, rotation `Slerp`) and
there is no ease/step/bezier. `{time, value}` becomes `{time, value, mode, tangents}`.

- Extend `SampleChannel()` - it is the single shared path, so playback and the editor's
  scrub both get it from one change.
- Dope sheet needs per-key mode display/editing.
- **Note:** scale keys currently round-trip through the file but do NOT deform the mesh -
  `trafo.Scale()` is commented out in the sampler and has been forever. Decide deliberately
  whether Phase 3 turns that on; doing so changes how every existing clip renders.

Follow-ons that become easy once the header exists: loop flag, authored fps (the editor
snaps at 30 but records nothing), events/notifies, root-motion flag.

## Phase 4 - a rig asset (this is a structural correction)

Some data belongs to neither the clip nor the project, but to the **skeleton**, which has
no asset of its own today:

| data | correct home |
|---|---|
| interpolation, events, loop, fps | the clip (`.p3da`) |
| **joint limits, IK chains, bone masks/layers** | **the rig - currently homeless** |

Blend layers are presently stored in `project.json` under `settings.animationBlends`, keyed
by animation path. That is wrong on reflection: an "UpperBody" mask is a property of the
*skeleton* and is reusable across every clip for that rig. Joint limits (needed by IK) have
the same shape.

Proposal: a small sidecar next to the `.p3dm` (e.g. `<model>.rig.json`) holding bone masks,
joint limits and IK chain definitions. Migrate `animationBlends`' layer half into it; the
per-clip weights/entries can stay project-side as preview state.

## Phase 5 - IK, as a bake-to-keys authoring tool

Design the API as a **chain** from day one so N-bone is a later branch, not a rewrite:

```
Solve(instance, rootBone, effectorBone, target, pole, iterations)
```

- Dispatch to a closed-form **two-bone** solution when the chain length is 2 (law of
  cosines + pole vector for the knee/elbow direction). Exact, non-iterative, deterministic.
- Fall back to FABRIK for longer chains (spine, tails, fingers).
- Put the solver **in the engine**, not the editor, so the editor's bake and any runtime
  component share one implementation and cannot disagree.

Determinism matters specifically for baking: the bake must be a pure function of
`(clip time, target)`. Do NOT seed an iterative solver from the previous frame's pose, or
scrubbing backwards will bake different keys than scrubbing forwards.

Editor side: an IK handle you drag; the solver writes the chain's bone rotations; Key
stores them as ordinary keys. Then "bake over range" - solve per frame across the clip - is
the feature people actually want (a foot staying planted while the hips move).

The real cost is **joint limits**, not the solver core (CCD ~40 lines, FABRIK ~60). Without
limits, knees bend backwards. Limits come from Phase 4.

## Phase 6 - runtime IK (optional, separate feature)

Baking cannot do what runtime IK does, because the target is not known at author time:
feet planting on terrain, head/spine look-at, hands on a moving prop.

The hook already exists - the editor's pose API is exactly what a solver needs:

```
SkeletonAnimation::Update(clock)          // clips write the pose
  -> solver reads GetBoneGlobalTransform, writes SetBoneLocalTransform
  -> RefreshSkinning()                     // one hierarchy walk, then draw
```

Genuine costs: **ordering** (`Update()` overwrites the whole pose each frame, so IK must run
strictly after it - needs a defined update order, not "whenever the component ticks"), and
**serialization** (an IK setup is per-object runtime state, so unlike blending it does belong
in the scene). Targets are naturally GameObjects.

---

## Things already paid for - do not rediscover these

Each of these produced plausible-looking wrong output, never an error.

1. **Euler angles in the engine are RADIANS.** `Quaternion::SetRotationFromEuler` and
   `Matrix::GetEulerFromRotationMatrix` both. `DEGTORAD`/`RADTODEG` are in `Core/Math/Math.h`.
   Passing 75 meaning degrees is a valid 75-radian rotation that wraps to about -23. The
   agent API and the bone inspector convert at the boundary; the Scene View's Properties
   panel does not (it shows raw radians).
2. **Blend weight is inverted from the engine's `scale`.** `Update()` blends in reverse with
   `SCALE(thisPose, accumulated, scale)`, so `scale` is the weight of the OTHER clips.
   `BlendWeightToScale()` (in `AnimationEditorDocument.h`) is the single conversion; verified
   by measurement (weight 1/0 reproduces walk exactly, 0.5/0.5 lands at the midpoint).
3. **Unkeyed bones hold bindPose, not identity.** `Play()` seeds
   `boneTransformationPerAnimation = bindPose` and `Update()` only overwrites bones a channel
   drives. `ApplyAnimationAtTime` matches this.
4. **OpenGL render targets are bottom-up.** Any viewport image needs V-flipped UVs
   (`uv0(0,1), uv1(1,0)`) guarded by `#if defined(_SDL2VULKAN) || defined(_SDL2METAL)`.
   Getting this wrong mirrors the whole viewport AND every screen-space mouse mapping.
5. **`PostEffectsManager` borrows the process-wide render device**, and
   `~PostEffectsManager` starts with `device->WaitIdle()`. By the time
   `Editor::Shutdown()` has run `CloseAllSceneDocuments()`, the device is already gone.
   Anything owning one must be destroyed BEFORE the scene documents. `MaterialPreview`
   still has this latent exposure.
6. **libgizmo's `SetGlobalTransform` is not what the name suggests.** `Rotate1Axe`'s local
   branch computes `R(inverse(globalTransform) * m_Axis, angle) * oldLocal`, and `m_Axis` is
   a RAW coordinate axis, not world-space. For a bone you must pass the inverse of the
   bone's own local rotation, or it turns about a skewed axis. Derivation is in
   `AnimationPreview::PrepareGizmo`. Do not "fix" libgizmo - SceneEditor depends on it.
7. **A skinned model's vertex AABB is not what you see.** The `.p3dm` vertices are in the
   modelled pose; the drawn result is those pushed through `bindPoseGlobal * boneOffset`.
   For `human.p3dm` the raw AABB is 18 units along Z while the posed skeleton is Y-up.
   Frame cameras from the posed skeleton.
8. **A channel can name a node that is not a bone.** Assimp exports carry these routinely;
   indexing the pose vector with the resulting -1 is an out-of-bounds write (guarded now).

## How to verify without a GUI

The editor writes `$TMPDIR/pyros3d-editor.json` (`{pid, port, token}`); send one JSON line
`{"id":1,"token":...,"cmd":...,"args":{...}}` to `127.0.0.1:<port>`. Every
`Editor::HandleAgentCommand` name works.

For animation specifically, `animation_skeleton` reports each joint's model-space position,
its projected viewport pixel, and the viewport image's screen origin - enough to click an
exact joint with `cliclick` and read the result back. That is how the gizmo axis bug was
found: drive a real drag, read the rotation back, extract its axis, and check the alignment
with a coordinate axis is 1.000 (a correct single-ring drag rotates about exactly one axis;
the broken one measured 0.72). `select_animation_bone` returns the selected bone's
descendants and their positions, which is how parent-to-child propagation was verified.

Note the window's screen origin from AppleScript includes the macOS title bar (~28px) while
ImGui's coordinates do not.

## Open questions - resolved

- Phase 3: scale keys are **opt-in per clip** via `ANIM_FLAG_APPLY_SCALE`. A v0 file has no
  flags word and loads as 0, so every existing clip renders exactly as before.
- Phase 4: **sidecar** (`<model>.rig.json`), not inside the `.p3dm`. The deciding argument is
  re-import: the `.p3dm` is a generated artifact and rewriting it would lose hand-authored
  joint limits. A sidecar also means the editor never writes the model file, so no rig edit
  can corrupt geometry.
- Phase 2: **GUID + name**. Scenes save the guid; `SkeletonAnimation::ResolveAnimationID`
  falls back guid -> name -> index, so scenes written before guids existed still resolve.
