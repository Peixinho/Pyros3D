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
# Freetype + Bullet (always required)
# ---------------------------------------------------------------------------
find_package(Freetype REQUIRED)
if (NOT FREETYPE_FOUND)
	message(FATAL_ERROR "Freetype2 not found")
endif()

find_package(Bullet REQUIRED)
if (NOT BULLET_FOUND)
	message(FATAL_ERROR "Bullet not found")
endif()

# ---------------------------------------------------------------------------
# Lua (optional)
# ---------------------------------------------------------------------------
if (HAVE_LUA_BINDINGS)
	add_compile_definitions(LUA_BINDINGS)

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

# ---------------------------------------------------------------------------
# Audio system libs (miniaudio talks to the OS stack)
# ---------------------------------------------------------------------------
if (APPLE)
	find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
	find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
	find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
	set(AUDIO_LIBS ${COREAUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY} ${AUDIOTOOLBOX_LIBRARY})
elseif (UNIX)
	set(AUDIO_LIBS ${CMAKE_DL_LIBS} pthread m)
else()
	set(AUDIO_LIBS "")
endif()
