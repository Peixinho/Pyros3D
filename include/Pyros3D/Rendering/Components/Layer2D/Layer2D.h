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
#include <memory>
#include <vector>

namespace p3d {

	// Put this on a root GameObject and everything under it is one layer.
	//
	// Deliberately thin, because most of what a layer needs already existed
	// and did not need reinventing:
	//
	//   * membership - the subtree, plus GameObject tags for anything that
	//     has to be addressed across layers;
	//   * filtering  - a renderer can take an include-only Tag, so "draw only this layer" is a parameter it has
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
		// How many exist anywhere: a scene with none is not searched for them.
		static uint32 LiveCount();

		// 1 = moves with the camera, 0 = pinned, 0.5 = half speed (further
		// away). The axes are independent so a layer can scroll horizontally
		// while staying put vertically, which is the common case for a
		// side-scroller's sky.
		void SetParallax(const Vec2 &p) { parallax = p; }
		const Vec2 &GetParallax() const { return parallax; }

		// Hides the layer without unparenting or deleting anything: every
		// mesh beneath it stops being drawn, and comes back as it was when
		// the layer is shown again. (This used to be a flag that was stored,
		// saved and loaded and that nothing read.)
		void SetVisible(const bool v);
		bool IsVisible() const { return visible; }
		// Whether `go` is the root of, or beneath, a hidden layer. For
		// anything that makes or enables a mesh by itself - a sprite, a
		// sprite batch - so it does not put one back under a hidden layer.
		static bool HiddenAbove(GameObject* go);

		// Where the layer root sits when the camera is at the origin. Captured
		// the first time ApplyParallax() runs, because parallax is expressed
		// against the authored position and the editor must keep showing that
		// position, not wherever the last frame's scroll left it.
		const Vec3 &GetBasePosition() const { return basePosition; }

		// Moves this layer's root for a camera at `scroll`. Idempotent - it
		// always positions from basePosition rather than accumulating, so it
		// is safe to call every frame and safe to call twice in one.
		//
		// NOTE the sign: this offsets by scroll * (parallax - 1), which moves
		// a far layer AGAINST the camera. It is kept as it is for the scripts
		// written against it; ScrollWithView() below is the one to use.
		void ApplyParallax(const Vec2 &scroll);

		// Scrolled by the engine rather than by a script: whoever shows the
		// scene calls ScrollWithView() on every automatic layer each frame
		// (see SceneMeta::View2D::autoParallax). Off by default, so a scene
		// whose script already moves its layers is left alone.
		void SetAutomatic(const bool a) { automatic = a; }
		bool IsAutomatic() const { return automatic; }
		// Places the layer for a view centred at `viewCenter`: at its
		// authored position when the view is at the origin, and from there
		// following the view by (1 - parallax) of its travel - so on screen
		// it moves at `parallax` times the speed of the gameplay layer. 1
		// does not move at all; 0 is pinned to the screen.
		void ScrollWithView(const Vec2 &viewCenter);
		// Puts the layer back where it was authored and forgets that
		// position, so the next ScrollWithView latches it afresh.
		void ResetScroll();

		// A layer loaded hidden has nothing beneath it yet when it is told
		// so; this is where it catches up, once the scene has it.
		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual bool NeedsUpdate() const { return false; }
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Layer2D; }

	private:

		Vec2 parallax;
		bool visible;
		void ApplyVisibility();

		Vec3 basePosition;
		bool haveBasePosition;
		bool automatic;
		// What hiding switched off, so showing switches on exactly those and
		// not something that was off for a reason of its own.
		std::vector<std::weak_ptr<IComponent> > hiddenByLayer;
		bool applied;
	};

};

#endif
