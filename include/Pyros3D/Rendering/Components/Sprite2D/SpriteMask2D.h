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
//               it is a rectangle, a circle, a convex polygon, or the
//               silhouette of an image (which is cut into rectangles when
//               it is loaded).
//
//               The edge is hard unless the mask is given a Feather: a
//               width, inside the outline, over which what it shows fades
//               out (or, for sprites shown outside it, in). That is cut
//               too - a band of pieces round the edge whose corners carry
//               the fade - so it is as much the same everywhere as the hard
//               edge is. An image's silhouette has no feather.
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
			Polygon = 2,
			// The opaque part of an image, laid over the rectangle Size
			// gives: any silhouette, holes and all. Hard-edged - a texel of
			// the image is in or out - and read at no finer than 128 cells
			// a side, which is what keeps it a few dozen pieces.
			Image = 3
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
		// How wide the soft edge is, in the scene's units, measured in from
		// the outline. 0 is a hard edge. More than the shape has room for
		// is as much as it has.
		f32 GetFeather() const { return feather; }
		void SetFeather(const f32 width);
		// 3 to 16 points making a convex outline, either way round. False
		// (and the old ones kept) for anything else.
		bool SetPolygon(const std::vector<Vec2> &points);
		const std::vector<Vec2> &GetPolygon() const { return polygon; }

		// The image an Image mask takes its shape from: where it is on disk,
		// and what a scene file stores. False if it cannot be read (the mask
		// then cuts nothing). Texels with alpha above the threshold are in.
		bool SetImage(const std::string &resolvedPath, const std::string &authoredPath, const int32 alphaThreshold = 127);
		const std::string &GetImagePath() const { return imagePath; }
		int32 GetAlphaThreshold() const { return alphaThreshold; }
		// How many rectangles the image came to.
		uint32 ImagePieceCount() const { return (uint32)imageRects.size(); }
		// The shape as convex pieces in the world that do not overlap: one
		// for a rectangle, a circle or a polygon, many for an image.
		void WorldPieces(std::vector<std::vector<Vec2> > &out) const;

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
		f32 feather = 0.f;
		std::vector<Vec2> polygon;
		std::string imagePath;
		int32 alphaThreshold = 127;
		// The image's opaque area, as (x0, y0, x1, y1) in 0..1 across it,
		// y UP - ready to be laid over Size.
		std::vector<Vec4> imageRects;
		bool enabled;
		uint32 revision;
	};

};

#endif /* SPRITEMASK2D_H */
