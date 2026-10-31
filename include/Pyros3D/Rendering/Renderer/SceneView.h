//============================================================================
// Name        : SceneView.h
// Author      : Duarte Peixinho
// Description : A second view of a scene, drawn into a texture a material shows - a scope's picture, a security
//               camera, a mirror, a map seen from above. Its own renderer, at its own size; drawn when asked, ahead
//               of the frame's own view, which then samples it as any other texture.
//============================================================================

#ifndef SCENEVIEW_H
#define SCENEVIEW_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <memory>

namespace p3d {

	class SceneGraph;
	class GameObject;
	class Projection;
	class Texture;


	class SceneRenderer;

	class PYROS3D_API SceneView
	{
	public:
		SceneView(gfx::Device* device, const uint32 width, const uint32 height);
		~SceneView();

		// What a material shows the view with (setColorMap): a Texture whose pixels are the view's last picture -
		// the renderer samples the picture itself; nothing of it is on the CPU
		const std::shared_ptr<Texture> &GetTexture() const { return texture; }
		// Its renderer: its sky, haze, background, exposure, reflections. Until something of it is set here, they follow
		// the renderer it was made beside (SetFollow) - each time it draws, so a day passing passes in it too
		SceneRenderer &Renderer() { return *renderer; }
		void SetFollow(SceneRenderer* main) { follow = main; }
		// The host's view - the one scripts' views are made beside (on its device) and follow: the player's, the
		// editor's Scene View in Play. Without one, the first renderer made.
		static void SetMainRenderer(SceneRenderer* main);
		static SceneRenderer* MainRenderer();
		// (its own background over the one it follows - what shows where the view sees nothing and has no sky)
		void SetBackground(const Vec3 &linear) { ownBackground = true; background = linear; }
		// (where it draws nothing, nothing - alpha 0: a figure cut out, laid over a UI)
		void SetTransparentBackground(const bool on) { transparent = on; }
		uint32 GetWidth() const { return width; }
		uint32 GetHeight() const { return height; }
		void Resize(const uint32 width, const uint32 height);
		// Draws it now: the scene from the camera through the projection (perspective - its field of view - or
		// orthographic, its height). What is hidden or shown before this, for this view alone, is as it is now.
		void Render(SceneGraph* scene, GameObject* camera, const Projection &projection);

	private:
		gfx::Device* device;
		std::unique_ptr<SceneRenderer> renderer;
		gfx::Texture target;
		uint32 width, height;
		std::shared_ptr<Texture> texture;
		SceneRenderer* follow = nullptr;
		bool ownBackground = false; Vec3 background;
		bool transparent = false;
		void MakeTarget();
	};

}

#endif /* SCENEVIEW_H */
