//============================================================================
// Name        : LoadingScreen.h
// Author      : Duarte Peixinho
// Description : What a game shows while a scene loads: a UI scene - the
//               engine's own, or one the game made - with a bar and a
//               percentage the load moves.
//============================================================================

#ifndef LOADINGSCREEN_H
#define LOADINGSCREEN_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace p3d {

	class SceneGraph;
	class GameObject;
	class UIRenderer;

	// A scene load runs on the main thread from its first object to its solved light, and nothing
	// was drawn while it did: a window that stayed black for as long as it took (a minute and more
	// for a big lit scene) and looked hung. This draws a screen between the load's steps - whenever
	// LoadProgress hears from the loaders, at most ~30 times a second - and keeps the window alive.
	//
	// The screen is a UI scene. The engine's own (DefaultSceneJson) is a title over a bar with the
	// percentage under it, in the built-in font; a game changes its look (Look: title, picture,
	// colours) or brings its own scene, made in the editor like any UI. Either way the elements
	// below are found by name and kept up to date - a scene of the game's has whichever it wants:
	//
	//   Loading_Bar      a UIRect whose right anchor is moved to the fraction done (a fill)
	//   Loading_Percent  a UIText set to "42%"
	//   Loading_Stage    a UIText set to what is being done ("Loading", "Lighting", ...)
	//   Loading_Title    a UIText given Look::title, when there is one
	//   Loading_Image    a UIImage given Look::image, when there is one
	class PYROS3D_API LoadingScreen
	{
	public:
		struct Look {
			std::string title;           // (empty: the default scene's own, "Loading")
			std::string image;           // a picture behind it all, a path as a scene names one
			Vec4 background = Vec4(0.055f, 0.06f, 0.07f, 1.f);
			Vec4 accent = Vec4(0.92f, 0.70f, 0.28f, 1.f);       // the bar
			Vec4 text = Vec4(0.92f, 0.92f, 0.90f, 1.f);
			bool showPercent = true;
		};

		// The engine's own screen, with a look in it.
		static std::string DefaultSceneJson(const Look &look);

		LoadingScreen();
		~LoadingScreen();

		// sceneJson: a UI scene (DefaultSceneJson, or a game's file's text); scenePath: where it
		// is, for its assets' paths. pump: called between frames to keep the window alive (the
		// platform's events, the hang watchdog's heartbeat) - nothing that touches the game.
		bool Open(const std::string &sceneJson, const std::string &scenePath, const Look &look, const uint32 width, const uint32 height,
			const std::function<void()> &pump);
		// Listens to LoadProgress from here until Close().
		void Listen();
		void SetProgress(const f32 fraction, const std::string &stage);
		// Draws now (Open and SetProgress draw by themselves, when it is time).
		void Draw();
		void Resize(const uint32 width, const uint32 height);
		// Called after each frame it draws, to put it on the screen where the device does not: on
		// GL the frame is the window's to present (SDL_GL_SwapWindow), and the device's EndFrame is
		// nothing - without this the screen was drawn and never shown, a black window for the load.
		void SetPresent(const std::function<void()> &present) { this->present = present; }
		void Close();
		bool IsOpen() const { return scene != NULL; }

	private:
		void Apply(const Look &look);
		GameObject* Find(const std::string &name);

		SceneGraph* scene;
		UIRenderer* ui;
		LoadedSceneAssets assets;
		uint32 width, height;
		std::function<void()> pump, present;
		Vec4 background;
		f32 fraction;
		std::string stage;
		bool showPercent;
		std::chrono::steady_clock::time_point lastDraw;
	};

}

#endif /* LOADINGSCREEN_H */
