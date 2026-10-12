//============================================================================
// Name        : SpriteMask2D.h
// Description : A shape that sprites are shown only inside of, or only
//               outside of: a window a scene is seen through, a porthole, a
//               health bar that empties from one end, a reveal.
//
//               The mask is a shape and nothing else - it draws nothing. A
//               sprite says how it is to be treated (Sprite2D::
//               SetMaskInteraction) and which mask it means: by default the
//               nearest one above it in the tree, so "a mask with the
//               things it shows as its children" needs nothing more said;
//               or one named, anywhere in the scene.
//
//               It works by cutting the sprite's quads against the mask's
//               outline before they are drawn (ClipSpriteMesh2D), not in
//               the renderer: so it is the same on every backend, it works
//               inside a SpriteBatch2D, and a sprite wholly outside a mask
//               costs no draw at all. What that means for the shape is that
//               it is an outline - a rectangle, a circle, a convex polygon -
//               and not the alpha of an image.
//============================================================================

#ifndef SPRITEMASK2D_H
#define SPRITEMASK2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <string>
#include <vector>

namespace p3d {

	class SceneGraph;
	class GameObject;

	namespace SpriteMaskShape2D
	{
		enum {
			Rect = 0,
			Circle = 1,
			// Up to 16 points, convex, in the object's local space.
			Polygon = 2
		};
	}

	class PYROS3D_API SpriteMask2D : public IComponent {

	public:

		SpriteMask2D();
		virtual ~SpriteMask2D();

		int32 GetShape() const { return shape; }
		void SetShape(const int32 s);
		// A rectangle's width and height; a circle's diameter is its x.
		const Vec2 &GetSize() const { return size; }
		void SetSize(const Vec2 &s);
		const Vec2 &GetOffset() const { return offset; }
		void SetOffset(const Vec2 &o);
		// 3 to 16 points making a convex outline, either way round. False
		// (and the old ones kept) for anything else.
		bool SetPolygon(const std::vector<Vec2> &points);
		const std::vector<Vec2> &GetPolygon() const { return polygon; }

		// Off, sprites that name it are drawn whole.
		void SetEnabled(const bool e);
		bool IsEnabled() const { return enabled; }

		// The outline in the world, counter-clockwise. Empty when it has no
		// owner or no area.
		void WorldOutline(std::vector<Vec2> &out) const;
		// Goes up whenever the shape changes; with the owner's transform it
		// is everything a sprite needs to know whether to cut itself again.
		uint32 GetRevision() const { return revision; }

		// The enabled mask a sprite on `go` means: the one named (anywhere
		// in go's scene), or with no name the nearest on go or above it.
		static SpriteMask2D* Find(GameObject* go, const std::string &name);
		static uint32 LiveCount();

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual bool NeedsUpdate() const { return false; }
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::SpriteMask2D; }

	private:

		int32 shape;
		Vec2 size;
		Vec2 offset;
		std::vector<Vec2> polygon;
		bool enabled;
		uint32 revision;
	};

};

#endif /* SPRITEMASK2D_H */
