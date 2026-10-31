//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "includes.h"
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>
#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)
#include <emscripten.h>
#define PYROS_EMSCRIPTEN 1
#endif
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
// (iOS: SDL owns the process's main - this one becomes SDL_main - and the app runs from its bundle)
#include <SDL.h>
#include <SDL_main.h>
#include <unistd.h>
#define PYROS_IOS 1
#endif
#endif
using namespace std;
using namespace p3d;
using namespace p3d::Math;

// Initialized Flag
bool initialized;

// Demo Instance
DEMO_NAME* window;

// Main Loop Function
void mainloop()
{

	if (!initialized)
	{
		// Create Context Windo
		window = new DEMO_NAME();

		// Initialize
		window->Init();

		// Set Initialized Flag
		initialized = true;
	}

	FrameProfiler &prof = FrameProfiler::Instance();
	prof.BeginFrame();

	{
		PYROS_PROFILE_SCOPE("App.GetEvents");
		window->GetEvents();
	}

	{
		PYROS_PROFILE_SCOPE("App.Update");
		window->Update();
	}

	{
		PYROS_PROFILE_SCOPE("App.Draw");
		window->Draw();
	}

	prof.EndFrame();
}

int main(int argc, char** argv) {

	// Before anything else: a Windows access violation otherwise kills the
	// process with no output at all, which is what "it just opens and closes"
	// looks like from the outside.
	InstallCrashHandler();
#ifdef PYROS_IOS
	// the shaders and assets are in the bundle: lookups are relative to where it runs
	if (char* base = SDL_GetBasePath()) { chdir(base); SDL_free(base); }
#endif

	initialized = false;

#ifdef PYROS_EMSCRIPTEN
	// fps=0 -> browser refresh rate. simulate_infinite_loop=0, so this
	// RETURNS and main() exits normally.
	//
	// The 1 that used to be here makes Emscripten throw an "unwind"
	// exception to escape main, which surfaces as an uncaught error in
	// the page. Most hosts shrug at it; a sandboxed iframe need not,
	// and one that treats an uncaught error in the frame as fatal
	// replaces the whole document with nothing - the demo, the text
	// around it, all of it. EXIT_RUNTIME defaults to 0, so the runtime
	// stays alive after main returns and the loop keeps firing, which
	// is what the unwind was faking.
	emscripten_set_main_loop(mainloop, 0, 0);
#else
	// Create Context Window
	window = new DEMO_NAME();

	// Initialize
	window->Init();

	// Set Initialized Flag
	initialized = true;

	// Game Loop
	while (window->IsRunning())
	{
		mainloop();
	}
#endif

#if !defined(PYROS_EMSCRIPTEN)
	// (what Shutdown() lets go of, the device frees once the frames using it are done)
	// Shutdown Window
	window->Shutdown();

	// Delete Context
	delete window;

	// end
	return 0;
#endif
}
