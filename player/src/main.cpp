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

#if defined(__APPLE__)
#include <string>
#include <pthread.h>
#include <pthread/qos.h>
#endif
#include "PyrosPlayer.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
// (iOS: SDL owns the process's main - this one becomes SDL_main - and starts UIKit for it)
#include <SDL_main.h>
#endif
#endif
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>
#include <Pyros3D/Utils/Console/Console.h>

using namespace p3d;

// (every platform: the game's own thread asks for the performance cores)
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>

#ifdef EMSCRIPTEN
#include <emscripten.h>
// A browser calls the game once a frame; it cannot be handed a loop that
// never returns. The same four steps as the loop in main() below.
static PyrosPlayer* webGame = NULL;
static void WebFrame()
{
	if (!webGame->IsRunning()) { emscripten_cancel_main_loop(); return; }
	FrameProfiler& prof = FrameProfiler::Instance();
	prof.BeginFrame();
	webGame->GetEvents();
	webGame->Update();
	webGame->Draw();
	prof.EndFrame();
}
#endif

int main(int argc, char** argv)
{
	// A built game is a GUI application on Windows and starts with stdout
	// going nowhere. Reattach first, so everything below can be seen by
	// anyone who ran it from a terminal to watch exactly that.
	AttachToParentConsole();

	// The thread the game runs on asks for the performance cores (and, on
	// Windows, not to be slowed to save power). Started from a Finder window
	// it has them anyway; started from a terminal, a script or another
	// program it inherits whatever that had, and a busy machine then runs the
	// whole frame on an efficiency core. On a processor of two kinds under
	// Windows or Linux nothing kept it off the slow ones at all.
	p3d::JobSystem::KeepOnPerformanceCores();

	// Before anything else: on Windows an access violation otherwise kills
	// the process with no output at all, which from the outside is exactly
	// what "the game just closes" looks like.
	InstallCrashHandler();
	// (3 s without a frame: a hang picking something up took 4 and the default 8 said nothing - where
	// every thread is then goes to PyrosHang.txt; a long load writes one too, and the next hang's replaces it)
	InstallHangWatchdog(3);

	// --fill-shader-cache <folder>: the shaders made from here on are kept there (see
	// PyrosPlayer::StepFillShaderCache) - said before anything is made, the window's own included.
	for (int i = 1; i + 1 < argc; i++)
		if (std::string(argv[i]) == "--fill-shader-cache")
		{
#ifdef _WIN32
			_putenv_s("PYROS_SPIRV_CACHE", argv[i + 1]);
#else
			setenv("PYROS_SPIRV_CACHE", argv[i + 1], 1);
#endif
		}

	// Every picture under assets/ (what --fill-texture-cache and the warm-up below encode)
	auto picturesUnderAssets = []() {
		std::vector<std::string> files;
		std::error_code ec;
		for (std::filesystem::recursive_directory_iterator it("assets", ec), end; !ec && it != end; it.increment(ec))
		{
			if (!it->is_regular_file(ec)) continue;
			std::string ext = it->path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") files.push_back(it->path().generic_string());
		}
		return files;
	};
	// --fill-texture-cache <folder>: every picture under assets/ in GPU blocks, with its mips, into that folder - and out.
	// A game shipped with it (texcache/ beside it) never encodes a map in a frame: on a first run each material's maps
	// were encoded as they first came into view, on the main thread - seconds of a frozen game landing in a town. (The
	// names are the pictures' paths from the game's folder and what they hold: the same on every machine and install.)
	for (int i = 1; i + 1 < argc; i++)
		if (std::string(argv[i]) == "--fill-texture-cache")
		{
#ifdef _WIN32
			_putenv_s("PYROS_TEXTURE_CACHE", argv[i + 1]);
#else
			setenv("PYROS_TEXTURE_CACHE", argv[i + 1], 1);
#endif
			std::error_code ec;
			std::filesystem::create_directories(argv[i + 1], ec);
			const std::vector<std::string> files = picturesUnderAssets();
			std::atomic<size_t> next(0), made(0);
			std::vector<std::thread> pool;
			const unsigned n = std::max(1u, std::thread::hardware_concurrency());
			for (unsigned t = 0; t < n; t++)
				pool.emplace_back([&]() { for (size_t k; (k = next++) < files.size();) if (p3d::Texture::MakeCompressedCache(files[k])) made++; });
			for (std::thread &t : pool) t.join();
			printf("[texture cache] %zu of %zu pictures in blocks in %s\n", (size_t)made, files.size(), argv[i + 1]);
			return 0;
		}

	PyrosPlayer* game = new PyrosPlayer();
	game->SetLaunchArgs(argc, argv);
	game->Init();

	// The maps' blocks made ahead, on threads of their own and below the game's priority: every picture of the game not
	// in the texture cache yet is encoded while the title is up - a few seconds, once for each picture new or changed -
	// so a material seen for the first time is read from the cache, not encoded in the frame that first draws it (on a
	// first run, seconds frozen landing in a town). PYROS_TEXTURE_WARM=0: not.
	std::atomic<bool> warmStop(false);
	std::vector<std::thread> warm;
#ifndef EMSCRIPTEN
	if (!(std::getenv("PYROS_TEXTURE_WARM") && std::getenv("PYROS_TEXTURE_WARM")[0] == '0') && p3d::Texture::GetCompression())
	{
		std::shared_ptr<std::vector<std::string> > files = std::make_shared<std::vector<std::string> >(picturesUnderAssets());
		std::shared_ptr<std::atomic<size_t> > next = std::make_shared<std::atomic<size_t> >(0);
		const unsigned hw = std::thread::hardware_concurrency();
		const unsigned n = hw > 4 ? (std::min)(hw - 2, 8u) : 1u;
		for (unsigned t = 0; t < n && !files->empty(); t++)
			warm.emplace_back([files, next, &warmStop]() {
#if defined(_WIN32)
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#elif defined(__APPLE__)
				pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
				for (size_t k; !warmStop.load() && (k = (*next)++) < files->size();) p3d::Texture::MakeCompressedCache((*files)[k]);
			});
	}
#endif

#ifdef EMSCRIPTEN
	// (the page has put the game's files in place before main() was run -
	// player/web/player-shell.js)
	webGame = game;
	emscripten_set_main_loop(WebFrame, 0, 0);
	return 0;
#endif

	// PYROS_QUIT_AFTER=<seconds>: the game closes itself, the way closing its
	// window does. For checking from a script that a build starts, runs and
	// leaves cleanly (exit code 0, no crash report) with nobody at the mouse.
	double quitAfter = 0.0;
	if (const char* q = std::getenv("PYROS_QUIT_AFTER")) quitAfter = std::atof(q);
	const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();

	while (game->IsRunning())
	{
		if (quitAfter > 0.0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() > quitAfter)
		{
			quitAfter = 0.0;
			game->Close();
		}
		FrameProfiler& prof = FrameProfiler::Instance();
		prof.BeginFrame();
		{
			PYROS_PROFILE_SCOPE("App.GetEvents");
			game->GetEvents();
		}
		{
			PYROS_PROFILE_SCOPE("App.Update");
			game->Update();
			FrameHeartbeat();
		}
		{
			PYROS_PROFILE_SCOPE("App.Draw");
			game->Draw();
		}
		prof.EndFrame();
	}

	warmStop = true;
	for (std::thread &t : warm) t.join();     // (each on a picture at most: a tenth of a second)
	game->Shutdown();
	// Everything that has to be closed has been, above. What `delete game` adds
	// is the script state's own teardown - and that runs after the window and
	// the render device are gone: every object a script was still holding (a
	// mesh it had made, a label) frees its buffers through no device at all,
	// and quitting the game ended in a crash report instead of a quiet exit.
	// The dedicated server leaves the same way, for the same reason.
	fflush(stdout); fflush(stderr);
	std::_Exit(0);
}
