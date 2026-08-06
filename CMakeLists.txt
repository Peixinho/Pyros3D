cmake_minimum_required(VERSION 3.11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Pyros3D)
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Options → dependencies → GL/Vulkan/ImGui → engine sources
include(PyrosOptions)
include(PyrosDependencies)
include(PyrosBackend)
include(PyrosSources)

# ---------------------------------------------------------------------------
# PyrosEngine library
# ---------------------------------------------------------------------------
add_library(PyrosEngine ${LIB_TYPE} ${PYROS_ENGINE_SOURCE})

target_include_directories(PyrosEngine
	PUBLIC
		${CMAKE_SOURCE_DIR}/include
		${IMGUI_INCLUDE_DIRS}
		${LUA_INCLUDE_DIR}
		${FREETYPE_INCLUDE_DIRS}
		${BULLET_INCLUDE_DIRS}
		${GLAD_INCLUDE_DIR}
	PRIVATE
		${OPENGL_INCLUDE_DIRS}
		${SPIRV_TOOLING_INCLUDE_DIRS}
)

target_link_libraries(PyrosEngine
	PUBLIC
		${OPENGL_LIBS}
		${BULLET_LIBRARIES}
		${FREETYPE_LIBRARIES}
		${LUA_LIBRARIES}
		${SPIRV_TOOLING_LIBS}
		${VULKAN_BACKEND_LIBS}
		${AUDIO_LIBS}
)

# PYROS3D_API (Export.h): dllexport when building the shared DLL on Windows.
if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
	target_compile_definitions(PyrosEngine PRIVATE _EXPORT)
endif()

if (EMSCRIPTEN)
	pyros_emscripten_apply_compile(PyrosEngine)
endif()

# ---------------------------------------------------------------------------
# Optional tools / demos
# ---------------------------------------------------------------------------
if (BUILD_CONVERTER)
	add_subdirectory(${CMAKE_SOURCE_DIR}/tools/AssimpImporter)
endif()

if (BUILD_DEMOS)
	add_subdirectory(${CMAKE_SOURCE_DIR}/examples)
endif()
