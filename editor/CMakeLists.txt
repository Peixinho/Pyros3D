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
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/GameObjectHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/LightHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/SoundHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Helpers/ParticleHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/IUInterface.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/OpenDir.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/MaterialEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/Utils/ReadDirectory.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/editor/UI/UISettings.cpp
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
	${SDL2_INCLUDE_DIRS}
	${IMGUI_INCLUDE_DIRS}
	${GLAD_INCLUDE_DIR}
)

target_link_libraries(PyrosBuilder PRIVATE
	PyrosEngine
	${PYROS_CONTEXT_LIB}
	${OPENGL_LIBS}
	${BOX3D_LIBRARIES}
	${FREETYPE_LIBRARIES}
	${LUA_LIBRARIES}
)

target_compile_definitions(PyrosBuilder PRIVATE ${PYROS_CONTEXT_DEFINITION})

# Same EXAMPLES_PATH trick DemoLauncher uses — middleclass.lua + demo scripts.
target_compile_definitions(PyrosBuilder PRIVATE
	PYROS_EXAMPLES_PATH=${CMAKE_SOURCE_DIR}/examples
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
# to the working directory — symlink both next to the binary (same trick as
# examples/, so edits to either are picked up without a rebuild).
if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/assets)
	file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets SYMBOLIC)
endif()
if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/shaders)
	file(CREATE_LINK ${CMAKE_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders SYMBOLIC)
endif()

# Ship AssimpImporter next to PyrosBuilder so Import Model can find it.
if (TARGET AssimpImporter)
	add_dependencies(PyrosBuilder AssimpImporter)
	add_custom_command(TARGET PyrosBuilder POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:AssimpImporter>
			$<TARGET_FILE_DIR:PyrosBuilder>
		COMMENT "Copy AssimpImporter next to PyrosBuilder")
endif()
