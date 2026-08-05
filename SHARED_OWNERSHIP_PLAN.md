# Shared-ownership (shared_ptr) for GameObject/IComponent, then Renderable/IMaterial

Design doc, Stage 1+2 IMPLEMENTED (2026-08-05). Written 2026-08-04 so a future session can
pick this up without re-deriving the architecture or re-discovering the correctness
landmine below. Read this in full before writing any code for this change.

**Status (2026-08-05):** Stage 1 (GameObject/IComponent) and Stage 2 (Renderable/IMaterial)
are in. Texture ownership in materials/particles/TextureAnimation (and LoadedSceneAssets
skeleton/texture animations) is closed - `GenericShaderMaterial`/`CustomShaderMaterial`
hold `shared_ptr<Texture>`, `ParticleSystemDesc::texture` and animation frames are
shared, Get*Map/GetFrame stay observing `Texture*`. Verify with:
`cmake --build build_gl --target PyrosEngine SimplePhysics DemoLauncher`.
Other standalone examples are intentionally not migrated yet (scope correction below) -
a full `cmake --build` of every example target will fail until those call sites move to
`make_shared` / drop trailing `delete`s. SimplePhysics is the Stage 2 fan-out proof
(1000 RenderingComponents sharing one Cube + one GenericShaderMaterial).

**Scope correction (2026-08-04): ignore the other ~29 standalone examples.** Going
forward, `DemoLauncher` is the only example that matters for verification, plus exactly
one pure-C++ example kept as a base/reference (not converted to Lua) - `SimplePhysics` is
the natural pick if nothing else is specified, since it already exercises
GameObject+components+physics+high-fan-out shared geometry (1000 `RenderingComponent`s off
one `Cube`), making it the best single stress test for this exact change. Every mention of
"examples"/"the example suite" below should be read as "DemoLauncher + that one C++
example," not literally every example in the repo.

## Context

The engine currently owns `GameObject`/`IComponent`/`Renderable`/`IMaterial` via raw
pointers with no consistent lifetime model. Investigating why revealed this isn't just a
"someone forgot to add a destructor" gap - it's structural: these types are bound to Lua
via sol2's default value-semantics (`sol::constructors<...>`), which placement-constructs
the object *inside Lua's own userdata block* (confirmed by reading `sol.hpp`'s
`detail::usertype_allocate<T>`/`lua_newuserdata` + the `__gc` handler calling a placement
destructor, never `delete`). A raw pointer obtained from a Lua-constructed object is
therefore not a normal heap pointer - calling `delete` on it is undefined behavior. The
same C++ types are also constructed directly via plain `new` throughout the example suite,
where `delete` *is* correct and required. Nothing distinguishes the two cases today, and
this can't be fixed from outside the engine (game/example code has no control over how
`GameObject.h`/`PyrosBindings.cpp` define ownership) - it has to change at the engine
level.

The proven fix already exists in this codebase: `AudioBus` (`include/Pyros3D/Audio/
AudioBus.h`/`.cpp`) is bound via `sol::factories([]() { return std::make_shared<AudioBus>();
})` instead of `sol::constructors<...>`, stored everywhere as `std::shared_ptr<AudioBus>`.
Under `shared_ptr`, Lua and C++ share one real reference count - either side can drop its
reference safely, and the object is destroyed via a normal `delete` only when the last
reference (Lua's or C++'s) goes away. This plan generalizes that exact recipe to
`GameObject`/`IComponent` first, then `Renderable`/`IMaterial`.

**Important nuance to carry through the whole plan**: `shared_ptr` only answers "when does
the C++ object's memory get freed." It does NOT replace the engine's existing `Unregister()`
plumbing, which frees GPU/Bullet-side resources (VAOs, rigid bodies) at an *intentional*
moment (removed from scene, demo switched away) - that still needs to be called explicitly
at the right time, same as today. `shared_ptr` fixes "the object leaks/dangles because
nobody tracked it," not "when should GPU resources be released," which stays a deliberate
`Scene->Remove()`/`Unregister()` call.

## Core mechanism (same shape everywhere)

- Lua binding: `sol::factories(...)` returning `std::make_shared<T>(args...)`, replacing
  `sol::constructors<sol::types<...>>`. **Lua call syntax is unaffected** - `Cube.new(...)`/
  `GameObject.new()` read identically to a script either way, exactly as `AudioBus.new()`
  already proves. This means NeonPulse's and DemoLauncher's Lua content needs zero changes
  for this - the whole Lua-authored game/demo layer is unaffected, only C++ signatures and
  example construction sites change.
- Engine-internal storage: wherever the engine currently holds an *owning* raw pointer to
  one of these types, it becomes `std::shared_ptr<T>`.
- Back-pointers stay raw, deliberately, not `shared_ptr`: `IComponent::Owner` (`GameObject*`)
  and `GameObject::_Owner` (parent pointer, `GameObject.h:174`) must NOT become owning
  `shared_ptr`s - that would create a reference cycle (parent owns child via `shared_ptr`,
  child points back to parent via `shared_ptr`) that `shared_ptr`'s refcounting can never
  collect. These stay plain observing pointers, same as today - components/children are
  always used while held alive by their owner, never independently outliving it.
- Callback/observer parameters stay raw too: `IPhysicsComponent::OnCollisionEnter(
  IPhysicsComponent* other)` doesn't need to extend the *other* object's lifetime, just
  observe it for the duration of the call - no change needed there.

## Stage 1 - `GameObject`/`IComponent`

- `include/Pyros3D/GameObjects/GameObject.h`/`.cpp`: `Components`
  (`std::vector<IComponent*>` → `std::vector<std::shared_ptr<IComponent>>`), `_Childs`
  (`std::vector<GameObject*>` → `std::vector<std::shared_ptr<GameObject>>`). `Add(shared_ptr
  <IComponent>)`/`Add(shared_ptr<GameObject>)` replace the raw-pointer overloads.
  `GetComponents()`/`GetChildren()` return the `shared_ptr` vectors directly - callers that
  only observe (the common case, e.g. Lua bindings, `RegisterComponents`) work unchanged
  via `.get()`/implicit dereference.
- `include/Pyros3D/Components/IComponent.h`: no storage change needed here (`Owner` stays
  raw, see above) - just confirms the type callers hand to `GameObject::Add()`.
- `include/Pyros3D/SceneGraph/SceneGraph.h`/`.cpp`: `_GameObjectListDynamic`/
  `_GameObjectListStatic{Previous,After}`/`_GameObjectListALL`
  (`std::vector<GameObject*>` → `std::vector<std::shared_ptr<GameObject>>`). `Add`/`Remove`
  take/return `shared_ptr<GameObject>` accordingly.
- `include/Pyros3D/Physics/PhysicsEngines/IPhysics.h`/`BulletPhysics.h`/`.cpp`: every
  `Create*` factory (`CreateBox`, `CreateTriangleMesh`, etc.) returns `shared_ptr<
  IPhysicsComponent>` instead of a raw pointer - `IPhysicsComponent` derives from
  `IComponent`, so this flows into `GameObject::Add()` the same way as any other component.
- `src/Pyros3D/Utils/Bindings/PyrosBindings.cpp`: convert every `IComponent`-derived
  usertype registration (`RenderingComponent`, `LuaComponent`, `ParticleSystem`,
  `DirectionalLight`/`PointLight`/`SpotLight`, every physics component, `AudioSource`) and
  `GameObject` itself from `sol::constructors<...>` to `sol::factories(...)` +
  `make_shared`, mirroring `AudioBus`'s exact registration shape.
- Examples: every `new GameObject()`/`new RenderingComponent(...)`/`new PhysicsBox(...)`/
  etc. constructed directly in C++ becomes `std::make_shared<T>(...)`, and the now-redundant
  manual `delete`/`Scene->Remove()`-then-`delete component` sequences simplify - `Scene->
  Remove()`/`Unregister()` still runs explicitly (GPU/Bullet cleanup timing, see the nuance
  above), but the trailing manual `delete`s go away since dropping the last `shared_ptr`
  reference now does that automatically and correctly.
- **Verify stage 1 alone before starting stage 2**: full rebuild, run every example on both
  GL and Vulkan, confirm zero crashes/leaks (ASAN pass), specifically re-exercise
  `ArenaFPS`'s previously-crash-prone teardown path and `Pyros3DEditor`'s
  interactively-created-then-leaked GameObjects.

## Stage 2 - `Renderable`/`IMaterial`

Depends on stage 1 being stable (touches the same `RenderingComponent` construction sites).

- `include/Pyros3D/Rendering/Components/Rendering/RenderingComponent.h`/`.cpp`:
  `renderable` (`Renderable*` → `shared_ptr<Renderable>`), `RenderingMesh::Material`
  (`IMaterial*` → `shared_ptr<IMaterial>`). Both constructors and both `AddLOD` overloads
  take `shared_ptr` parameters.
- `src/Pyros3D/Utils/Bindings/PyrosBindings.cpp`: convert every `Renderable`-derived
  usertype (`Cube`, `Sphere`, `Plane`, `Cylinder`, `Cone`, `Capsule`, `Torus`, `TorusKnot`,
  `Model`, `Decal`, `Text`, `Terrain`) and every `IMaterial`-derived usertype
  (`GenericShaderMaterial`, `CustomShaderMaterial`) the same way as stage 1.
- Examples: the high-fan-out shared-geometry cases matter most here for actually proving
  this works (`SimplePhysics`: 1000 `RenderingComponent`s off one `Cube`, `DeferredRendering`:
  100, `PBRSpheres`/`DeferredPBRSpheres`/`SSRTest`/`ScreenSpaceReflection`: 5-26 each) -
  migrate `new Cube(...)` → `make_shared<Cube>(...)`, drop the now-redundant single trailing
  `delete cubeHandle`.
- `include/Pyros3D/Utils/Serialization/SceneSerializer.h`/`.cpp`: `LoadedSceneAssets`'s
  vectors (`GameObject*`, `IMaterial*`, `Texture*`, `Renderable*`, etc.) become `shared_ptr`
  vectors for consistency - `UnloadScene()`'s job doesn't change (still frees exactly what
  one `LoadScene()` call produced), just the pointer type.
- **Verify stage 2**: same full-suite + ASAN pass, plus explicit confirmation the
  high-fan-out shared-geometry examples still render identically (visual spot-check, not
  just "doesn't crash") after migrating off manual `new`/`delete`.

## Non-goals

- No change to how GPU/Bullet resources are released - `Unregister()`/`Scene->Remove()`
  still runs at the same deliberate moments it does today; `shared_ptr` only fixes when the
  C++ object's *memory* is freed, not when its GPU-side resources are.
- No change to Lua-authored content (NeonPulse's `.lua` files, DemoLauncher's scene JSONs) -
  `sol::factories` keeps Lua construction syntax identical.
- `IComponent::Owner`/`GameObject::_Owner` (parent/back-pointers) stay raw - converting
  these to `shared_ptr` would create uncollectable reference cycles; this is a deliberate
  exclusion, not an oversight.

## Verification (both stages)

Full rebuild + run the entire example suite on GL and Vulkan after each stage, not just at
the end. An ASAN pass specifically, since this changes allocation/ownership patterns
engine-wide and is exactly the kind of change most likely to surface a subtle
double-free/use-after-free if something was missed. Re-exercise the specific
already-known-fragile teardown paths found earlier this session (`ArenaFPS`'s prior crash,
`Pyros3DEditor`'s unbounded interactive leak, `DeferredRendering`/`NeonPulse`'s
previously-empty `Shutdown()`) as concrete pass/fail checks, not just "nothing obviously
broke."

## Related work not yet started

A separate, smaller design doc (`TERRAIN_STREAMING_PLAN.md`, same repo root) and a
NeonPulse/DemoLauncher integration plan (discussed in-session, not yet written to a file)
are also queued but independent of this one - none of the three block each other.
