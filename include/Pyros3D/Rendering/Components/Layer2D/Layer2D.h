//============================================================================
// Name        : Layer2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A 2D scene layer - draw order and parallax for its subtree
//============================================================================

#ifndef LAYER2D_H
#define LAYER2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	// Put this on a root GameObject and everything under it is one layer.
	//
	// Deliberately thin, because most of what a layer needs already existed
	// and did not need reinventing:
	//
	//   * membership - the subtree, plus GameObject tags for anything that
	//     has to be addressed across layers;
	//   * filtering  - IRenderer::GroupAndSortAssets() already takes an
	//     include-only Tag, so "draw only this layer" is a parameter it has
	//     always had;
	//   * ordering   - the layer root's own z. Under the orthographic camera
	//     a 2D scene uses, z *is* draw order, and the renderer's existing
	//     sort already honours it. Nothing new.
	//
	// The one thing nothing carried is how fast a layer scrolls relative to
	// the camera, which is this component's whole reason to exist.
	class PYROS3D_API Layer2D : public IComponent {

	public:

		Layer2D(const Vec2 &parallax = Vec2(1.f, 1.f));
		virtual ~Layer2D();

		// 1 = moves with the camera, 0 = pinned, 0.5 = half speed (further
		// away). The axes are independent so a layer can scroll horizontally
		// while staying put vertically, which is the common case for a
		// side-scroller's sky.
		void SetParallax(const Vec2 &p) { parallax = p; }
		const Vec2 &GetParallax() const { return parallax; }

		// Hides the layer without unparenting or deleting anything.
		void SetVisible(const bool v) { visible = v; }
		bool IsVisible() const { return visible; }

		// Where the layer root sits when the camera is at the origin. Captured
		// the first time ApplyParallax() runs, because parallax is expressed
		// against the authored position and the editor must keep showing that
		// position, not wherever the last frame's scroll left it.
		const Vec3 &GetBasePosition() const { return basePosition; }

		// Moves this layer's root for a camera at `scroll`. Idempotent - it
		// always positions from basePosition rather than accumulating, so it
		// is safe to call every frame and safe to call twice in one.
		void ApplyParallax(const Vec2 &scroll);

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Layer2D; }

	private:

		Vec2 parallax;
		bool visible;
		Vec3 basePosition;
		bool haveBasePosition;
	};

};

#endif
