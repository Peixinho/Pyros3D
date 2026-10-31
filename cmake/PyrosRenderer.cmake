# ---------------------------------------------------------------------------
# The renderer: the device layer (include/Pyros3D/Rendering/Device) and the renderer on it
# (include/Pyros3D/Rendering/Renderer).
#
# Its shaders (resources/shaders) are compiled to SPIR-V here, when the engine is
# built, and embedded in it: nothing compiles a shader at run time.
# Its devices: Metal (that SPIR-V through SPIRV-Cross) and WebGPU (Dawn natively - that SPIR-V to WGSL through
# Tint, cmake/PyrosWebGPU.cmake). Either is enough; with both, PYROS_GFX_BACKEND=webgpu picks WebGPU.
# ---------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/cmake/PyrosWebGPU.cmake)
function(pyros_add_renderer target)
	set(RENDERER_METAL OFF)
	if (METAL_BACKEND_FOUND AND SPIRV_CROSS_MSL_LIBRARY)
		set(RENDERER_METAL ON)
	endif()
	pyros_add_webgpu(${target} RENDERER_WEBGPU)
	set(RENDERER_VULKAN OFF)
	if (BUILD_VULKAN_BACKEND AND TARGET volk::volk)
		set(RENDERER_VULKAN ON)
	endif()
	if (NOT RENDERER_METAL AND NOT RENDERER_WEBGPU AND NOT RENDERER_VULKAN)
		message(STATUS "Renderer: off (it needs Metal with spirv-cross-msl, WebGPU - Dawn and Tint - or Vulkan)")
		return()
	endif()
	find_program(GLSLC_EXECUTABLE glslc REQUIRED)

	set(src ${CMAKE_SOURCE_DIR}/resources/shaders)
	set(out ${CMAKE_BINARY_DIR}/generated/shaders)
	file(MAKE_DIRECTORY ${out})
	file(GLOB RENDERER_SHADER_INCLUDES ${src}/*.glsl)
	set(RENDERER_SHADERS surface.vert surface.frag fullscreen.vert tonemap.frag shadow.vert depthonly.frag transmittance.frag skyview.frag sky.frag skysh.frag depth.vert gtao.frag aoblur.frag skyshsum.frag bloomdown.frag bloomup.frag luminance.frag reduce4.frag adapt.frag
		surface.vert@INSTANCED depth.vert@INSTANCED shadow.vert@INSTANCED depth.vert@ALPHA depth.vert@INSTANCED,ALPHA depthalpha.frag alphacut.frag surface.frag@NOCUT post.frag
		shadow.vert@ALPHA shadow.vert@INSTANCED,ALPHA shadowalpha.frag surface.vert@SKINNED depth.vert@SKINNED shadow.vert@SKINNED shadow.vert@SKINNED,ALPHA particle.vert particle.frag particle_sim.comp rtshadow.frag rtreflect.frag ui.vert ui.frag clouds.frag clouds.frag@DOME cloudshadow.frag taa.frag velocity.frag novelocity.frag particle.frag@HALF particlecomposite.frag skybox.frag skyboxtable.frag imgui.vert imgui.frag overlay.vert overlay.frag surface.vert@VERTEX_COLOR probefilter.frag)
	set(RENDERER_SPIRV "")
	# (a permutation is the same source with defines: "file@A,B" -> file.A.B.inc, compiled with -DA -DB)
	foreach(entry ${RENDERER_SHADERS})
		string(FIND "${entry}" "@" at)
		if (at EQUAL -1)
			set(s ${entry})
			set(name ${entry})
			set(defs "")
		else()
			string(SUBSTRING "${entry}" 0 ${at} s)
			math(EXPR after "${at} + 1")
			string(SUBSTRING "${entry}" ${after} -1 defines)
			string(REPLACE "," ";" defineList "${defines}")
			string(REPLACE "," "." suffix "${defines}")
			set(name ${s}.${suffix})
			set(defs "")
			foreach(d ${defineList})
				list(APPEND defs -D${d})
			endforeach()
		endif()
		add_custom_command(
			OUTPUT ${out}/${name}.inc
			COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.1 -O -mfmt=num ${defs} -I ${src} -o ${out}/${name}.inc ${src}/${s}
			DEPENDS ${src}/${s} ${RENDERER_SHADER_INCLUDES}
			COMMENT "The renderer shader ${name}"
			VERBATIM)
		list(APPEND RENDERER_SPIRV ${out}/${name}.inc)
		if (EMSCRIPTEN AND RENDERER_WEBGPU)
			# (the browser's: its WGSL too, made here by the native tool - the page turns no SPIR-V into WGSL)
			add_custom_command(
				OUTPUT ${out}/${name}.wgsl.inc
				COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.1 -O ${defs} -I ${src} -o ${out}/${name}.spv ${src}/${s}
				COMMAND ${PYROS_SPIRV2WGSL} ${out}/${name}.spv ${out}/${name}.wgsl.inc
				DEPENDS ${src}/${s} ${RENDERER_SHADER_INCLUDES}
				COMMENT "The renderer shader ${name} (WGSL)"
				VERBATIM)
			list(APPEND RENDERER_SPIRV ${out}/${name}.wgsl.inc)
		endif()
	endforeach()
	# (the shaders that trace with the GPU's ray tracing - GL_EXT_ray_query: SPIR-V 1.4, Vulkan 1.2's - for the native
	# backends that have it (Metal, Vulkan); no WGSL: WebGPU has no ray queries, it traces with rtshadow/rtreflect's own BVH)
	if (NOT EMSCRIPTEN)
		foreach(entry rtshadow.frag@RAYQUERY rtreflect.frag@RAYQUERY)
			string(FIND "${entry}" "@" at)
			string(SUBSTRING "${entry}" 0 ${at} s)
			math(EXPR after "${at} + 1")
			string(SUBSTRING "${entry}" ${after} -1 define)
			set(name ${s}.${define})
			add_custom_command(
				OUTPUT ${out}/${name}.inc
				COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.2 -O -mfmt=num -D${define} -I ${src} -o ${out}/${name}.inc ${src}/${s}
				DEPENDS ${src}/${s} ${RENDERER_SHADER_INCLUDES}
				COMMENT "The renderer shader ${name} (ray queries)"
				VERBATIM)
			list(APPEND RENDERER_SPIRV ${out}/${name}.inc)
		endforeach()
	endif()
	# (the sources a material's surface is compiled against at run time, as text)
	set(RENDERER_EMBEDDED common.glsl surface_draw.glsl instancing.glsl skinning.glsl wind.glsl lighting.glsl shadows.glsl skylight.glsl atmosphere.glsl surface.frag surface.vert)
	set(RENDERER_EMBEDDED_DEPS "")
	foreach(f ${RENDERER_EMBEDDED})
		list(APPEND RENDERER_EMBEDDED_DEPS ${src}/${f})
	endforeach()
	string(REPLACE ";" "," RENDERER_EMBEDDED_ARG "${RENDERER_EMBEDDED}")
	add_custom_command(
		OUTPUT ${out}/sources.inc
		COMMAND ${CMAKE_COMMAND} -DOUTPUT=${out}/sources.inc -DDIR=${src} "-DFILES=${RENDERER_EMBEDDED_ARG}" -P ${CMAKE_SOURCE_DIR}/cmake/EmbedShaderSources.cmake
		DEPENDS ${RENDERER_EMBEDDED_DEPS} ${CMAKE_SOURCE_DIR}/cmake/EmbedShaderSources.cmake
		COMMENT "The renderer shader sources (for materials compiled at run time)"
		VERBATIM)
	list(APPEND RENDERER_SPIRV ${out}/sources.inc)
	add_custom_target(PyrosShaders DEPENDS ${RENDERER_SPIRV})
	add_dependencies(${target} PyrosShaders)

	set(RENDERER_SOURCE
		${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/SceneRenderer.cpp
		${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/FrameGraph.cpp
		${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/ImGuiRenderer.cpp
		${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/SceneSettings.cpp
		${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/SceneView.cpp
		${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/RendererLua.cpp)
	# (glslang: the materials' surfaces compiled at run time - PYROS_GLSLANG_DIR, else the system's. Without it - the
	# browser - the surfaces come from the surface cache a native run wrote: SceneRenderer::SetSurfaceCache)
	set(RENDERER_GLSLANG "without a GLSL compiler: surfaces from the cache")
	if (NOT EMSCRIPTEN)
		find_path(GLSLANG_C_INCLUDE glslang/Include/glslang_c_interface.h HINTS ${PYROS_GLSLANG_DIR}/include /opt/homebrew/include /usr/local/include)
		find_library(RENDERER_GLSLANG_LIBRARY NAMES glslang HINTS ${PYROS_GLSLANG_DIR}/lib /opt/homebrew/lib /usr/local/lib)
		find_library(RENDERER_GLSLANG_LIMITS_LIBRARY NAMES glslang-default-resource-limits HINTS ${PYROS_GLSLANG_DIR}/lib /opt/homebrew/lib /usr/local/lib)
		if (GLSLANG_C_INCLUDE AND RENDERER_GLSLANG_LIBRARY AND RENDERER_GLSLANG_LIMITS_LIBRARY)
			list(APPEND RENDERER_SOURCE ${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/GlslToSpirv.cpp)
			target_include_directories(${target} PRIVATE ${GLSLANG_C_INCLUDE})
			target_link_libraries(${target} PRIVATE ${RENDERER_GLSLANG_LIBRARY} ${RENDERER_GLSLANG_LIMITS_LIBRARY})
			target_compile_definitions(${target} PRIVATE PYROS_RENDERER_GLSLANG)
			set(RENDERER_GLSLANG "glslang")
		endif()
	endif()
	set(RENDERER_DEVICES "")
	if (RENDERER_METAL)
		list(APPEND RENDERER_SOURCE ${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/MetalDevice.mm)
		set_source_files_properties(${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/MetalDevice.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
		target_compile_definitions(${target} PUBLIC PYROS_GFX_METAL)
		list(APPEND RENDERER_DEVICES Metal)
	endif()
	if (RENDERER_WEBGPU)
		list(APPEND RENDERER_DEVICES WebGPU)
	endif()
	if (RENDERER_VULKAN)
		# (its SPIR-V as it is)
		list(APPEND RENDERER_SOURCE ${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/VulkanDevice.cpp)
		# (and the platform's surface: a CAMetalLayer through MoltenVK, a window's HWND, an X11 window)
		if (APPLE)
			set(RENDERER_VK_PLATFORM VK_USE_PLATFORM_METAL_EXT)
		elseif (WIN32)
			set(RENDERER_VK_PLATFORM "VK_USE_PLATFORM_WIN32_KHR;NOMINMAX;WIN32_LEAN_AND_MEAN")
		else()
			set(RENDERER_VK_PLATFORM VK_USE_PLATFORM_XLIB_KHR)
		endif()
		set_source_files_properties(${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/VulkanDevice.cpp PROPERTIES COMPILE_DEFINITIONS "${RENDERER_VK_PLATFORM}")
		target_compile_definitions(${target} PUBLIC PYROS_GFX_VULKAN)
		list(APPEND RENDERER_DEVICES Vulkan)
	endif()
	target_sources(${target} PRIVATE ${RENDERER_SOURCE})
	target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/generated)
	string(REPLACE ";" ", " RENDERER_DEVICES "${RENDERER_DEVICES}")
	message(STATUS "Renderer: on (${RENDERER_DEVICES}; ${RENDERER_GLSLANG})")
endfunction()
