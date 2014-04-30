//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "RotatingCube.h"
#include <emscripten.h>

using namespace std;
using namespace p3d;
using namespace p3d::Math;
/*
 * 
 */
RotatingCube* window;
bool init;
void mainloop()
{

	if (!init)
	{
		// Create Context Window
		window = new RotatingCube();
		// Initialize
		window->Init();
		init = true;
	}
    // Get Events
    window->GetEvents();

    // Update
    window->Update();
    
    // Draw in Screen
    window->Draw();
}
int main(int argc, char** argv) {
	
	init = false;
	#ifdef EMSCRIPTEN
	  emscripten_set_main_loop(mainloop, 30, 0);
	#else
	  while (1)
	  {
	    mainloop();
	  }
	#endif   

        // Shutdown Window
    window->Shutdown();
    
    // Delete Context
    delete window;
    return 0;
}

