# AssimpImporter — converts .obj/.dae/.fbx/... to the engine's .p3dm/.p3da.
#
# Built via the root CMakeLists with -DBUILD_CONVERTER=ON, and it inherits
# the dependency variables from the parent exactly like examples/ and
# editor/ do. It used to carry its own project() plus a second copy of the
# OPENGL_VERSION/STATIC_LIB/Lua option handling, none of which agreed with
# cmake/PyrosOptions.cmake, alongside a hardcoded /usr/local prefix and
# link_directories() - which is what kept it Unix-only.
#
# The tool itself only ever includes Pyros3D headers and assimp; freetype,
# Lua, Box3D and OpenGL all arrive transitively through PyrosEngine's PUBLIC
# link interface, so none of them are named here.

cmake_minimum_required(VERSION 3.11)

if (NOT TARGET PyrosEngine)
	message(FATAL_ERROR
		"tools/AssimpImporter must be built via the root CMakeLists with "
		"-DBUILD_CONVERTER=ON (it links the PyrosEngine target).")
endif()

find_package(assimp REQUIRED)

add_executable(AssimpImporter
	${CMAKE_CURRENT_SOURCE_DIR}/src/AssimpAnimationImporter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/AssimpModelImporter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/MainProgram.cpp
)

target_include_directories(AssimpImporter PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

# assimp 5.x exports the assimp::assimp target; older installs (and some
# distro packages) only set ASSIMP_LIBRARIES/ASSIMP_INCLUDE_DIRS.
if (TARGET assimp::assimp)
	target_link_libraries(AssimpImporter PRIVATE PyrosEngine assimp::assimp)
else()
	target_link_libraries(AssimpImporter PRIVATE PyrosEngine ${ASSIMP_LIBRARIES})
	target_include_directories(AssimpImporter PRIVATE ${ASSIMP_INCLUDE_DIRS})
endif()

if (IS_DESKTOP)
	target_compile_definitions(AssimpImporter PRIVATE DESKTOP)
endif()

# Same dllimport switch the demos and the editor use — PYROS3D_API has to
# resolve to __declspec(dllimport) on the consuming side of the DLL.
if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
	target_compile_definitions(AssimpImporter PRIVATE _IMPORT)
endif()
