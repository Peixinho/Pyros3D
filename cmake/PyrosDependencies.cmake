# Third-party find_package / brew helpers. Expects PyrosOptions.cmake loaded.

# ---------------------------------------------------------------------------
# Debug
# ---------------------------------------------------------------------------
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_definitions(_DEBUG)
endif()

if (IS_DESKTOP)
	add_compile_definitions(DESKTOP)
endif()

# ---------------------------------------------------------------------------
# Freetype
# ---------------------------------------------------------------------------
if (EMSCRIPTEN)
	# Provided by -sUSE_FREETYPE=1 (see PyrosEmscripten.cmake).
	set(FREETYPE_FOUND TRUE)
	set(FREETYPE_INCLUDE_DIRS "")
	set(FREETYPE_LIBRARIES "")
elseif (ANDROID)
	# No reliable system FreeType in the NDK — fetch a static build.
	include(FetchContent)
	set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
	FetchContent_Declare(freetype
		GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
		GIT_TAG VER-2-13-2
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(freetype)
	set(FREETYPE_FOUND TRUE)
	set(FREETYPE_INCLUDE_DIRS "")
	set(FREETYPE_LIBRARIES freetype)
	message(STATUS "FreeType: FetchContent (Android)")
else()
	find_package(Freetype REQUIRED)
	if (NOT FREETYPE_FOUND)
		message(FATAL_ERROR "Freetype2 not found")
	endif()
endif()

# ---------------------------------------------------------------------------
# Box3D (submodule: src/Pyros3D/Ext/box3d @ v0.1.0)
# ---------------------------------------------------------------------------
set(BOX3D_DIR ${CMAKE_SOURCE_DIR}/src/Pyros3D/Ext/box3d)
if (NOT EXISTS "${BOX3D_DIR}/CMakeLists.txt")
	message(FATAL_ERROR
		"Box3D submodule missing at ${BOX3D_DIR}. "
		"Run: git submodule update --init --recursive")
endif()
set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX3D_VALIDATE OFF CACHE BOOL "" FORCE)
if (EMSCRIPTEN)
	set(BOX3D_DISABLE_SIMD ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(${BOX3D_DIR} ${CMAKE_BINARY_DIR}/_deps/box3d)
set(BOX3D_INCLUDE_DIRS ${BOX3D_DIR}/include)
set(BOX3D_LIBRARIES box3d)
message(STATUS "Box3D: submodule ${BOX3D_DIR}")

# ---------------------------------------------------------------------------
# Lua (optional)
# ---------------------------------------------------------------------------
if (HAVE_LUA_BINDINGS)
	add_compile_definitions(LUA_BINDINGS)

	if (EMSCRIPTEN)
		include(FetchContent)
		FetchContent_Declare(lua54
			URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
		)
		FetchContent_GetProperties(lua54)
		if (NOT lua54_POPULATED)
			FetchContent_Populate(lua54)
		endif()
		if (NOT TARGET lua54)
			set(LUA_SRC ${lua54_SOURCE_DIR}/src)
			add_library(lua54 STATIC
				${LUA_SRC}/lapi.c ${LUA_SRC}/lcode.c ${LUA_SRC}/lctype.c ${LUA_SRC}/ldebug.c
				${LUA_SRC}/ldo.c ${LUA_SRC}/ldump.c ${LUA_SRC}/lfunc.c ${LUA_SRC}/lgc.c
				${LUA_SRC}/llex.c ${LUA_SRC}/lmem.c ${LUA_SRC}/lobject.c ${LUA_SRC}/lopcodes.c
				${LUA_SRC}/lparser.c ${LUA_SRC}/lstate.c ${LUA_SRC}/lstring.c ${LUA_SRC}/ltable.c
				${LUA_SRC}/ltm.c ${LUA_SRC}/lundump.c ${LUA_SRC}/lvm.c ${LUA_SRC}/lzio.c
				${LUA_SRC}/lauxlib.c ${LUA_SRC}/lbaselib.c ${LUA_SRC}/lcorolib.c ${LUA_SRC}/ldblib.c
				${LUA_SRC}/liolib.c ${LUA_SRC}/lmathlib.c ${LUA_SRC}/loadlib.c ${LUA_SRC}/loslib.c
				${LUA_SRC}/lstrlib.c ${LUA_SRC}/ltablib.c ${LUA_SRC}/lutf8lib.c ${LUA_SRC}/linit.c
			)
			target_include_directories(lua54 PUBLIC ${LUA_SRC})
		endif()
		set(LUA_INCLUDE_DIR ${lua54_SOURCE_DIR}/src)
		set(LUA_LIBRARIES lua54)
		set(LUA51_FOUND TRUE)
	else()
		# sol.hpp only supports Lua 5.1–5.4. Unversioned "lua" packages are often
		# 5.5+ now; prefer an explicit 5.4/5.3/5.1 install when present.
		if (APPLE)
			execute_process(COMMAND brew --prefix lua@5.4
				OUTPUT_VARIABLE HOMEBREW_LUA54_PREFIX
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET)
			if (HOMEBREW_LUA54_PREFIX AND EXISTS "${HOMEBREW_LUA54_PREFIX}/include/lua/lua.h")
				set(LUA_INCLUDE_DIR "${HOMEBREW_LUA54_PREFIX}/include/lua" CACHE PATH "Lua include directory")
				set(LUA_LIBRARY "${HOMEBREW_LUA54_PREFIX}/lib/liblua.dylib" CACHE FILEPATH "Lua library")
			endif()
		elseif (UNIX)
			# Prefer versioned distro packages over /usr/include + liblua.so (5.5).
			# Only override when unset or pointing at the unversioned headers.
			if (NOT LUA_INCLUDE_DIR OR LUA_INCLUDE_DIR STREQUAL "/usr/include")
				foreach(_pyros_lua_ver 5.4 5.3 5.1)
					if (EXISTS "/usr/include/lua${_pyros_lua_ver}/lua.h")
						find_library(_pyros_lua_lib
							NAMES lua${_pyros_lua_ver} lua-${_pyros_lua_ver}
							PATHS /usr/lib /usr/lib64
							PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu
						)
						if (_pyros_lua_lib)
							set(LUA_INCLUDE_DIR "/usr/include/lua${_pyros_lua_ver}" CACHE PATH "Lua include directory" FORCE)
							set(LUA_LIBRARY "${_pyros_lua_lib}" CACHE FILEPATH "Lua library" FORCE)
							unset(_pyros_lua_lib CACHE)
							break()
						endif()
						unset(_pyros_lua_lib CACHE)
					endif()
				endforeach()
				unset(_pyros_lua_ver)
			endif()
		endif()

		find_package(Lua51 REQUIRED)
		if (NOT LUA51_FOUND)
			message(FATAL_ERROR "Lua not found (need 5.1–5.4 for sol.hpp)")
		endif()

		# Keep LUA_LIBRARIES in sync with LUA_LIBRARY — a stale cache entry can
		# leave headers on 5.1 while still linking the unversioned 5.5 .so.
		set(_pyros_lua_libs "${LUA_LIBRARY}")
		if (LUA_MATH_LIBRARY)
			list(APPEND _pyros_lua_libs "${LUA_MATH_LIBRARY}")
		endif()
		set(LUA_LIBRARIES "${_pyros_lua_libs}" CACHE STRING "Lua link libraries" FORCE)
		unset(_pyros_lua_libs)

		# Reject Lua 5.5+ even if FindLua51 accepted a mismatched cache.
		if (EXISTS "${LUA_INCLUDE_DIR}/lua.h")
			unset(_pyros_lua_num)
			file(STRINGS "${LUA_INCLUDE_DIR}/lua.h" _pyros_lua_ver_line
				REGEX "^[ \t]*#define[ \t]+LUA_VERSION_NUM")
			if (_pyros_lua_ver_line MATCHES "LUA_VERSION_NUM[ \t]+([0-9]+)")
				set(_pyros_lua_num "${CMAKE_MATCH_1}")
			else()
				# Lua 5.5+: LUA_VERSION_NUM is an expression over MAJOR_N/MINOR_N.
				file(STRINGS "${LUA_INCLUDE_DIR}/lua.h" _pyros_lua_maj
					REGEX "^[ \t]*#define[ \t]+LUA_VERSION_MAJOR_N[ \t]+")
				file(STRINGS "${LUA_INCLUDE_DIR}/lua.h" _pyros_lua_min
					REGEX "^[ \t]*#define[ \t]+LUA_VERSION_MINOR_N[ \t]+")
				if (_pyros_lua_maj MATCHES "MAJOR_N[ \t]+([0-9]+)"
						AND _pyros_lua_min MATCHES "MINOR_N[ \t]+([0-9]+)")
					set(_pyros_lua_maj_n "${CMAKE_MATCH_1}")
					# Second MATCH overwrote CMAKE_MATCH_1 — re-parse minor.
					string(REGEX MATCH "MINOR_N[ \t]+([0-9]+)" _ "${_pyros_lua_min}")
					set(_pyros_lua_min_n "${CMAKE_MATCH_1}")
					string(REGEX MATCH "MAJOR_N[ \t]+([0-9]+)" _ "${_pyros_lua_maj}")
					set(_pyros_lua_maj_n "${CMAKE_MATCH_1}")
					math(EXPR _pyros_lua_num "${_pyros_lua_maj_n} * 100 + ${_pyros_lua_min_n}")
				endif()
			endif()
			if (DEFINED _pyros_lua_num AND (_pyros_lua_num LESS 501 OR _pyros_lua_num GREATER 504))
				message(FATAL_ERROR
					"sol.hpp needs Lua 5.1–5.4, but LUA_INCLUDE_DIR=${LUA_INCLUDE_DIR} "
					"is version ${_pyros_lua_num}. Pass -DLUA_INCLUDE_DIR=.../lua5.4 "
					"(or 5.3/5.1) and matching -DLUA_LIBRARY, or clear the build cache.")
			endif()
			unset(_pyros_lua_ver_line)
			unset(_pyros_lua_maj)
			unset(_pyros_lua_min)
			unset(_pyros_lua_maj_n)
			unset(_pyros_lua_min_n)
			unset(_pyros_lua_num)
		endif()
	endif()
endif()

# ---------------------------------------------------------------------------
# Audio system libs (miniaudio talks to the OS stack)
# ---------------------------------------------------------------------------
if (EMSCRIPTEN)
	# miniaudio uses Web Audio; no extra system libs.
	set(AUDIO_LIBS "")
elseif (ANDROID)
	# miniaudio OpenSL ES / AAudio via NDK; link OpenSLES when present.
	find_library(PYROS_OPENSLES_LIBRARY OpenSLES)
	set(AUDIO_LIBS ${CMAKE_DL_LIBS})
	if (PYROS_OPENSLES_LIBRARY)
		list(APPEND AUDIO_LIBS ${PYROS_OPENSLES_LIBRARY})
	endif()
elseif (APPLE)
	find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
	find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
	find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
	set(AUDIO_LIBS ${COREAUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY} ${AUDIOTOOLBOX_LIBRARY})
elseif (UNIX)
	set(AUDIO_LIBS ${CMAKE_DL_LIBS} pthread m)
else()
	set(AUDIO_LIBS "")
endif()

# ---------------------------------------------------------------------------
# SDL2 on Android — required by File.cpp (SDL_RWops) even when BUILD_DEMOS=OFF.
# Supply via Gradle (SDL2::SDL2 already defined) or:
#   -DSDL2_INCLUDE_DIR=... -DSDL2_LIBRARY=...
# ---------------------------------------------------------------------------
if (ANDROID)
	if (TARGET SDL2::SDL2)
		# Parent project / earlier find already defined it.
	elseif (SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
		add_library(SDL2::SDL2 UNKNOWN IMPORTED)
		set_target_properties(SDL2::SDL2 PROPERTIES
			IMPORTED_LOCATION "${SDL2_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
		)
	else()
		message(FATAL_ERROR
			"Android build requires SDL2. Pass -DSDL2_INCLUDE_DIR and -DSDL2_LIBRARY "
			"(from SDL2's android build) or define an SDL2::SDL2 imported target before "
			"including Pyros3D. See otherplatforms/android/README.md")
	endif()
	message(STATUS "SDL2 (Android): using SDL2::SDL2")
endif()
