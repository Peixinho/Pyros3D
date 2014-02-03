//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "RotatingCube.h"

using namespace std;
using namespace p3d;
using namespace p3d::Math;
/*
 * 
 */

int main(int argc, char** argv) {

    // Create Context Window
    RotatingCube* window = new RotatingCube();
    
    // Initialize
    window->Init();

    // Game Loop
    while(window->IsRunning())
    {
        // Get Events
        window->GetEvents();
        
        // Update
        window->Update();
        
        // Draw in Screen
        window->Draw();
        
    }
    
    // Shutdown Window
    window->Shutdown();
    
    // Delete Context
    delete window;
    
    return 0;
}

