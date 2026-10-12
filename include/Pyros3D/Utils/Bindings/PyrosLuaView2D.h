//============================================================================
// Name        : PyrosLuaView2D.h
// Description : The `view` table scripts frame a 2D scene with, bound ONCE.
//
//               The editor's play mode and the player each used to build
//               this table for themselves, the same dozen functions written
//               twice - which is how the two drift, and a script that frames
//               the game must not behave one way in the preview and another
//               in a build. A host now says where its view lives and what to
//               do when it changes, and gets the same table as the other.
//============================================================================

#ifndef PYROSLUAVIEW2D_H
#define PYROSLUAVIEW2D_H

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <functional>

namespace p3d {

	class SceneGraph;
	class Physics2DWorld;

	struct LuaView2DHost {
		// The scene's view. Asked for at every call rather than captured:
		// loading another scene replaces it.
		std::function<SceneMeta::View2D*()> view;
		// The view was changed by a script. The player rebuilds its
		// projection; the editor marks the scene as touched.
		std::function<void()> changed;
		// The game's viewport, in pixels.
		std::function<Math::Vec2()> viewportSize;
		// Where the mouse is in that viewport, in pixels from its top-left.
		std::function<Math::Vec2()> mouseInViewport;
	};

	// Sets the global `view`.
	PYROS3D_API void LuaBindView2D(sol::state &lua, const LuaView2DHost &host);

	// One frame of a 2D scene's own view, for whoever is showing it: finds
	// what the view follows (by name, anywhere in the scene), runs
	// View2D::Update, and scrolls the automatic layers. Returns false when
	// the view is not enabled. The camera itself is the caller's to place,
	// from view.RenderCenter() / view.CameraMatrix().
	PYROS3D_API bool UpdateSceneView2D(SceneMeta::View2D &view, SceneGraph* scene,
		const f32 dt, const f32 aspect);
	// Puts every automatic layer back where it was authored - when a scene
	// stops being played.
	PYROS3D_API void ResetAutomaticLayers(SceneGraph* scene);

}

#endif
#endif
