# PyrosBuilder

The scene editor, revived from the standalone `editor_old` checkout and moved
into the engine tree so it builds against the engine as it is today rather than
against a snapshot of it.

## Building

```sh
cmake -S . -B build_editor -DBUILD_EDITOR=ON
cmake --build build_editor --target PyrosBuilder
cd build_editor/editor && ./PyrosBuilder
```

`BUILD_EDITOR` requires an OpenGL window context (`CONTEXT=SDL2`, the default
for `PYROS_GRAPHICS=OpenGL`). The UI is drawn by `imgui_impl_opengl3` and
`Editor::DrawUI()` issues `glClear`/`glViewport` directly, so the Vulkan and
Metal contexts are rejected at configure time rather than failing at runtime.

CMake symlinks `assets/` and `resources/shaders` next to the binary, so both
are picked up from the source tree without a rebuild.

## What this folder deliberately does not contain

The standalone editor carried its own copy of ImGui, its own glad, its own
`SDL2Context`, and a nested checkout of the engine. All four have been dropped:
ImGui and glad come from the engine (`src/Pyros3D/Ext`), and the window context
is the same `examples/WindowManagers/SDL2/SDL2Context.cpp` the examples use.
Keeping private copies is what let the editor drift ~200 commits behind the
engine until it no longer compiled.

Only the files that are actually reachable were carried over — the old
`OpenDir`/`ReadDirectory` file dialog was left behind (its only call site is
commented out), as were the duplicated engine shaders and the second copy of
`IUInterface`.

## Layout

Panels are top-level ImGui windows docked into the `Main` host window's
dockspace. `View > Default Layout` rebuilds the arrangement (Scene Tree |
Scene View | Tools over Properties, Log across the bottom) via DockBuilder;
otherwise whatever the user last arranged is restored from `imgui.ini` in the
build directory.

## Gizmo rendering

`libgizmo` draws through its own small unlit program (`GizmoTransformRender.cpp`)
rather than through `GenericShaderMaterial`. The engine moved material scalars
and transform matrices into uniform blocks, so `glGetUniformLocation("uColor")`
and friends all return -1 — the gizmo's hand-rolled `Shader::SendUniform()`
calls were silently doing nothing and it inherited whatever the previously
drawn object had left in those blocks. It also draws from a VAO + VBO, since a
core profile has no client-side vertex arrays.

## Assets

All textures are PNG. The editor originally shipped `.dds` versions of the
light/gameobject/selection/icon textures and they loaded fine at the time, but
`Texture::LoadDDS` was removed from the engine in 5321eb4, so a `.dds` now falls
through to stb_image, which cannot decode it. The PNG sources were already
alongside the DDS files, so they are simply what ships.

## Known limitations

Shadow defaults are a starting point, not tuned values. A light with "Cast
Shadows" ticked exposes Shadow Bias, Map Size, Range and (directional only)
Cascades in the Properties panel, all seeded from the light itself — bias
applies on the next shadow pass, the rest rebuild the shadow map. The defaults
(5/3 bias, 2048², 0.01–50, one cascade) showed heavy acne in a synthetic test
scene, so expect to tune them against real content.

Scene files are JSON via the engine's `SceneSerializer`. `File > New / Open /
Save / Save As` are wired; Open and Save As take a typed path rather than
offering a file browser, because the editor has no file-listing widget (the
original one was never finished).

Two things to know about how that interacts with this editor:

* The grid, the cameras and the helper icons live in the *same* `SceneGraph`
  as user content, and `SaveScene()` writes every GameObject in the graph. So
  they are detached around a save/load and re-attached afterwards - otherwise
  they end up in the file and are duplicated on every load.
* A loaded scene is invisible to the tree, selection and gizmos until it has
  been registered with the editor's own id/name/parent bookkeeping, which the
  serializer knows nothing about. `SceneObjects::Adopt()` walks what was
  loaded and does that. Component entries are named generically ("Mesh") -
  the primitive shape is in the file but is not recoverable from the live
  Renderable.
