//============================================================================
// Name        : main.cpp
// Description : Emscripten entry for the JS-first Pyros3D library.
//               Runtime does not auto-run (INVOKE_RUN=0); JS constructs
//               Application and owns requestAnimationFrame.
//============================================================================

#include <Pyros3D/Utils/Bindings/PyrosEmbind.h>
#include "JsApplication.h"
#include <memory>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#endif

using namespace p3d;

#ifdef __EMSCRIPTEN__

namespace {

	std::shared_ptr<JsApplication> MakeApplication(uint32 w, uint32 h, const std::string &title, uint32 windowType)
	{
		return std::make_shared<JsApplication>(w, h, title, windowType);
	}

} // namespace

EMSCRIPTEN_BINDINGS(pyros_js_application)
{
	using namespace emscripten;

	constant("WindowType_Fullscreen", (int)WindowType::Fullscreen);
	constant("WindowType_Resize", (int)WindowType::Resize);
	constant("WindowType_Close", (int)WindowType::Close);
	constant("WindowType_None", (int)WindowType::None);
	constant("WindowType_Titlebar", (int)WindowType::Titlebar);

	class_<JsApplication>("Application")
		.smart_ptr_constructor("Application", &MakeApplication)
		.function("init", &JsApplication::Init)
		.function("pollEvents", &JsApplication::PollEvents)
		.function("draw", &JsApplication::Present)
		.function("getTime", &JsApplication::GetTimeSeconds)
		.function("getWidth", &JsApplication::GetWidth)
		.function("getHeight", &JsApplication::GetHeight)
		.function("isRunning", &JsApplication::Running)
		.function("close", &JsApplication::Quit)
		.function("onResize", &JsApplication::ResizeWindow);
}

#endif

int main(int, char **)
{
#ifdef __EMSCRIPTEN__
	EnsurePyrosEmbindLinked();
#endif
	return 0;
}
