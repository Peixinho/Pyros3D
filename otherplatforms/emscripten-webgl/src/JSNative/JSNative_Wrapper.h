#pragma once
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
class PyrosJSNative {
	private:
		EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
	public:
		PyrosJSNative() {}
		void CreateContext();
};