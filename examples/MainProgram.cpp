//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "includes.h"
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#if defined(EMSCRIPTEN)
#include <emscripten.h>
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

	// Get Events
	window->GetEvents();

	// Update
	window->Update();

	// Draw in Screen
	window->Draw();
}

int main(int argc, char** argv) {

	initialized = false;

#ifdef EMSCRIPTEN
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

#if !defined(EMSCRIPTEN)
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
