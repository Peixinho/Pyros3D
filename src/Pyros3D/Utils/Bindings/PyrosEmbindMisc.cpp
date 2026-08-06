//============================================================================
// Name        : PyrosEmbindMisc.cpp
// Description : Embind File + mouse helpers.
//               Input Lua callbacks are NOT bound — use canvas/DOM events from
//               JS, or a future JS-friendly Input bridge. Key_/MouseButton_
//               constants live in PyrosEmbindEnums.cpp.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Core/File/File.h>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Core/Math/Math.h>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;

namespace {

	bool File_OpenRead(File &f, const std::string &path) { return f.Open(path, false); }
	bool File_OpenWrite(File &f, const std::string &path) { return f.Open(path, true); }
	uint32 File_Size(const File &f) { return f.Size(); }

	Vec2 GetMousePosition()
	{
		return InputManager::GetMousePosition();
	}

} // namespace

namespace p3d {
	void PyrosEmbindMiscForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_misc)
{
	class_<File>("File")
		.constructor<>()
		.function("open", &File_OpenRead)
		.function("openWrite", &File_OpenWrite)
		.function("rewind", &File::Rewind)
		.function("close", &File::Close)
		.function("size", &File_Size);
		// write/read/getData — byte-buffer APIs; prefer FS from JS for assets

	emscripten::function("getMousePosition", &GetMousePosition);

	// placeDecalAtCursor — skipped (Lua sol::object material + LUA_RenderingComponent)
	// asGameObject — skipped (Lua shared_ptr coercion only)
	// setMaterialExtraUniformBlock — skipped (sol::table offsets)
	// Input / LuaInputBridge — skipped (Lua std::function callbacks); use DOM/canvas
}

#endif /* EMSCRIPTEN */
