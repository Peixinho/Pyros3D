//============================================================================
// Name        : JsApplication.cpp
//============================================================================

#include "JsApplication.h"

namespace p3d {

	JsApplication::JsApplication(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType)
		: SDL2Context(width, height, title, windowType)
	{
	}

	JsApplication::~JsApplication() = default;

	void JsApplication::Init()
	{
		SDL2Context::Init();
	}

	void JsApplication::Update()
	{
		// Intentionally empty — JS drives the frame.
	}

	void JsApplication::Shutdown()
	{
		SDL2Context::Shutdown();
	}

}
