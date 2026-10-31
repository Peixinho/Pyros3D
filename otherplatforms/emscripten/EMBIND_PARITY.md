# Embind ↔ Lua API parity

Goal: every Lua `new_usertype` / `set_function` in `PyrosBindings.cpp` has a matching Embind export (modular TUs under `src/Pyros3D/Utils/Bindings/`).

JS clients load `createPyros3D` (MODULARIZE) and construct `Application` + engine types from JS — see `examples/web/`.

| Type / function | Status |
|-----------------|--------|
| Application (SDL2 window) | done (`examples/web/native`) |
| WindowType_* | done |
| Vec2, Vec3, Vec4 | done (`PyrosEmbindMath.cpp`) |
| Quaternion, Matrix | done |
| degToRad | done |
| Scene | done; `load` / `loadText` / `unload` / `getMeta` / `find` / `getObjects` read an editor scene without Lua (scripts not run; `P.loadSceneFromUrl` in pyros-assets.js fetches it and its assets); save skipped |
| GameObject / GameObjectBase | done (JS uses GameObject; Lua hooks skipped) |
| onUpdate / onInit / onDestroy / attachScript | skipped (Lua-only) |
| asGameObject | skipped (Lua shared_ptr coercion) |
| getComponent | `getPhysicsComponent` / `getRenderingComponent` / `getChildren` on GameObject (no by-type lookup) |
| Projection | done |
| Renderer (`app.createRenderer()`) | the engine's renderer on the canvas: `renderScene(projection, camera, scene)`, `resize`, `useSceneSettings(scene)`, `setBackground` / `setGlobalLight` (sRGB), `setSky` / `setHaze` / `setClouds` / `setReflections`, `setAutoExposure(on, ev)` / `setExposure`, `setTemporalAA`, `setToneCurve`, `setFrustumCulling`, LOD |
| Renderable, IMaterial, RenderingMesh | done |
| IComponent, IlightComponent | done |
| IPhysicsComponent | done, with `onCollisionEnter(fn)` / `onCollisionExit(fn)` |
| RayCastHit | done |
| LuaComponent / LuaComponent_fromFile | skipped (Lua-only) |
| DirectionalLight, PointLight, SpotLight | done (C++ types; no Lua onUpdate hooks) |
| Texture | done (+loadTextureFromMemory / JS loadFromUrl; `updateData(typedArray, level)` refills a createEmptyTexture, e.g. from a canvas) |
| GenericShaderMaterial, CustomShaderMaterial | done (custom: a `.surface` file, or `""` and `setSurface(text)`) |
| setMaterialExtraUniformBlock | skipped (sol::table) |
| Cube…TorusKnot, Model, Font, Text | done |
| Decal | skipped (DecalVertex vector from JS) |
| RenderingComponent, RenderingInstancedComponent | done (`fromOptions` / `fromOptionsDist`; AddBuffer skipped) |
| ParticleSystem* | done |
| SekeletonAnimation / SekeletonAnimationInstance | done (Lua typo names kept) |
| TextureAnimation* | done |
| IPhysics / Box3DPhysics | done (convex-hull / multi-sphere vector APIs partial) |
| GenericShaderMaterial wind | `setWind(strength, rate, frequency, lean)` with `ShaderUsage_VertexWind` |
| AudioManager / AudioBus / Sound / AudioSource | done |
| Key_* / MouseButton_* | done (enums) |
| Input (LuaInputBridge) | skipped (Lua callbacks; use DOM/canvas) |
| File | done (open/close/size; byte buffers via FS) |
| getMousePosition | done |
| placeDecalAtCursor | skipped (sol material + LUA_RenderingComponent) |
| ShaderUsage_* and other enum constants | done (`PyrosEmbindEnums.cpp`; Clamp + Clapm alias) |

## Files
- `PyrosEmbind.cpp` — Scene, GameObject, Projection, Renderer, IComponent
- `PyrosEmbindEnums.cpp` — constants
- `PyrosEmbindMath.cpp` — Vec/Quat/Matrix
- `PyrosEmbindRender.cpp` — materials, lights, RC
- `PyrosEmbindAssets.cpp` — Texture, shapes, Model, Font, Text, anim, particles
- `PyrosEmbindPhysics.cpp` — Box3D / raycast
- `PyrosEmbindUI.cpp` — ui* functions (the renderer draws a scene's UI canvases)
- `PyrosEmbindAudio.cpp` — audio
- `PyrosEmbindMisc.cpp` — File, getMousePosition

Smoke test: `examples/web` — Neon Pulse (`neonpulse/*.js`).
| PhysicsVehicle wheel pose | `getWheelTravel` / `getWheelSteer` / `getWheelSpin` / `isWheelInContact` |
