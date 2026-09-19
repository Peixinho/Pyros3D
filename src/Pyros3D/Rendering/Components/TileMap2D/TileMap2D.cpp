//============================================================================
// Name        : TileMap2D.cpp
// Description : See the header.
//============================================================================

#include <Pyros3D/Rendering/Components/TileMap2D/TileMap2D.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/Culling/Culling.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <utility>
#include <map>
#include <algorithm>
#include <cmath>

namespace p3d {

	namespace {

		// One chunk's geometry. Rebuilt in place on an edit rather than
		// replaced: IGeometry::buffersRevision exists for exactly this, and
		// SendBuffers() bumps it so the renderer's per-shader VAO cache stops
		// pointing at the buffers Dispose() just freed. Skipping that draws
		// the PREVIOUS tiles with the new index count - see Text::UpdateText,
		// the other user of this sequence.
		class TileChunkGeometry : public PrimitiveGeometry {
		public:
			explicit TileChunkGeometry(const TileChunkMesh2D &m) { Apply(m); }

			void Refill(const TileChunkMesh2D &m)
			{
				Dispose();
				index.clear();
				tVertex.clear();
				tNormal.clear();
				tTexcoord.clear();
				Apply(m);
			}

		private:
			void Apply(const TileChunkMesh2D &m)
			{
				tVertex = m.vertex;
				tNormal = m.normal;
				tTexcoord = m.texcoord;
				index = m.index;

				// PrimitiveGeometry::CalculateBounding() is an empty override -
				// every concrete shape sets these by hand in its constructor
				// (see Plane.cpp). Inherit it and the chunk ships with
				// uninitialised bounds, and the cull test keeps or drops it at
				// random.
				minBounds = m.minBounds;
				maxBounds = m.maxBounds;
				BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
				BoundingSphereRadius = maxBounds.distance(BoundingSphereCenter);

				// The full sequence: CreateBuffers() builds the attribute list
				// without uploading it, and a mesh whose buffers were never
				// sent crashes the renderer the first time it is bound.
				CreateBuffers(false);
				SendBuffers();
			}
		};

		class TileMapRenderable : public Renderable {
		public:
			void AddChunk(const TileChunkMesh2D &m)
			{
				Geometries.push_back(new TileChunkGeometry(m));
			}
			void Finish() { CalculateBounding(); }
		};

		// Atlases are shared between tilemaps but NOT through
		// Texture::LoadShared: that cache's key is filename|type|mipmapping,
		// so filter and wrap are not part of a texture's identity there, and
		// its contract says callers that mutate per-instance state must not
		// use it. An atlas must be Nearest + ClampToEdge, which is exactly
		// that mutation. A private cache keeps every sharer a tilemap, and
		// every tilemap wants the same settings.
		std::map<std::string, std::weak_ptr<Texture> > &AtlasCache()
		{
			static std::map<std::string, std::weak_ptr<Texture> > cache;
			return cache;
		}

		std::shared_ptr<Texture> LoadAtlas(const std::string &path)
		{
			std::map<std::string, std::weak_ptr<Texture> > &cache = AtlasCache();
			std::map<std::string, std::weak_ptr<Texture> >::iterator it = cache.find(path);
			if (it != cache.end())
			{
				if (std::shared_ptr<Texture> hit = it->second.lock()) return hit;
				cache.erase(it);
			}

			std::shared_ptr<Texture> t = std::make_shared<Texture>();
			// No mipmaps: a minified mip blends across cell boundaries, which
			// is the same seam the half-texel inset exists to prevent, only
			// unfixable from the UVs.
			if (!t->LoadTexture(path, TextureType::Texture, false))
				return std::shared_ptr<Texture>();
			t->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
			t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
			cache[path] = t;
			return t;
		}

	}

	TileMap2D::TileMap2D(const Vec2 &tileSize)
		: tileSize(tileSize), lit(false), fullDirty(true), collidersDirty(true),
		materialLit(false)
	{
	}

	TileMap2D::~TileMap2D() {}

	void TileMap2D::SetTileSet(const TileSet2D &set)
	{
		tileset = set;
		// A new tileset means a new atlas, so a path resolved for the old one
		// must not survive - see SetResolvedAtlasPath.
		resolvedAtlas.clear();
		// The atlas may have changed, and every cell's UVs certainly have.
		fullDirty = true;
		collidersDirty = true;
	}

	void TileMap2D::SetResolvedAtlasPath(const std::string &absolute)
	{
		if (resolvedAtlas == absolute) return;
		resolvedAtlas = absolute;
		fullDirty = true;
	}

	void TileMap2D::SetTileSize(const Vec2 &s)
	{
		tileSize = s;
		// Every vertex moves, and so does every chunk's bounds - which is a
		// geometry-list-level change, not a refill.
		fullDirty = true;
		collidersDirty = true;
	}

	void TileMap2D::SetLit(const bool l)
	{
		if (lit == l) return;
		lit = l;
		fullDirty = true;
	}

	const TileMap2D::Chunk* TileMap2D::FindChunk(const int32 cx, const int32 cy) const
	{
		std::map<int64, Chunk>::const_iterator it = chunks.find(Key(cx, cy));
		return it == chunks.end() ? NULL : &it->second;
	}

	int32 TileMap2D::GetTile(const int32 x, const int32 y) const
	{
		const Chunk* c = FindChunk(ChunkOf(x), ChunkOf(y));
		if (!c) return EMPTY;
		const uint16 v = c->cells[(size_t)InChunk(y) * CHUNK + InChunk(x)];
		return v == 0 ? EMPTY : (int32)v - 1;
	}

	bool TileMap2D::SetTile(const int32 x, const int32 y, const int32 index)
	{
		const int32 cx = ChunkOf(x), cy = ChunkOf(y);
		const int64 key = Key(cx, cy);
		const uint16 store = (index < 0) ? 0 : (uint16)(index + 1);

		std::map<int64, Chunk>::iterator it = chunks.find(key);
		if (it == chunks.end())
		{
			// Painting nothing onto nothing is not an edit.
			if (store == 0) return false;
			Chunk fresh;
			fresh.cells.assign((size_t)CHUNK * CHUNK, 0);
			it = chunks.insert(std::make_pair(key, fresh)).first;
		}

		const size_t at = (size_t)InChunk(y) * CHUNK + InChunk(x);
		uint16 &cell = it->second.cells[at];
		if (cell == store) return false;

		const bool wasEmpty = (it->second.filled == 0);
		if (cell == 0 && store != 0) it->second.filled++;
		else if (cell != 0 && store == 0) it->second.filled--;
		cell = store;
		const bool nowEmpty = (it->second.filled == 0);

		// A chunk appearing or emptying changes the geometry LIST, which means
		// a new mesh set on the sibling component - not a buffer refill.
		if (wasEmpty || nowEmpty) fullDirty = true;
		else if (std::find(dirtyChunks.begin(), dirtyChunks.end(), key) == dirtyChunks.end())
			dirtyChunks.push_back(key);

		if (nowEmpty) chunks.erase(it);
		collidersDirty = true;
		return true;
	}

	int32 TileMap2D::Fill(const int32 x0, const int32 y0, const int32 x1, const int32 y1,
		const int32 index)
	{
		const int32 lox = std::min(x0, x1), hix = std::max(x0, x1);
		const int32 loy = std::min(y0, y1), hiy = std::max(y0, y1);
		int32 changed = 0;
		for (int32 y = loy; y <= hiy; y++)
			for (int32 x = lox; x <= hix; x++)
				if (SetTile(x, y, index)) changed++;
		return changed;
	}

	void TileMap2D::ClearTiles()
	{
		if (chunks.empty()) return;
		chunks.clear();
		dirtyChunks.clear();
		fullDirty = true;
		collidersDirty = true;
	}

	int32 TileMap2D::PaintedCount() const
	{
		int32 n = 0;
		for (std::map<int64, Chunk>::const_iterator i = chunks.begin(); i != chunks.end(); ++i)
			n += i->second.filled;
		return n;
	}

	std::vector<std::pair<int32, int32> > TileMap2D::NonEmptyChunks() const
	{
		std::vector<std::pair<int32, int32> > out;
		for (std::map<int64, Chunk>::const_iterator i = chunks.begin(); i != chunks.end(); ++i)
		{
			if (i->second.filled == 0) continue;
			out.push_back(std::make_pair((int32)(uint32)(i->first & 0xffffffff),
				(int32)(i->first >> 32)));
		}
		return out;
	}

	bool TileMap2D::GetTileBounds(int32 &minX, int32 &minY, int32 &maxX, int32 &maxY) const
	{
		bool any = false;
		for (std::map<int64, Chunk>::const_iterator i = chunks.begin(); i != chunks.end(); ++i)
		{
			if (i->second.filled == 0) continue;
			const int32 cx = (int32)(uint32)(i->first & 0xffffffff);
			const int32 cy = (int32)(i->first >> 32);
			for (int32 ly = 0; ly < CHUNK; ly++)
				for (int32 lx = 0; lx < CHUNK; lx++)
				{
					if (i->second.cells[(size_t)ly * CHUNK + lx] == 0) continue;
					const int32 x = cx * CHUNK + lx, y = cy * CHUNK + ly;
					if (!any) { minX = maxX = x; minY = maxY = y; any = true; }
					else {
						if (x < minX) minX = x;
						if (x > maxX) maxX = x;
						if (y < minY) minY = y;
						if (y > maxY) maxY = y;
					}
				}
		}
		return any;
	}

	Vec2 TileMap2D::TileToWorld(const int32 x, const int32 y) const
	{
		return Vec2(((f32)x + 0.5f) * tileSize.x, ((f32)y + 0.5f) * tileSize.y);
	}

	void TileMap2D::WorldToTile(const Vec2 &p, int32 &x, int32 &y) const
	{
		// floor, not truncate: a point at -0.5 with 1-unit cells is in tile
		// -1, and truncation would call it tile 0 along with everything up to
		// +1 - a two-cell-wide column at the origin that no other cell has.
		x = (tileSize.x != 0.f) ? (int32)std::floor(p.x / tileSize.x) : 0;
		y = (tileSize.y != 0.f) ? (int32)std::floor(p.y / tileSize.y) : 0;
	}

	std::vector<Vec4> TileMap2D::BuildColliderBoxes() const
	{
		std::vector<Vec4> out;

		int32 minX = 0, minY = 0, maxX = 0, maxY = 0;
		if (!GetTileBounds(minX, minY, maxX, maxY)) return out;

		const int32 w = maxX - minX + 1;
		const int32 h = maxY - minY + 1;
		if (w < 1 || h < 1) return out;

		// Flattened solid map over the painted extent only, so an empty
		// margin costs nothing. Consumed in place as rectangles are taken.
		std::vector<uchar> solid((size_t)w * h, 0);
		for (int32 y = 0; y < h; y++)
			for (int32 x = 0; x < w; x++)
			{
				const int32 t = GetTile(minX + x, minY + y);
				// Sloped cells are deliberately EXCLUDED from the merge. A
				// rectangle run that swallowed one would replace its triangle
				// with a full square - the slope would collide as a step, and
				// the merge is what made "just mark the tile solid" produce a
				// staircase. They come back as polygons in BuildColliderPolys.
				if (IsSolidCell(minX + x, minY + y)
					&& !(t >= 0 && tileset.HasOutline(t)))
					solid[(size_t)y * w + x] = 1;
			}

		for (int32 y = 0; y < h; y++)
		{
			for (int32 x = 0; x < w; x++)
			{
				if (!solid[(size_t)y * w + x]) continue;

				// Grow right while the row holds.
				int32 rw = 0;
				while (x + rw < w && solid[(size_t)y * w + x + rw]) rw++;

				// Then grow down while EVERY column of that run holds. A
				// partial row cannot be taken - the rectangle has to stay a
				// rectangle - so the rows below it are left for their own
				// pass.
				int32 rh = 1;
				while (y + rh < h)
				{
					bool full = true;
					for (int32 k = 0; k < rw && full; k++)
						full = solid[(size_t)(y + rh) * w + x + k] != 0;
					if (!full) break;
					rh++;
				}

				for (int32 j = 0; j < rh; j++)
					for (int32 i = 0; i < rw; i++)
						solid[(size_t)(y + j) * w + x + i] = 0;

				const f32 tx = (f32)(minX + x);
				const f32 ty = (f32)(minY + y);
				out.push_back(Vec4(
					(tx + rw * 0.5f) * tileSize.x,
					(ty + rh * 0.5f) * tileSize.y,
					rw * 0.5f * tileSize.x,
					rh * 0.5f * tileSize.y));
			}
		}
		return out;
	}

	// One convex polygon per sloped cell, in the map's local space - the same
	// space BuildColliderBoxes uses, so both go on the same body untouched.
	//
	// Wound counter-clockwise. Box2D computes its own hull so winding is not
	// strictly load-bearing, but a consistent order keeps these readable next
	// to the box list and makes a degenerate tile obvious.
	std::vector<std::vector<Vec2> > TileMap2D::BuildColliderPolys() const
	{
		std::vector<std::vector<Vec2> > out;

		int32 minX = 0, minY = 0, maxX = 0, maxY = 0;
		if (!GetTileBounds(minX, minY, maxX, maxY)) return out;

		for (int32 ty = minY; ty <= maxY; ty++)
		{
			for (int32 tx = minX; tx <= maxX; tx++)
			{
				const int32 t = GetTile(tx, ty);
				if (t < 0 || !IsSolidCell(tx, ty)) continue;
				if (!tileset.HasOutline(t)) continue;

				// The cell's own rect. Corners are named for what they are so
				// the four cases below read as pictures rather than algebra.
				const f32 x0 = (f32)tx * tileSize.x;
				const f32 x1 = (f32)(tx + 1) * tileSize.x;
				const f32 y0 = (f32)ty * tileSize.y;
				const f32 y1 = (f32)(ty + 1) * tileSize.y;
				const Vec2 bl(x0, y0), br(x1, y0), tl(x0, y1), tr(x1, y1);

				const int32 shape = tileset.Shape(t);

				// A CURVED floor is not one convex piece - a valley wall is
				// concave, and Box2D shapes must be convex - so it is emitted
				// as trapezoid columns under its height profile. One code path
				// would do for straight slopes too, but a ramp is exactly one
				// triangle and spending eight shapes on it would multiply the
				// collider count of an ordinary hill by eight.
				const bool isCeil = TileShape2DIsCeilProfile(shape);
				const bool custom = tileset.HasHeightProfile(t);
				if (custom || isCeil || shape == TileShape2D::ArcConvexBR
					|| shape == TileShape2D::ArcConvexBL
					|| shape == TileShape2D::ArcConcaveBR || shape == TileShape2D::ArcConcaveBL)
				{
					// A ceiling arc borrows the same curve and fills the other
					// side of it, so the column loop below only has to know
					// which end of each column is solid.
					const int32 profile = isCeil ? TileShape2DCeilBase(shape) : shape;
					const int32 kCols = 8;
					for (int32 c = 0; c < kCols; c++)
					{
						const f32 t0 = (f32)c / (f32)kCols;
						const f32 t1 = (f32)(c + 1) / (f32)kCols;
						f32 h0 = custom ? tileset.SurfaceHeight(t, t0)
							: TileShape2DHeight(profile, t0);
						f32 h1 = custom ? tileset.SurfaceHeight(t, t1)
							: TileShape2DHeight(profile, t1);
						if (isCeil)
						{
							// Solid from the curve UP to the cell top, so the
							// column's "height" is what is left above it.
							h0 = 1.f - h0;
							h1 = 1.f - h1;
						}
						// A column of zero height is no shape at all, and
						// b2ComputeHull would reject it anyway. Give the thin
						// end a floor so the surface stays continuous.
						const f32 kMinH = 0.02f;
						if (h0 < kMinH && h1 < kMinH) continue;
						if (h0 < kMinH) h0 = kMinH;
						if (h1 < kMinH) h1 = kMinH;

						const f32 cx0 = ((f32)tx + t0) * tileSize.x;
						const f32 cx1 = ((f32)tx + t1) * tileSize.x;
						std::vector<Vec2> col;
						if (isCeil)
						{
							// Hangs DOWN from the cell's top edge.
							const f32 top = (f32)(ty + 1) * tileSize.y;
							col.push_back(Vec2(cx0, top - h0 * tileSize.y));
							col.push_back(Vec2(cx1, top - h1 * tileSize.y));
							col.push_back(Vec2(cx1, top));
							col.push_back(Vec2(cx0, top));
						}
						else
						{
							const f32 cy = (f32)ty * tileSize.y;
							col.push_back(Vec2(cx0, cy));
							col.push_back(Vec2(cx1, cy));
							col.push_back(Vec2(cx1, cy + h1 * tileSize.y));
							col.push_back(Vec2(cx0, cy + h0 * tileSize.y));
						}
						out.push_back(col);
					}
					continue;
				}

				std::vector<Vec2> poly;
				switch (shape)
				{
					// Right angle bottom-right: floor rising to the right.
					case TileShape2D::SlopeBR:
						poly.push_back(bl); poly.push_back(br); poly.push_back(tr);
						break;
					// Right angle bottom-left: floor rising to the left.
					case TileShape2D::SlopeBL:
						poly.push_back(bl); poly.push_back(br); poly.push_back(tl);
						break;
					// Right angle top-right: ceiling.
					case TileShape2D::SlopeTR:
						poly.push_back(br); poly.push_back(tr); poly.push_back(tl);
						break;
					// Right angle top-left: ceiling.
					case TileShape2D::SlopeTL:
						poly.push_back(bl); poly.push_back(tr); poly.push_back(tl);
						break;
					default:
						continue;
				}
				out.push_back(poly);
			}
		}
		return out;
	}

	namespace {
		// Vertex identity for stitching. Quantised, because two cells must
		// agree on a shared corner exactly or the edge will not cancel and a
		// seam survives into the chain - which is the whole thing this is
		// here to remove. Tile corners are computed the same way from both
		// sides, so this only has to absorb float noise.
		typedef std::pair<int64, int64> VKey;
		static VKey KeyOf(const Vec2 &p)
		{
			return VKey((int64)llround((f64)p.x * 4096.0),
				(int64)llround((f64)p.y * 4096.0));
		}

		// One cell's solid outline in UNIT cell space, counter-clockwise.
		// Lives in TileSet2D so the collider and anything DRAWING a tile's
		// collision - the tileset sheet, the paint overlay - read one
		// definition and cannot show a different shape from the one you hit.
		static void CellOutline(const TileSet2D &set, const int32 tile,
			const int32 arcSegments, std::vector<Vec2> &out)
		{
			TileSet2DCellOutline(set, tile, arcSegments, out);
		}
	}

	void TileMap2D::SetSolidOverride(const int32 x, const int32 y, const uint8 mode)
	{
		const int64 k = Key(x, y);
		std::map<int64, uint8>::iterator it = solidOverride.find(k);
		const uint8 was = (it == solidOverride.end()) ? 0 : it->second;
		if (was == mode) return;
		if (mode == 0) { if (it != solidOverride.end()) solidOverride.erase(it); }
		else solidOverride[k] = mode;
		// The collider set just changed even though no tile did - without
		// this the map keeps the colliders it merged before the override.
		collidersDirty = true;
	}

	uint8 TileMap2D::GetSolidOverride(const int32 x, const int32 y) const
	{
		std::map<int64, uint8>::const_iterator it = solidOverride.find(Key(x, y));
		return it == solidOverride.end() ? 0 : it->second;
	}

	bool TileMap2D::IsSolidCell(const int32 x, const int32 y) const
	{
		const uint8 ov = GetSolidOverride(x, y);
		if (ov == 1) return true;
		if (ov == 2) return false;
		const int32 t = GetTile(x, y);
		return t >= 0 && tileset.IsSolid(t);
	}

	std::vector<Vec3> TileMap2D::SolidOverrides() const
	{
		std::vector<Vec3> out;
		out.reserve(solidOverride.size());
		for (std::map<int64, uint8>::const_iterator i = solidOverride.begin();
			i != solidOverride.end(); ++i)
		{
			// Key() is (y << 32) | (uint32)x - y in the HIGH half. Unpacking
			// them the other way round silently transposes the whole map.
			const int32 y = (int32)(i->first >> 32);
			const int32 x = (int32)(uint32)(i->first & 0xffffffffLL);
			out.push_back(Vec3((f32)x, (f32)y, (f32)i->second));
		}
		return out;
	}

	int32 TileMap2D::AutoGroupAt(const int32 x, const int32 y) const
	{
		const int32 t = GetTile(x, y);
		return t < 0 ? -1 : tileset.AutoTileForTile(t);
	}

	void TileMap2D::RefitAuto(const int32 x, const int32 y)
	{
		const int32 g = AutoGroupAt(x, y);
		if (g < 0) return;
		// bit 0 north, 1 east, 2 south, 3 west - a neighbour counts only when
		// it is the SAME group, so two terrains meeting get an edge each
		// rather than merging into one blob.
		int32 mask = 0;
		if (AutoGroupAt(x, y + 1) == g) mask |= 1;
		if (AutoGroupAt(x + 1, y) == g) mask |= 2;
		if (AutoGroupAt(x, y - 1) == g) mask |= 4;
		if (AutoGroupAt(x - 1, y) == g) mask |= 8;
		const int32 want = tileset.AutoTileAt(g, mask);
		if (want >= 0 && want != GetTile(x, y)) SetTile(x, y, want);
	}

	void TileMap2D::RefitAround(const int32 x, const int32 y)
	{
		RefitAuto(x, y + 1);
		RefitAuto(x + 1, y);
		RefitAuto(x, y - 1);
		RefitAuto(x - 1, y);
	}

	bool TileMap2D::SetTileAuto(const int32 x, const int32 y, const int32 group)
	{
		if (group < 0 || (size_t)group >= tileset.autotiles.size()) return false;
		// Provisional, so the cell is already a member of the group before the
		// mask is worked out - otherwise it would fit itself against a cell
		// that is not yet part of the terrain.
		SetTile(x, y, tileset.autotiles[(size_t)group].base);
		RefitAuto(x, y);
		RefitAround(x, y);
		return true;
	}

	bool TileMap2D::SetAnimationTime(const f32 seconds)
	{
		if (!tileset.HasAnims()) return false;
		animTime = seconds;

		// Which animations changed picture since the last build.
		const size_t n = tileset.anims.size();
		if (animLastFrame.size() != n) animLastFrame.assign(n, -1);
		std::vector<bool> changed(n, false);
		bool any = false;
		for (size_t i = 0; i < n; i++)
		{
			const int32 f = tileset.AnimFrameAt((int32)i, animTime);
			if (f != animLastFrame[i]) { animLastFrame[i] = f; changed[i] = true; any = true; }
		}
		if (!any) return false;

		// Only the chunks that contain one of those tiles. A level with one
		// torch in it should re-upload one chunk, not all of them.
		bool marked = false;
		for (std::map<int64, Chunk>::const_iterator it = chunks.begin();
			it != chunks.end(); ++it)
		{
			const Chunk &c = it->second;
			bool hit = false;
			for (size_t k = 0; k < c.cells.size() && !hit; k++)
			{
				if (c.cells[k] == 0) continue;
				const int32 a = tileset.AnimForTile((int32)c.cells[k] - 1);
				if (a >= 0 && (size_t)a < n && changed[(size_t)a]) hit = true;
			}
			if (hit && std::find(dirtyChunks.begin(), dirtyChunks.end(), it->first)
				== dirtyChunks.end())
			{ dirtyChunks.push_back(it->first); marked = true; }
		}
		return marked;
	}

	void TileMap2D::ClearSolidOverrides()
	{
		if (solidOverride.empty()) return;
		solidOverride.clear();
		collidersDirty = true;
	}

	std::vector<std::vector<Vec2> > TileMap2D::BuildColliderChains() const
	{
		std::vector<std::vector<Vec2> > loops;

		int32 minX = 0, minY = 0, maxX = 0, maxY = 0;
		if (!GetTileBounds(minX, minY, maxX, maxY)) return loops;

		// Directed boundary edges, keyed from->to. An edge shared by two solid
		// cells appears once in each direction, and the two cancel: everything
		// left is real boundary.
		std::map<std::pair<VKey, VKey>, int> edges;
		std::map<VKey, Vec2> coord;

		std::vector<Vec2> unit;
		for (int32 ty = minY; ty <= maxY; ty++)
		{
			for (int32 tx = minX; tx <= maxX; tx++)
			{
				const int32 t = GetTile(tx, ty);
				if (!IsSolidCell(tx, ty)) continue;
				// A forced-solid cell with no tile of its own collides as a
				// full square; there is no shape to read off an empty cell.
				CellOutline(tileset, t, 8, unit);
				if (unit.size() < 3) continue;

				for (size_t i = 0; i < unit.size(); i++)
				{
					const Vec2 &u0 = unit[i];
					const Vec2 &u1 = unit[(i + 1) % unit.size()];
					const Vec2 a(((f32)tx + u0.x) * tileSize.x, ((f32)ty + u0.y) * tileSize.y);
					const Vec2 b(((f32)tx + u1.x) * tileSize.x, ((f32)ty + u1.y) * tileSize.y);
					const VKey ka = KeyOf(a), kb = KeyOf(b);
					if (ka == kb) continue;          // zero-length, e.g. an arc
					coord[ka] = a; coord[kb] = b;

					std::map<std::pair<VKey, VKey>, int>::iterator rev
						= edges.find(std::make_pair(kb, ka));
					if (rev != edges.end())
					{
						// Interior: the neighbour owns the other side of it.
						if (--rev->second <= 0) edges.erase(rev);
					}
					else edges[std::make_pair(ka, kb)]++;
				}
			}
		}
		if (edges.empty()) return loops;

		// Stitch the survivors into closed loops by following each edge's end
		// vertex to the edge that starts there.
		std::multimap<VKey, VKey> next;
		for (std::map<std::pair<VKey, VKey>, int>::const_iterator i = edges.begin();
			i != edges.end(); ++i)
			for (int k = 0; k < i->second; k++)
				next.insert(std::make_pair(i->first.first, i->first.second));

		while (!next.empty())
		{
			const VKey start = next.begin()->first;
			VKey cur = start;
			std::vector<Vec2> loop;
			// Bounded so a malformed edge set cannot spin forever - but the
			// bound is captured BEFORE the walk. `next` is erased from as the
			// walk consumes edges, so testing against next.size() compares a
			// growing counter with a shrinking container: they meet in the
			// middle and the loop is abandoned about halfway round, leaving a
			// partial outline that closes with a straight line across the
			// level. Terrain built from it looks plausible and is not solid.
			const size_t maxSteps = next.size() + 4;
			for (size_t guard = 0; guard < maxSteps; guard++)
			{
				std::multimap<VKey, VKey>::iterator it = next.find(cur);
				if (it == next.end()) break;
				const VKey nxt = it->second;
				next.erase(it);
				loop.push_back(coord[cur]);
				cur = nxt;
				if (cur == start) break;
			}
			// Collinear runs cost a segment each and buy nothing; drop them so
			// a long flat floor is two points, not two hundred.
			if (loop.size() >= 3)
			{
				std::vector<Vec2> simple;
				for (size_t i = 0; i < loop.size(); i++)
				{
					const Vec2 &p = loop[(i + loop.size() - 1) % loop.size()];
					const Vec2 &c = loop[i];
					const Vec2 &n = loop[(i + 1) % loop.size()];
					const f32 cross = (c.x - p.x) * (n.y - c.y) - (c.y - p.y) * (n.x - c.x);
					if (std::fabs(cross) > 1e-6f) simple.push_back(c);
				}
				// Under three corners there is no area to collide with - a
				// walk that closed on itself immediately, or a loop that was
				// collinear all the way round.
				if (simple.size() >= 3)
				{
					// Box2D wants FOUR points for a closed chain, and a lone
					// triangle has three. Dropping it - which is what the old
					// `simple.size() >= 4` gate did - meant a ramp with nothing
					// under it, an isolated wedge, or a cell forced solid over
					// a slope tile produced NO collider at all: solid in the
					// tileset, drawn on screen, and walked straight through.
					// Splitting the longest edge costs one vertex and changes
					// the outline not at all.
					while (simple.size() < 4)
					{
						size_t at = 0;
						f32 best = -1.f;
						for (size_t i = 0; i < simple.size(); i++)
						{
							const Vec2 &a = simple[i];
							const Vec2 &b = simple[(i + 1) % simple.size()];
							const f32 d = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
							if (d > best) { best = d; at = i; }
						}
						const Vec2 &a = simple[at];
						const Vec2 &b = simple[(at + 1) % simple.size()];
						simple.insert(simple.begin() + (long)at + 1,
							Vec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f));
					}
					loops.push_back(simple);
				}
			}
		}
		return loops;
	}

	bool TileMap2D::SyncColliders()
	{
		collidersDirty = false;
		if (!Owner) return false;

		Physics2D* body = NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::Physics2D)
				body = static_cast<Physics2D*>(comps[i].get());
		if (!body) return false;

		// Terrain goes in as CHAIN LOOPS - one continuous outline with no
		// internal faces.
		//
		// Merged boxes and per-tile polygons are separate convex pieces that
		// meet edge to edge, and every one of those touching faces is a real
		// surface: a downward sensor ray can land on the vertical side of a
		// box and report a 90-degree "floor" in the middle of a gentle ramp.
		// That breaks any controller that steers by surface normal. A chain
		// has no internal faces - the edge shared by two solid cells is
		// cancelled in BuildColliderChains before the loop is built.
		//
		// PYROS_TILE_CHAINS=0 falls back to the old boxes+polygons path.
		static const char* chainEnv = getenv("PYROS_TILE_CHAINS");
		static const bool kUseChains = !(chainEnv != NULL && chainEnv[0] == '0');
		if (kUseChains)
		{
			const std::vector<std::vector<Vec2> > chains = BuildColliderChains();
			if (!chains.empty())
			{
				body->SetCompoundShapes(std::vector<Vec4>(),
					std::vector<Physics2D::Poly2D>(), chains);
				body->SetCastsShadow(false);
				return true;
			}
		}
		body->SetCompoundShapes(BuildColliderBoxes(), BuildColliderPolys(),
			std::vector<Physics2D::Poly2D>());

		// Forced, not left to the author. A map's merged rectangles are four
		// occluder segments each against a scene-wide budget of 32
		// (PYROS_MAX_OCCLUDERS_2D), so a map that cast would spend the whole
		// budget on itself and every prop in the scene would silently stop
		// casting. A map that wants a shadow gets an explicit Occluder2D.
		body->SetCastsShadow(false);
		return true;
	}

	bool TileMap2D::BuildChunkMesh(const int32 cx, const int32 cy, TileChunkMesh2D &out) const
	{
		const Chunk* c = FindChunk(cx, cy);
		if (!c || c->filled == 0) return false;

		out.vertex.clear();
		out.normal.clear();
		out.texcoord.clear();
		out.index.clear();
		out.tileCount = 0;

		out.vertex.reserve((size_t)c->filled * 4);
		out.normal.reserve((size_t)c->filled * 4);
		out.texcoord.reserve((size_t)c->filled * 4);
		out.index.reserve((size_t)c->filled * 6);

		const Vec3 n(0.f, 0.f, 1.f);

		for (int32 ly = 0; ly < CHUNK; ly++)
		{
			for (int32 lx = 0; lx < CHUNK; lx++)
			{
				const uint16 cell = c->cells[(size_t)ly * CHUNK + lx];
				if (cell == 0) continue;

				const int32 x = cx * CHUNK + lx;
				const int32 y = cy * CHUNK + ly;
				const f32 x0 = (f32)x * tileSize.x;
				const f32 y0 = (f32)y * tileSize.y;
				const f32 x1 = x0 + tileSize.x;
				const f32 y1 = y0 + tileSize.y;

				// (left, top, right, bottom) in texcoord space, v running down
				// the atlas. The quad's BOTTOM edge takes the rect's bottom v,
				// matching the winding every sprite in this engine uses
				// (SpriteRig2D's QuadGeometry).
				// The animated frame, when this tile starts one. The MAP still
				// stores the key tile; only the picture changes, so nothing
				// about collision, tags or serialization moves with it.
				int32 drawTile = (int32)cell - 1;
				const int32 anim = tileset.AnimForTile(drawTile);
				if (anim >= 0)
				{
					const int32 f = tileset.AnimFrameAt(anim, animTime);
					if (f >= 0) drawTile = f;
				}
				const Vec4 uv = tileset.UVRect(drawTile);

				const __INDEX_C_TYPE__ base = (__INDEX_C_TYPE__)out.vertex.size();

				out.vertex.push_back(Vec3(x0, y0, 0.f)); out.texcoord.push_back(Vec2(uv.x, uv.w));
				out.vertex.push_back(Vec3(x1, y0, 0.f)); out.texcoord.push_back(Vec2(uv.z, uv.w));
				out.vertex.push_back(Vec3(x1, y1, 0.f)); out.texcoord.push_back(Vec2(uv.z, uv.y));
				out.vertex.push_back(Vec3(x0, y1, 0.f)); out.texcoord.push_back(Vec2(uv.x, uv.y));
				for (int k = 0; k < 4; k++) out.normal.push_back(n);

				out.index.push_back(base + 0); out.index.push_back(base + 1); out.index.push_back(base + 2);
				out.index.push_back(base + 2); out.index.push_back(base + 3); out.index.push_back(base + 0);

				out.tileCount++;
			}
		}

		// The whole chunk, painted or not - see TileChunkMesh2D::minBounds.
		out.minBounds = Vec3((f32)(cx * CHUNK) * tileSize.x,
			(f32)(cy * CHUNK) * tileSize.y, 0.f);
		out.maxBounds = Vec3((f32)((cx + 1) * CHUNK) * tileSize.x,
			(f32)((cy + 1) * CHUNK) * tileSize.y, 0.f);

		return out.tileCount > 0;
	}

	RenderingComponent* TileMap2D::FindSibling() const
	{
		if (!Owner) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
				return static_cast<RenderingComponent*>(comps[i].get());
		return NULL;
	}

	bool TileMap2D::EnsureMaterial()
	{
		const std::string want = !resolvedAtlas.empty()
			? resolvedAtlas
			: (resolve ? resolve(tileset.image) : tileset.image);
		if (material && materialFor == want && materialLit == lit) return true;

		atlas = want.empty() ? std::shared_ptr<Texture>() : LoadAtlas(want);
		if (!atlas)
		{
			echo("WARNING: tilemap could not load atlas '" + tileset.image + "'");
			return false;
		}

		// Lighting2D is distance falloff with no N.L, which is what a flat
		// quad needs - with N.L a light in the map's own plane leaves it unlit.
		uint32 usage = ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::Texture;
		if (lit) usage |= ShaderUsage::Lighting2D;

		std::shared_ptr<GenericShaderMaterial> mat = std::make_shared<GenericShaderMaterial>(usage);
		mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
		mat->SetColorMap(atlas);
		mat->EnableBlending();
		mat->BlendingEquation(BlendEq::Add);
		mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		mat->SetTransparencyFlag(true);
		mat->SetCullFace(CullFace::DoubleSided);

		material = mat;
		materialFor = want;
		materialLit = lit;
		return true;
	}

	bool TileMap2D::Rebuild()
	{
		RenderingComponent* rc = FindSibling();
		if (!rc)
		{
			echo("WARNING: TileMap2D has no sibling RenderingComponent to draw through");
			return false;
		}
		if (!EnsureMaterial()) return false;

		// Box, not the RenderingMesh default of Sphere. CullingSphereTest
		// centres its sphere on the owner's world POSITION rather than on the
		// bounding sphere's own centre, so an object whose geometry does not
		// straddle its origin is tested against a sphere sitting in the wrong
		// place: a map painted out to x=160 vanished from about x=116
		// onwards, and the cutoff MOVED as the map grew, because the radius
		// grew with it. CullingBoxTest uses the real world-space box.
		//
		// A sphere is a poor fit for a long flat map regardless - for a
		// 200x60 one it covers more than three times the area.
		rc->SetCullingGeometry(CullingGeometry::Box);

		std::shared_ptr<TileMapRenderable> r = std::make_shared<TileMapRenderable>();
		std::vector<std::shared_ptr<IMaterial> > mats;
		chunkOrder.clear();

		// std::map order, so the geometry list is the same every rebuild and
		// chunkOrder can be indexed into.
		for (std::map<int64, Chunk>::const_iterator i = chunks.begin(); i != chunks.end(); ++i)
		{
			if (i->second.filled == 0) continue;
			const int32 cx = (int32)(uint32)(i->first & 0xffffffff);
			const int32 cy = (int32)(i->first >> 32);
			TileChunkMesh2D mesh;
			if (!BuildChunkMesh(cx, cy, mesh)) continue;
			r->AddChunk(mesh);
			mats.push_back(material);
			chunkOrder.push_back(i->first);
		}
		r->Finish();

		rc->AdoptGeneratedRenderable(r, mats);

		dirtyChunks.clear();
		fullDirty = false;
		return true;
	}

	void TileMap2D::RebuildDirtyInPlace()
	{
		RenderingComponent* rc = FindSibling();
		if (!rc || !rc->GetRenderable()) { fullDirty = true; return; }
		Renderable* r = rc->GetRenderable();

		for (size_t d = 0; d < dirtyChunks.size(); d++)
		{
			std::vector<int64>::iterator at =
				std::find(chunkOrder.begin(), chunkOrder.end(), dirtyChunks[d]);
			// A chunk nobody built a geometry for cannot be refilled - that is
			// the full-rebuild case, and reaching here means the two paths
			// disagreed about which one this edit was.
			if (at == chunkOrder.end()) { fullDirty = true; return; }
			const size_t slot = (size_t)(at - chunkOrder.begin());
			if (slot >= r->Geometries.size()) { fullDirty = true; return; }

			const int32 cx = (int32)(uint32)(dirtyChunks[d] & 0xffffffff);
			const int32 cy = (int32)(dirtyChunks[d] >> 32);
			TileChunkMesh2D mesh;
			if (!BuildChunkMesh(cx, cy, mesh)) { fullDirty = true; return; }

			static_cast<TileChunkGeometry*>(r->Geometries[slot])->Refill(mesh);
		}
		dirtyChunks.clear();
	}

	void TileMap2D::Update(const f64 time)
	{
		// Before the rebuild check, so a frame change is picked up by the same
		// pass that would have rebuilt an edit.
		SetAnimationTime((f32)time);

		if (fullDirty) Rebuild();
		else if (!dirtyChunks.empty())
		{
			RebuildDirtyInPlace();
			// RebuildDirtyInPlace can discover it needed the other path.
			if (fullDirty) Rebuild();
		}

		// After the mesh, and at a frame boundary - never mid-step. Rebuilding
		// a body inside the solver's step faults, and SetCompoundBoxes only
		// marks the body dirty when the merge actually came out different, so
		// an edit that does not change the solid set costs nothing here.
		if (collidersDirty) SyncColliders();
	}

};
