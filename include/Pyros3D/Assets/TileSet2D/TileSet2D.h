//============================================================================
// Name        : TileSet2D.h
// Description : A tileset as a FILE (.p3dt) - an atlas image cut into a grid,
//               plus whatever each cell means to the game.
//
//               Deliberately a sidecar beside the PNG rather than a block
//               inside a scene: two maps can share a tileset, and re-cutting
//               one does not touch any scene file. What a scene stores is a
//               path to one of these.
//
//               Deliberately data and arithmetic ONLY - no Texture, no GL.
//               The atlas is loaded by whoever draws with it (TileMap2D),
//               which keeps this half testable without a window and stops a
//               tileset from owning a GPU resource it cannot outlive. See
//               TILEMAP_PLAN.md.
//
//               The image's pixel size is NOT stored in the file. The PNG is
//               the truth about its own dimensions, and a .p3dt that
//               disagreed with it would cut the wrong cells while looking
//               perfectly well-formed. Callers set it via SetImageSize()
//               (TileSet2DReadImageSize() reads it without decoding).
//============================================================================

#ifndef TILESET2D_H
#define TILESET2D_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	// What one cell means. Only cells that differ from the default are
	// stored, so a purely decorative tileset serializes to nothing here.
	struct PYROS3D_API TileInfo2D {
		// Whether the collider builder treats this cell as filled. This is
		// the whole of Phase 3's input: solid cells are greedy-merged into
		// rectangles and attached to one static body.
		bool solid = false;
		// Free-form, for the game to read (e.g. "ice", "ladder"). Not
		// interpreted by the engine.
		std::vector<std::string> tags;
	};

	// The contents of one .p3dt.
	struct PYROS3D_API TileSet2D {

		// Path exactly as authored - project-relative, "assets/" prefix and
		// all. Resolving it is the caller's job, because only the caller
		// knows where the project root is. (Stripping the prefix is the trap
		// in memory/scene-asset-paths-keep-the-assets-prefix.md.)
		std::string image;

		// Cell size in PIXELS, and the gaps around and between cells. The
		// three standard tileset knobs; `spacing` is the gutter that stops
		// neighbours bleeding in the source art, `margin` the border.
		int32 tileW = 16;
		int32 tileH = 16;
		int32 margin = 0;
		int32 spacing = 0;

		// Cells per row. 0 means "derive from the image width", which is what
		// almost every sheet wants; an explicit value exists for a sheet with
		// unused space on the right that would otherwise be cut into tiles.
		int32 columns = 0;

		// Sparse: absent means a default-constructed TileInfo2D.
		std::map<int32, TileInfo2D> tiles;

		// The atlas's pixel size. Everything below returns 0 / a zero rect
		// until this is known - a tileset is arithmetic over an image, and
		// without the image there is no answer to give.
		void SetImageSize(const int32 w, const int32 h);
		int32 GetImageWidth() const { return imageW; }
		int32 GetImageHeight() const { return imageH; }
		bool HaveImageSize() const { return imageW > 0 && imageH > 0; }

		int32 Columns() const;
		int32 Rows() const;
		int32 TileCount() const;

		// The cell's texture coordinates as (left, top, right, bottom).
		//
		// v runs DOWN: v=0 is the image's top row, matching the quad every
		// sprite in this engine is built as (SpriteRig2D's QuadGeometry maps
		// its top vertices to v=0) and matching the row-major, top-left-first
		// order tiles are indexed in.
		//
		// Inset by half a texel on all four edges. Without it neighbouring
		// cells bleed into each other along every seam under any filtering -
		// the artifact that reads as a rendering bug and is a UV bug.
		//
		// An index outside [0, TileCount()) returns a zero rect rather than
		// clamping into a real cell: a caller drawing the wrong tile silently
		// is worse than one drawing a degenerate quad it can notice.
		Vec4 UVRect(const int32 index) const;

		bool IsSolid(const int32 index) const;
		// NULL when the cell carries no non-default information.
		const TileInfo2D* Find(const int32 index) const;

	private:
		int32 imageW = 0;
		int32 imageH = 0;
	};

	// Reads an image's dimensions without decoding it. False if the file is
	// missing or is not an image stb can identify.
	PYROS3D_API bool TileSet2DReadImageSize(const std::string &resolvedPath,
		int32 &w, int32 &h);

	// The same contents as text, without touching the filesystem - the editor
	// snapshots a whole tileset per edit, and routing that through the FILE
	// serializer is what guarantees an undo cannot restore something the
	// format cannot express. (The same reasoning as Character2DToString.)
	PYROS3D_API std::string TileSet2DToString(const TileSet2D &set);
	PYROS3D_API bool TileSet2DFromString(const std::string &text,
		TileSet2D &out, std::string *errorOut = NULL);

	// Reads / writes a .p3dt. Neither touches the atlas image, so a loaded
	// tileset has no image size until the caller supplies one.
	PYROS3D_API bool LoadTileSet2D(const std::string &filename,
		TileSet2D &out, std::string *errorOut = NULL);
	PYROS3D_API bool SaveTileSet2D(const std::string &filename,
		const TileSet2D &set, std::string *errorOut = NULL);

}

#endif /* TILESET2D_H */
