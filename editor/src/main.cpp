//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "Editor.h"
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
