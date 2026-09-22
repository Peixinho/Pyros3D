//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "includes.h"
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>
#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)
#include <emscripten.h>
#define PYROS_EMSCRIPTEN 1
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
	// Every example's Shutdown() override deletes its own GPU-owned
	// resources (materials, textures, FBOs, meshes) *before* calling
	// BaseExample::Shutdown() (see e.g. DeferredPBRSpheres::Shutdown()) -
	// none of that chain ever waits for the GPU to finish the last
	// submitted frame first. On Vulkan, destroying a pipeline/sampler/
	// image still referenced by an in-flight command buffer is exactly
	// VUID-vkDestroyPipeline-pipeline-00765/VUID-vkDestroySampler-
	// sampler-01082 - real, reproducible on every clean exit (seen
	// repeatedly this session). One wait here, before any example's
	// Shutdown() runs, covers all of them without touching each one.
	GetActiveRenderDevice().WaitIdle();

	// Shutdown Window
	window->Shutdown();

	// Delete Context
	delete window;

	// end
	return 0;
#endif
}
