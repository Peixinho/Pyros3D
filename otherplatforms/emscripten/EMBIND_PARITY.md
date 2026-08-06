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
| Scene | done (save/load skipped — need Lua state) |
| GameObject / GameObjectBase | done (JS uses GameObject; Lua hooks skipped) |
| onUpdate / onInit / onDestroy / attachScript | skipped (Lua-only) |
| asGameObject | skipped (Lua shared_ptr coercion) |
| getComponent | skipped (Lua sol::object return) |
| Projection | done |
| ForwardRenderer | done (full IRenderer surface via wrappers) |
| DeferredRenderer, VelocityRenderer, CubemapRenderer | done |
| FrameBuffer | done |
| Renderable, IMaterial, RenderingMesh | done |
| IComponent, IlightComponent | done |
| IPhysicsComponent | done (collision callbacks skipped — Lua std::function) |
| RayCastHit | done |
| LuaComponent / LuaComponent_fromFile | skipped (Lua-only) |
| DirectionalLight, PointLight, SpotLight | done (C++ types; no Lua onUpdate hooks) |
| Uniform, Texture, Shader | done (Uniform::SetValue void* skipped) |
| GenericShaderMaterial, CustomShaderMaterial | done (`fromShader` factory — Embind arity-only overloads) |
| setMaterialExtraUniformBlock | skipped (sol::table) |
| Cube…TorusKnot, Model, Font, Text | done |
| Decal | skipped (DecalVertex vector from JS) |
| RenderingComponent, RenderingInstancedComponent | done (`fromOptions` / `fromOptionsDist`; AddBuffer skipped) |
| ParticleSystem* | done |
| SekeletonAnimation / SekeletonAnimationInstance | done (Lua typo names kept) |
| TextureAnimation* | done |
| IPhysics / BulletPhysics | done (convex-hull / multi-sphere vector APIs partial) |
| PostEffectsManager + effect types | done |
| VignetEffect / MotionBlur | done (Lua typo names) |
| addPostEffect, clearPostEffectHandles, motionBlur*, ssao*, buildDOF* | done |
| AudioManager / AudioBus / Sound / AudioSource | done |
| Key_* / MouseButton_* | done (enums) |
| Input (LuaInputBridge) | skipped (Lua callbacks; use DOM/canvas) |
| File | done (open/close/size; byte buffers via FS) |
| getMousePosition | done |
| placeDecalAtCursor | skipped (sol material + LUA_RenderingComponent) |
| ShaderUsage_* and other enum constants | done (`PyrosEmbindEnums.cpp`; Clamp + Clapm alias) |

## Files
- `PyrosEmbind.cpp` — Scene, GameObject, Projection, ForwardRenderer, IComponent
- `PyrosEmbindEnums.cpp` — constants
- `PyrosEmbindMath.cpp` — Vec/Quat/Matrix
- `PyrosEmbindRender.cpp` — Deferred/Velocity/Cubemap, FBO, materials, lights, RC
- `PyrosEmbindAssets.cpp` — Texture, shapes, Model, Font, Text, anim, particles
- `PyrosEmbindPhysics.cpp` — Bullet / raycast
- `PyrosEmbindPostFX.cpp` — post FX + helpers
- `PyrosEmbindAudio.cpp` — audio
- `PyrosEmbindMisc.cpp` — File, getMousePosition

Smoke test: `examples/web` — Neon Pulse (`neonpulse/*.js`).
