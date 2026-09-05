# PyrosBuilder — the scene editor.
#
# Built via the root CMakeLists with -DBUILD_EDITOR=ON; it inherits
# PYROS_CONTEXT / IMGUI_DIR / GLAD_INCLUDE_DIR / the dependency variables
# from the parent, exactly like examples/ does. It deliberately vendors
# nothing of its own: ImGui, glad and the SDL2 window manager all come from
# the engine tree (the standalone editor repo this was revived from carried
# private copies of all three, which is what made it drift out of sync).

cmake_minimum_required(VERSION 3.11)

if (NOT DEFINED PYROS_CONTEXT)
	message(FATAL_ERROR
		"editor/ must be built via the root CMakeLists with -DBUILD_EDITOR=ON "
		"(it inherits PYROS_GRAPHICS / CONTEXT / OPENGL_VERSION from the parent).")
endif()

# Builds on OpenGL, Vulkan and Metal. The ImGui backend, the frame-end draw
# and the scene viewport's texture handle are all selected per context (see
# Editor.h/Editor.cpp); libgizmo is the one part still GL-only and is compiled
# out elsewhere rather than blocking the build.
set(PYROS_EDITOR_GL_CONTEXT OFF)

# libcurl for the AI Assistant panel's HTTPS provider calls (OpenAI /
# Anthropic / any OpenAI-compatible endpoint). The macOS SDK and the Windows
# SDK both ship it; Linux needs libcurl dev packages.
# libcurl is only the AI Assistant's HTTP transport, and there is no libcurl
# under emscripten - a browser makes requests through fetch(). Optional rather
# than required so the editor can be built for the web at all; the assistant
# compiles out there (see PYROS_EDITOR_NO_CURL).
if (EMSCRIPTEN)
	set(PYROS_EDITOR_HAVE_CURL OFF)
	message(STATUS "  Editor: libcurl unavailable (emscripten) - AI Assistant disabled")
else()
	find_package(CURL REQUIRED)
	set(PYROS_EDITOR_HAVE_CURL ON)
endif()
if (PYROS_CONTEXT STREQUAL "SDL2" OR PYROS_CONTEXT STREQUAL "SDL" OR PYROS_CONTEXT STREQUAL "SFML")
	set(PYROS_EDITOR_GL_CONTEXT ON)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/PyrosWindowContext.cmake)

set(EDITOR_SOURCE
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AxisHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/SceneEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/SceneEditOps.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/SceneCommands.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UndoStack.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AssetCommands.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/SceneObjects.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ProjectManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AgentServer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/EditorDebugDraw.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CodeEditorDocument.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/MaterialGraphTypes.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/MaterialCodegen.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/MaterialEditorDocument.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/MaterialPreview.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AnimationEditorDocument.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AnimationPreview.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/AnimationEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Character2DDocument.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Character2DPreview.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/Character2DEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/WebTextInput.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/GameObjectHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/LightHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/SoundHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/ParticleHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/IUInterface.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/OpenDir.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/MaterialEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Utils/ReadDirectory.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/UISettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/RenderTargetsTab.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AIAssistant.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit/TextEditor.cpp
)

# Backend-agnostic since it draws through DebugRenderer.
set(LIBGIZMO_SOURCE
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/libgizmo/GizmoTransformMove.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/libgizmo/GizmoTransformRender.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/libgizmo/GizmoTransformRotate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/libgizmo/GizmoTransformScale.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/libgizmo/ZBaseMaths.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/libgizmo/ZMathsFunc.cpp
)

add_executable(PyrosBuilder
	${EDITOR_SOURCE}
	${LIBGIZMO_SOURCE}
	# std::string overloads for ImGui::InputText (SceneEditor's name fields).
	${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
	${PYROS_IMGUI_BACKEND_SOURCE}
	${PYROS_WINDOW_MANAGER}
)

target_include_directories(PyrosBuilder PRIVATE
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit
	# WindowManagers/ (not .../SDL2) so Editor.h's "SDL2/SDL2Context.h"
	# resolves to the same context the examples use.
	${CMAKE_SOURCE_DIR}/examples/WindowManagers
	# Prefab resolution, shared with the player and deliberately outside
	# the engine (see shared/PrefabResolver.h).
	${CMAKE_SOURCE_DIR}/shared
	${SDL2_INCLUDE_DIRS}
	${IMGUI_INCLUDE_DIRS}
	${GLAD_INCLUDE_DIR}
	${CURL_INCLUDE_DIRS}
)

# The emscripten SDL2/Freetype ports supply their own headers, and a target
# only sees them if it is compiled with the port flags - which is why the
# engine's own targets call this. Without it the editor cannot even find
# ft2build.h.
if (EMSCRIPTEN)
	pyros_emscripten_apply_compile(PyrosBuilder)
	# ...and the LINK flags. -sUSE_SDL=2 is what actually pulls the SDL2 port's
	# library in; with only the compile flags every SDL_* call compiled fine
	# and then failed at link as an undefined symbol.
	# .js, not .html: editor/web/index.html is the page, staged below.
	pyros_emscripten_apply_link_no_shell(PyrosBuilder)

	# The browser has no filesystem, so everything the editor opens at startup
	# has to be baked into the virtual one: its own icons and fonts, and the
	# engine's shaders. Mounted at the paths the code already uses, so nothing
	# in the editor needs a web-specific asset path.
	#
	# `projects` is deliberately NOT preloaded - a project is the user's, not
	# ours, and shipping one would make the web build open somebody else's
	# work on startup. It is created in IDBFS at runtime instead.
	target_link_options(PyrosBuilder PRIVATE
		"SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@assets"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/resources/shaders@shaders"
		# The class library every LuaComponent script is written against.
		# Just this one file, not all of examples/assets: that tree is demo
		# content (models, textures, whole scenes) the editor has no use for,
		# and merging it into /assets would collide with the editor's own.
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/examples/assets/middleclass.lua@assets/middleclass.lua"

		# Emscripten's default stack is 64 KB. A desktop thread gets 8 MB, and
		# the editor is written for that: ImGui frames, the scene walks, the
		# JSON snapshots the undo stack takes. Overflowing it does not say
		# "stack overflow" - the guard page is not there - it comes out as
		# "memory access out of bounds" from whatever wrote past the end,
		# which is what this build did on startup.
		"SHELL:-sSTACK_SIZE=8MB"
		# Start with enough heap that the first frame does not spend its time
		# growing it. It still grows (ALLOW_MEMORY_GROWTH) - this is only the
		# starting point.
		"SHELL:-sINITIAL_MEMORY=256MB"
		# So the NEXT one of these reports what it actually is rather than an
		# address. Costs a little speed; a browser build that crashes silently
		# costs a lot more.
		"SHELL:-sASSERTIONS=1"
		# The page calls into the module for on-screen keyboard input
		# (WebTextInput.cpp); without ccall/cwrap exported those calls are
		# undefined at runtime, which shows up as a keyboard that appears and
		# types nothing.
		"SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
	)

	# Diagnosing a wasm crash without this is reading addresses. -g2 keeps the
	# name section so a browser stack shows functions; SAFE_HEAP turns
	# "memory access out of bounds" into the actual bad address and size.
	# Both cost speed, which is why they are opt-in rather than always on.
	option(PYROS_WEB_DEBUG "Web editor: symbol names + heap checking" OFF)
	# SAFE_HEAP says "there was a bad access"; ASan says which allocation, how
	# big, and where it was freed. They cannot be combined, so this is a
	# separate mode rather than a louder version of the same one.
	option(PYROS_WEB_ASAN "Web editor: AddressSanitizer instead of SAFE_HEAP" OFF)
	if (PYROS_WEB_ASAN)
		target_compile_options(PyrosBuilder PRIVATE "SHELL:-g2" "SHELL:-fsanitize=address")
		target_link_options(PyrosBuilder PRIVATE
			"SHELL:-g2"
			"SHELL:-fsanitize=address"
			"SHELL:-sASSERTIONS=2"
			# ASan needs room of its own for the shadow map on top of the heap.
			"SHELL:-sINITIAL_MEMORY=1GB"
		)
		message(STATUS "  Editor(web): PYROS_WEB_ASAN on - AddressSanitizer (very slow)")
	elseif (PYROS_WEB_DEBUG)
		target_compile_options(PyrosBuilder PRIVATE "SHELL:-g2")
		target_link_options(PyrosBuilder PRIVATE
			"SHELL:-g2"
			"SHELL:-sASSERTIONS=2"
			"SHELL:-sSAFE_HEAP=1"
			"SHELL:-sSTACK_OVERFLOW_CHECK=2"
			# GL_ASSERTIONS validates every GL call and names what was wrong
			# with it; GL_DEBUG logs the calls themselves. This is the
			# instrument for "the calls all succeed and nothing appears",
			# which no amount of reading the shader will answer.
			"SHELL:-sGL_ASSERTIONS=1"
			"SHELL:-sGL_DEBUG=1"
		)
		message(STATUS "  Editor(web): PYROS_WEB_DEBUG on - symbols + SAFE_HEAP (slow)")
	endif()

	# Our own page, not emscripten's default shell: that one puts the canvas in
	# a fixed-size box with a textarea under it, which is fine for a demo and
	# useless for an editor. index.html + editor-shell.js give it the whole
	# viewport, handle resize (in device pixels) and fullscreen.
	#
	# Named PyrosBuilder.js, so index.html's <script src> resolves beside it.
	add_custom_command(TARGET PyrosBuilder POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
			$<TARGET_FILE_DIR:PyrosBuilder>/index.html
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/web/editor-shell.js
			$<TARGET_FILE_DIR:PyrosBuilder>/editor-shell.js
		COMMENT "PyrosBuilder: staging the web page beside the module"
	)
endif()

# The assistant's whole implementation is libcurl; without it the file
# compiles to nothing rather than the editor failing to link.
if (NOT PYROS_EDITOR_HAVE_CURL)
	target_compile_definitions(PyrosBuilder PRIVATE PYROS_EDITOR_NO_CURL)
endif()

target_link_libraries(PyrosBuilder PRIVATE
	PyrosEngine
	${PYROS_CONTEXT_LIB}
	${OPENGL_LIBS}
	${BOX3D_LIBRARIES}
	${FREETYPE_LIBRARIES}
	${LUA_LIBRARIES}
	${CURL_LIBRARIES}
)

target_compile_definitions(PyrosBuilder PRIVATE ${PYROS_CONTEXT_DEFINITION})

# Same EXAMPLES_PATH trick DemoLauncher uses — middleclass.lua + demo scripts.
target_compile_definitions(PyrosBuilder PRIVATE
	# Empty on the web: PYROS_ASSET_ROOT is an absolute path on the BUILD
	# machine, which means nothing inside a browser's virtual filesystem.
	# Left empty, the one place that uses it resolves to
	# "/assets/middleclass.lua" - which is where the preload below puts it.
	$<IF:$<BOOL:${EMSCRIPTEN}>,PYROS_EXAMPLES_PATH=,PYROS_EXAMPLES_PATH=${PYROS_ASSET_ROOT}>
)

# DebugRenderer now follows the same pipeline + UBO path as Forward/
# PostEffects (CreatePipeline/BindPipeline, shared GlobalMatrices/
# ObjectMatrix/MaterialUniforms UBOs, DrawArrays with VAO VBOs), so gizmos
# and bounds work on OpenGL, Vulkan and Metal.
target_compile_definitions(PyrosBuilder PRIVATE PYROS_EDITOR_HAS_DEBUG_DRAW)

if (IS_DESKTOP)
	target_compile_definitions(PyrosBuilder PRIVATE DESKTOP)
endif()

if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
	target_compile_definitions(PyrosBuilder PRIVATE _IMPORT)
endif()

# The editor loads "assets/..." and the engine loads "shaders/..." relative
# to the working directory — stage both next to the binary (same trick as
# examples/, so edits to either are picked up without a rebuild).
pyros_stage_dir(${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
pyros_stage_dir(${CMAKE_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)

# Ship AssimpImporter next to PyrosBuilder so Import Model can find it.
if (TARGET AssimpImporter)
	add_dependencies(PyrosBuilder AssimpImporter)
	add_custom_command(TARGET PyrosBuilder POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:AssimpImporter>
			$<TARGET_FILE_DIR:PyrosBuilder>
		COMMENT "Copy AssimpImporter next to PyrosBuilder")
endif()
