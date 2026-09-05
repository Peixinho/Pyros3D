//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "Editor.h"
#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#if defined(EMSCRIPTEN)
	#include <emscripten.h>
#endif
using namespace std;
using namespace p3d;
using namespace p3d::Math;

// Initialized Flag
bool initialized;

// Demo Instance
Editor* window;

// Main Loop Function
void mainloop()
{

	if (!initialized)
	{
		// Create Context Window
		window = Editor::getInstance();

		// Initialize
		window->Init();

		// Set Initialized Flag
		initialized = true;
	}

	// Same three top-level scopes the player and the demo launcher use
	// (see player/src/main.cpp), so a frame profiled under the editor's Play
	// mode lines up with the same frame profiled in the built game. The
	// profiler is a no-op while disabled, and the window that shows this is
	// off unless asked for (View > Windows > Profiler, or F3).
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

	// See MainProgram.cpp - a silent access violation is what
	// "it opens and closes" looks like without this.
	InstallCrashHandler();

	initialized = false;

	#ifdef EMSCRIPTEN
		emscripten_set_main_loop(mainloop, 0, 0);
	#else
		// Create Context Window
		window = Editor::getInstance();

		// Initialize
		window->Init();

		// Set Initialized Flag
		initialized = true;
		
		// Game Loop
	    while(window->IsRunning())
	    {
		mainloop();
	    }
	#endif   

    #if !defined(EMSCRIPTEN)
	    // Shutdown Window
	    window->Shutdown();
	    
	    // Clean up singleton instance
	    Editor::cleanupInstance();

	    // end
	    return 0;
	#endif
}
