//============================================================================
// Name        : SpriteSheet2D.h
// Description : A sprite sheet (.p3ds): how ONE image is cut into named
//               frames, and which runs of frames are animation clips.
//
//               This is what a sprite draws from. The image stays one texture
//               on the GPU and a frame is a rectangle of it, so a character
//               with sixty frames is one decode, one texture and one material
//               - not sixty PNGs written beside the sheet and sixty textures
//               swapped at run time, which is what slicing used to mean.
//
//               Like a tileset (.p3dt) it is a JSON sidecar beside the image:
//               two scenes share it, and re-cutting it touches no scene file.
//               Like a tileset it holds no Texture and never touches a render
//               device, so everything here can be checked without a window.
//
//               Frames are free rectangles, not a grid, because that is what
//               a packed atlas is. A grid is just one way of making them
//               (SliceGrid); cutting round the opaque islands of an image is
//               another (SpriteSheet2DAutoSlice); and reading the JSON an
//               atlas packer or Aseprite wrote is a third (ImportSpriteAtlasJson).
//============================================================================

#ifndef SPRITESHEET2D_H
#define SPRITESHEET2D_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <string>
#include <vector>

namespace p3d {

	struct PYROS3D_API SpriteFrame2D {
		std::string name;

		// The rectangle of the image this frame samples, in PIXELS, with y
		// running DOWN from the image's top - the convention every image
		// editor and every packer uses. It does not leak out of here: a
		// sprite's quad is built y-up like the world it sits in.
		int32 x = 0, y = 0, w = 0, h = 0;

		// Where the owner's origin sits on the frame, normalised over the
		// frame's SOURCE size, y UP: (0.5, 0) is bottom-centre, where a
		// standing character's feet are. Per frame, because an attack frame
		// that reaches forward must not slide the body back to stay centred.
		Vec2 pivot = Vec2(0.5f, 0.5f);

		// Nine-slice borders in pixels: left, bottom, right, top. All zero
		// means "not sliced". Only the middle stretches; the corners keep
		// their size, which is the whole point of a panel or a platform that
		// has to be drawn at any length.
		Vec4 border = Vec4(0.f, 0.f, 0.f, 0.f);

		// A packer trims the transparent margin off a frame to save atlas
		// space. These say what it trimmed: the size the frame had before,
		// and where the kept rectangle sat inside it (from the top-left).
		// Without them every frame of a trimmed animation is centred on its
		// own kept pixels and the character jitters. Zero source size means
		// the frame was not trimmed.
		int32 sourceW = 0, sourceH = 0;
		int32 offsetX = 0, offsetY = 0;

		// How long this frame is held, in seconds; zero means "the clip's
		// rate decides". Aseprite authors timing per frame, and an animation
		// with a long anticipation frame loses it if every frame gets 1/fps.
		f32 duration = 0.f;

		int32 SourceW() const { return sourceW > 0 ? sourceW : w; }
		int32 SourceH() const { return sourceH > 0 ? sourceH : h; }
		bool HasBorder() const { return border.x > 0.f || border.y > 0.f || border.z > 0.f || border.w > 0.f; }
	};

	// Something a clip tells the game at a given moment: a footstep, the
	// frame a sword connects on. Named, so a script switches on the name and
	// the artist can move it to another frame without touching the script.
	struct PYROS3D_API SpriteEvent2D {
		int32 at = 0;          // position in the clip's frame list, not a frame index
		std::string name;
	};

	struct PYROS3D_API SpriteClip2D {
		std::string name;
		// Indices into the sheet's frames, in play order. A list rather than
		// a first/last pair so a clip can repeat a frame, skip one, or run
		// backwards without the sheet having to be re-ordered for it.
		std::vector<int32> frames;
		f32 fps = 12.f;
		bool loop = true;
		// Forwards then backwards, without the end frames doubled.
		bool pingPong = false;
		std::vector<SpriteEvent2D> events;
	};

	namespace SpriteFilter2D {
		enum {
			Linear = 0,
			// Pixel art. Linear filtering blurs every texel edge of art that
			// was drawn to have them.
			Nearest = 1
		};
	}

	struct PYROS3D_API SpriteSheet2D {

		// Project-relative path of the image, "assets/" prefix intact.
		std::string image;

		// A normal map laid out exactly as the image is - same size, same
		// frames - or empty. With one, a sprite that takes 2D lighting is
		// shaded by which way each texel faces the light, and a flat
		// drawing reads as a thing with a shape; without, a light is a
		// wash of brightness by distance. How far above the plane a light's
		// z puts it is how steeply it rakes across the surface.
		std::string normalImage;

		// How many image pixels make one world unit. This is what makes a
		// sprite the size its art says it is: a 32px frame at 32 is exactly
		// one unit tall, and every frame of the sheet agrees with every
		// other without anyone scaling a quad by eye.
		f32 pixelsPerUnit = 100.f;

		int32 filter = SpriteFilter2D::Linear;

		std::vector<SpriteFrame2D> frames;
		std::vector<SpriteClip2D> clips;

		// --- derived, not serialized ---------------------------------------

		// The image's pixel size. Not in the file for the reason a tileset's
		// is not: the PNG is the truth about its own dimensions. Until this
		// is set, UVRect() returns a zero rect rather than guessing.
		void SetImageSize(const int32 w, const int32 h);
		int32 GetImageWidth() const { return imageW; }
		int32 GetImageHeight() const { return imageH; }
		bool HaveImageSize() const { return imageW > 0 && imageH > 0; }

		// --- lookup --------------------------------------------------------

		int32 FindFrame(const std::string &name) const;   // -1 when absent
		int32 FindClip(const std::string &name) const;    // -1 when absent
		bool ValidFrame(const int32 i) const { return i >= 0 && i < (int32)frames.size(); }

		// (left, top, right, bottom) in texture coordinates, v running down,
		// inset by EdgeInset() texels so a frame never samples its
		// neighbour's pixels. Zero rect for an index out of range or an
		// unresolved image size.
		Vec4 UVRect(const int32 frame) const;

		// How far inside a frame's rectangle its texture coordinates stop,
		// in texels. The inset is what keeps a neighbour's pixels out, and
		// it is also taken off the picture: the quad is the frame's full
		// size, so half a texel in from each side shows only half of each
		// edge texel - nothing, where a frame has a clear margin, and a
		// visibly thinner outline where it was cut tight to its pixels.
		//
		// So it is as small as the sheet allows. Smoothly filtered frames
		// packed edge to edge need the half texel: a sample at the edge
		// blends with what is next to it. Nearest filtering never blends,
		// and frames with clear padding round them have nothing next to
		// them to blend with - both need only enough to stay off the exact
		// boundary. Negative (the default) means "decide from the filter".
		f32 edgeInset = -1.f;
		f32 EdgeInset() const { return edgeInset >= 0.f ? edgeInset : (filter == SpriteFilter2D::Nearest ? 0.02f : 0.5f); }

		// --- making frames --------------------------------------------------

		// Replaces the frames with a regular grid, named prefix0, prefix1...
		// row by row from the top-left. Returns how many cells it made.
		// Clips are dropped: their indices meant the old frames.
		int32 SliceGrid(const int32 cellW, const int32 cellH,
			const int32 margin = 0, const int32 spacing = 0,
			const std::string &prefix = "frame");

		// The same by count rather than by size: a sheet known to be 6x4.
		int32 SliceCount(const int32 columns, const int32 rows,
			const std::string &prefix = "frame");

		// Appends a clip over frames [first, last] inclusive (either order -
		// last < first plays backwards). Returns its index, or -1 when the
		// range is not inside the sheet.
		int32 AddClipRange(const std::string &name, const int32 first, const int32 last,
			const f32 fps = 12.f, const bool loop = true);

	private:
		int32 imageW = 0;
		int32 imageH = 0;
	};

	// --- playing a clip ------------------------------------------------------

	// Where a clip is at `seconds` after it started. Pure, so "frame 3 at
	// 0.25s" is a test and not something found by watching.
	struct PYROS3D_API SpriteClipCursor2D {
		int32 step = 0;        // position in clip.frames
		int32 frame = -1;      // sheet frame index, -1 for an empty clip
		bool finished = false; // a non-looping clip ran out
	};
	PYROS3D_API SpriteClipCursor2D SpriteClip2DAt(const SpriteSheet2D &sheet,
		const SpriteClip2D &clip, const f32 seconds);
	// How long one pass of the clip takes, honouring per-frame durations.
	PYROS3D_API f32 SpriteClip2DLength(const SpriteSheet2D &sheet, const SpriteClip2D &clip);

	// --- cutting by content ---------------------------------------------------

	// One rectangle per island of pixels whose alpha is above the threshold,
	// in reading order (top to bottom, then left to right, by rows that
	// overlap vertically). `rgba` is w*h*4 bytes, row 0 at the top. Islands
	// closer than `merge` pixels are one sprite - a character's detached
	// hand or a dotted 'i' is not its own frame. Rectangles smaller than
	// `minSize` on both axes are dropped as stray pixels.
	struct PYROS3D_API SpriteRect2D { int32 x = 0, y = 0, w = 0, h = 0; };
	PYROS3D_API std::vector<SpriteRect2D> SpriteSheet2DAutoSlice(const uchar* rgba,
		const int32 w, const int32 h, const int32 alphaThreshold = 8,
		const int32 merge = 1, const int32 minSize = 2);

	// The same, reading the image itself. False when it cannot be decoded.
	// Here so that a tool cutting a sheet needs no image decoder of its own.
	PYROS3D_API bool SpriteSheet2DAutoSliceFile(const std::string &resolvedImage,
		std::vector<SpriteRect2D> &out, const int32 alphaThreshold = 8,
		const int32 merge = 1, const int32 minSize = 2);

	// --- packing loose images into one ----------------------------------------

	// A game's art usually arrives as one file per frame. Drawn that way it
	// is one texture and one draw call per frame; packed, it is one texture,
	// one material and one batch. This is the packer: it lays the images
	// out in one picture and says where each went, as frames of a sheet.
	struct PYROS3D_API SpriteAtlasImage2D {
		std::string name;              // the frame's name
		int32 w = 0, h = 0;
		std::vector<uchar> rgba;       // w*h*4, row 0 at the top
	};
	struct PYROS3D_API SpriteAtlasOptions2D {
		// Empty pixels left around every image, so a filtered sample at one
		// frame's edge does not pick up its neighbour.
		int32 padding = 2;
		// Each image's edge pixels repeated outward this far into the
		// padding (never more than half of it): what stops a seam showing
		// between tiles drawn edge to edge.
		int32 extrude = 0;
		// Cut the transparent margin off each image. The frame remembers
		// what was cut (sourceW/H, offsetX/Y), so it is drawn exactly where
		// the whole image would have been and its pivot does not move.
		bool trim = true;
		int32 alphaThreshold = 0;
		// Sides a power of two, for the drivers that still care.
		bool powerOfTwo = true;
		// Refused, with a message, rather than made bigger than this.
		int32 maxSize = 4096;
	};
	// `frames` comes back in the order the images were given, so an
	// animation's frames stay in order wherever they landed in the picture.
	PYROS3D_API bool PackSpriteAtlas2D(const std::vector<SpriteAtlasImage2D> &images,
		const SpriteAtlasOptions2D &options, std::vector<uchar> &rgbaOut,
		int32 &widthOut, int32 &heightOut, std::vector<SpriteFrame2D> &framesOut,
		std::string* errorOut = NULL);

	// The same from files, writing the packed picture as a PNG. `sheetOut`
	// gets the frames and the image size; its image path is the caller's to
	// set (the sheet stores a project-relative one, and this knows no
	// project). Frames are named after their files.
	PYROS3D_API bool PackSpriteAtlasFiles2D(const std::vector<std::string> &resolvedImages,
		const SpriteAtlasOptions2D &options, const std::string &resolvedOutputPng,
		SpriteSheet2D &sheetOut, std::string* errorOut = NULL);

	// A clip for every run of frames named alike and numbered - walk_0,
	// walk_1, walk_2 is a clip "walk" - in number order. Names that already
	// have a clip are left alone. Returns how many clips were added.
	PYROS3D_API int32 SpriteSheet2DClipsFromNames(SpriteSheet2D &sheet, const f32 fps = 12.f);

	// --- other tools' atlases --------------------------------------------------

	// Reads the JSON that Aseprite ("Export Sprite Sheet") and TexturePacker /
	// free-tex-packer (JSON hash or JSON array) write: frames with their
	// rectangles, trim, pivots and durations, and Aseprite's frame tags as
	// clips. `image` is left as the atlas's own "meta.image" file name - the
	// caller knows which folder it sits in. Rotated frames are refused by name
	// rather than drawn sideways.
	PYROS3D_API bool ImportSpriteAtlasJson(const std::string &text,
		SpriteSheet2D &out, std::string *errorOut = NULL);

	// --- the file ---------------------------------------------------------------

	PYROS3D_API std::string SpriteSheet2DToString(const SpriteSheet2D &sheet);
	PYROS3D_API bool SpriteSheet2DFromString(const std::string &text,
		SpriteSheet2D &out, std::string *errorOut = NULL);
	PYROS3D_API bool LoadSpriteSheet2D(const std::string &filename,
		SpriteSheet2D &out, std::string *errorOut = NULL);
	PYROS3D_API bool SaveSpriteSheet2D(const std::string &filename,
		const SpriteSheet2D &sheet, std::string *errorOut = NULL);

	// --- geometry -----------------------------------------------------------------

	namespace SpriteDraw2D {
		enum {
			Simple = 0,   // one quad, the frame at its own size
			Sliced = 1,   // nine-slice, stretched to `size`
			Tiled = 2     // the frame repeated to fill `size`
		};
	}

	struct PYROS3D_API SpriteMeshParams2D {
		int32 frame = 0;
		bool flipX = false;
		bool flipY = false;
		Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
		int32 mode = SpriteDraw2D::Simple;
		// World size for Sliced/Tiled. Ignored by Simple, whose size is the
		// frame's pixels over pixelsPerUnit.
		Vec2 size = Vec2(1.f, 1.f);
		// 0 = the sheet's.
		f32 pixelsPerUnit = 0.f;
		// A pivot that replaces the frame's own. For the one sprite that
		// wants to hang from its top edge while every other user of the
		// sheet stands on its feet - and for a bare image, which has no
		// sheet file to say where its pivot is.
		bool overridePivot = false;
		Vec2 pivot = Vec2(0.5f, 0.5f);
	};

	// A sprite's quads, CPU-side, in the owner's local space with the pivot
	// at the origin. Appends, so a batch is this called once per sprite into
	// one mesh; `transform` (when given) places the sprite in the batch.
	struct PYROS3D_API SpriteMesh2D {
		std::vector<Vec3> vertex;
		std::vector<Vec3> normal;
		// Which way the texture's u and v run across the quad, after flips
		// and the batch's transform: what a normal map is read against.
		std::vector<Vec3> tangent;
		std::vector<Vec3> bitangent;
		std::vector<Vec2> texcoord;
		std::vector<Vec4> color;
		std::vector<__INDEX_C_TYPE__> index;
		Vec3 minBounds = Vec3(0.f, 0.f, 0.f);
		Vec3 maxBounds = Vec3(0.f, 0.f, 0.f);
		void Clear();
		int32 QuadCount() const { return (int32)(index.size() / 6); }
	};
	// False (and nothing appended) when the frame is out of range, the image
	// size is unresolved, or the frame has no area.
	PYROS3D_API bool BuildSpriteMesh2D(const SpriteSheet2D &sheet,
		const SpriteMeshParams2D &params, SpriteMesh2D &out,
		const Matrix* transform = NULL);
	// Cuts the quads of `mesh` from `firstVertex` on against a convex
	// outline given in the mesh's own space, keeping what is inside it - or,
	// with `inside` false, what is outside. Texture coordinates and colours
	// are carried to the new corners, so what is left looks exactly as it
	// did. This is how a sprite mask works: on the quads, before they are
	// drawn, which needs nothing of the renderer and so works in a batch
	// and on every backend. False when nothing is left (the quads are then
	// removed).
	PYROS3D_API bool ClipSpriteMesh2D(SpriteMesh2D &mesh, const size_t firstVertex,
		const std::vector<Vec2> &convexOutline, const bool inside);

	// The local-space rectangle a sprite covers: (minX, minY, maxX, maxY).
	// What a selection outline and a collider fit both want, without building
	// the mesh to find out.
	PYROS3D_API Vec4 SpriteLocalRect2D(const SpriteSheet2D &sheet,
		const SpriteMeshParams2D &params);

}

#endif /* SPRITESHEET2D_H */
