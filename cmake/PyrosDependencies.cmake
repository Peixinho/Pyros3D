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
		# sol.hpp only supports Lua 5.1–5.4. Prefer Homebrew lua@5.4 over the
		# unversioned formula (often 5.5+).
		if (APPLE)
			execute_process(COMMAND brew --prefix lua@5.4
				OUTPUT_VARIABLE HOMEBREW_LUA54_PREFIX
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET)
			if (HOMEBREW_LUA54_PREFIX AND EXISTS "${HOMEBREW_LUA54_PREFIX}/include/lua/lua.h")
				set(LUA_INCLUDE_DIR "${HOMEBREW_LUA54_PREFIX}/include/lua" CACHE PATH "Lua include directory")
				set(LUA_LIBRARY "${HOMEBREW_LUA54_PREFIX}/lib/liblua.dylib" CACHE FILEPATH "Lua library")
			endif()
		endif()

		find_package(Lua51 REQUIRED)
		if (NOT LUA51_FOUND)
			message(FATAL_ERROR "Lua not found (need 5.1–5.4 for sol.hpp)")
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
