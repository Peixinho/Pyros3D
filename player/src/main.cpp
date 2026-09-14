//============================================================================
// Name        : main.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Entry point for a built game. Mirrors examples/MainProgram.cpp
//               (crash handler, profiler scopes, wait-idle before teardown)
//               without the DEMO_NAME indirection - a player only ever runs
//               one thing.
//============================================================================

#include "PyrosPlayer.h"
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>
#include <Pyros3D/Utils/Console/Console.h>

using namespace p3d;

int main(int argc, char** argv)
{
	// A built game is a GUI application on Windows and starts with stdout
	// going nowhere. Reattach first, so everything below can be seen by
	// anyone who ran it from a terminal to watch exactly that.
	AttachToParentConsole();

	// Before anything else: on Windows an access violation otherwise kills
	// the process with no output at all, which from the outside is exactly
	// what "the game just closes" looks like.
	InstallCrashHandler();

	PyrosPlayer* game = new PyrosPlayer();
	game->Init();

	while (game->IsRunning())
	{
		FrameProfiler& prof = FrameProfiler::Instance();
		prof.BeginFrame();
		{
			PYROS_PROFILE_SCOPE("App.GetEvents");
			game->GetEvents();
		}
		{
			PYROS_PROFILE_SCOPE("App.Update");
			game->Update();
		}
		{
			PYROS_PROFILE_SCOPE("App.Draw");
			game->Draw();
		}
		prof.EndFrame();
	}

	// One wait covers every GPU-owning thing Shutdown() is about to free -
	// see MainProgram.cpp's comment for the Vulkan validation errors this
	// exists to prevent.
	if (IsActiveRenderDeviceSet())
		GetActiveRenderDevice().WaitIdle();

	game->Shutdown();
	delete game;
	return 0;
}
