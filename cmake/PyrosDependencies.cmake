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
else()
	find_package(Freetype REQUIRED)
	if (NOT FREETYPE_FOUND)
		message(FATAL_ERROR "Freetype2 not found")
	endif()
endif()

# ---------------------------------------------------------------------------
# Bullet
# ---------------------------------------------------------------------------
if (EMSCRIPTEN)
	# No reliable system Bullet on emsdk — fetch a static build.
	include(FetchContent)
	# bullet3 3.25 still declares cmake_minimum_required(<3.5); modern CMake rejects that.
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
	set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
	set(BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
	set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
	set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
	set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
	set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
	set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)
	set(INSTALL_LIBS OFF CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(bullet3
		GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
		GIT_TAG 3.25
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(bullet3)
	set(BULLET_FOUND TRUE)
	set(BULLET_INCLUDE_DIRS ${bullet3_SOURCE_DIR}/src)
	set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath)
	message(STATUS "Bullet: FetchContent ${bullet3_SOURCE_DIR}")
else()
	find_package(Bullet REQUIRED)
	if (NOT BULLET_FOUND)
		message(FATAL_ERROR "Bullet not found")
	endif()
endif()

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
