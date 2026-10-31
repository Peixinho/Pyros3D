//============================================================================
// Name        : PyrosEmbind.h
// Description : Emscripten Embind surface mirroring PyrosBindings (Lua).
//               Target: full parity with Lua usertypes/functions. Implemented
//               incrementally — see otherplatforms/emscripten/EMBIND_PARITY.md.
//============================================================================

#ifndef PYROSEMBIND_H
#define PYROSEMBIND_H

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <Pyros3D/Core/Math/Math.h>
#include <memory>

namespace p3d {
	// Force-link all EMSCRIPTEN_BINDINGS TUs (static lib otherwise drops them).
	void EnsurePyrosEmbindLinked();

	class SceneGraph;
	class GameObject;
	class Projection;
	namespace gfx { class Device; }
	class SceneRenderer;

	// The page's renderer (JS "Renderer"): a scene drawn by the engine's renderer onto the canvas, from a camera
	// object and a projection. Made by the host, which has the graphics device - Application.createRenderer().
	class EmbindRenderer
	{
	public:
		EmbindRenderer(gfx::Device* device, const uint32 width, const uint32 height);
		~EmbindRenderer();
		void Resize(const uint32 width, const uint32 height) { this->width = width; this->height = height; }
		void RenderScene(const Projection &projection, const std::shared_ptr<GameObject> &camera, SceneGraph &scene);
		// (colours as picked: sRGB)
		void SetBackground(const Math::Vec4 &color);
		void SetGlobalLight(const Math::Vec4 &color);
		// The settings a scene file loaded with Scene.load carries: its ambient, background, sky, reflections, exposure
		void UseSceneSettings(SceneGraph &scene);
		SceneRenderer* Get() const { return renderer.get(); }
	private:
		std::unique_ptr<SceneRenderer> renderer;
		uint32 width, height;
	};
}

#endif

#endif /* PYROSEMBIND_H */
