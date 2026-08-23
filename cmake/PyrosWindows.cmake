# MSVC/Windows-wide settings. Included from the root CMakeLists *before*
# anything else, because these have to reach box3d and the vendored Ext
# sources too (add_subdirectory / add_compile_* only affect targets created
# after the call).

if (NOT WIN32)
	return()
endif()

# Windows.h defines min/max as macros, which collide with std::min/std::max -
# used all over the editor and the engine - and drags in a large surface of
# rarely-needed headers. Both defines are the standard remedy and are safe
# for the few translation units that include Windows.h deliberately
# (ReadDirectory.h, SceneObjects.cpp, AgentServer.cpp).
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

# MSVC's <cmath> only defines M_PI when _USE_MATH_DEFINES is set; two call
# sites rely on it (RenderingInstancedComponent.cpp, SceneEditor.cpp).
add_compile_definitions(_USE_MATH_DEFINES)

# The engine predates the _s variants and uses fopen/getenv/sscanf directly.
add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)

if (MSVC)
	# sol.hpp, imgui and VulkanRenderDevice.cpp all blow past the 65279
	# section limit of a normal .obj.
	add_compile_options(/bigobj)
	# Sources are UTF-8 (imgui's and the editor's string literals included);
	# without this MSVC reads them in the system codepage.
	add_compile_options(/utf-8)
endif()

# PYROS3D_API (include/Pyros3D/Other/Export.h) is applied class-by-class and
# does not cover every symbol the editor and the demos pull out of the DLL.
# Let CMake generate a .def exporting everything as well, so a SHARED build
# links without having to annotate the entire engine first. Harmless for
# STATIC builds - CMake ignores it there.
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON CACHE BOOL "Auto-export all DLL symbols on Windows")
