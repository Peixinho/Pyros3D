//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "VR_ShootingRange.h"

using namespace std;
using namespace p3d;
using namespace p3d::Math;

// Initialized Flag
bool initialized;

// Demo Instance
VR_ShootingRange* window;

// Main Loop Function
void mainloop()
{

	if (!initialized)
	{
		// Create Context Windo
		window = new VR_ShootingRange();

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

	// Create Context Window
	window = new VR_ShootingRange();

	// Initialize
	window->Init();

	// Set Initialized Flag
	initialized = true;
	
	// Game Loop
    while(window->IsRunning())
    {
		mainloop();
    }
    // Shutdown Window
    window->Shutdown();
    
    // Delete Context
    delete window;

    // end
    return 0;
}
