//============================================================================
// Name        : NavMesh.cpp
// Description : Where a character can walk, and the way there.
//============================================================================

#include <Pyros3D/Utils/Navigation/NavMesh.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace p3d {

	namespace {
		const unsigned short kWalk = 1;               // the one polygon flag: walkable
		const int kMaxPathPolys = 2048;
		const uint32 kFileMagic = 'P' << 24 | '3' << 16 | 'N' << 8 | 'M';
		const uint32 kFileVersion = 1;

		f32 Frand() { return (f32)rand() / ((f32)RAND_MAX + 1.f); }

		uint32 NextPow2(uint32 v) { v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; return v + 1; }
		uint32 Ilog2(uint32 v) { uint32 r = 0; while (v >>= 1) r++; return r; }

		// Everything a tile's build makes, freed together.
		struct TileBuild {
			rcHeightfield* solid = NULL; rcCompactHeightfield* chf = NULL; rcContourSet* cset = NULL;
			rcPolyMesh* pmesh = NULL; rcPolyMeshDetail* dmesh = NULL;
			~TileBuild() { rcFreeHeightField(solid); rcFreeCompactHeightfield(chf); rcFreeContourSet(cset); rcFreePolyMesh(pmesh); rcFreePolyMeshDetail(dmesh); }
		};
	}

	NavMesh::NavMesh() : navMesh(NULL), query(NULL), buildMs(0.0) {}
	NavMesh::~NavMesh() { dtFreeNavMeshQuery(query); dtFreeNavMesh(navMesh); }

	void NavMesh::ClearGeometry() { verts.clear(); indices.clear(); }
	void NavMesh::ClearLinks() { links.clear(); }

	void NavMesh::AddTriangles(const std::vector<Vec3> &vertices, const std::vector<uint32> &idx, const Matrix &world)
	{
		const int32 base = (int32)(verts.size() / 3);
		for (const Vec3 &v : vertices) { const Vec3 w = world * v; verts.push_back(w.x); verts.push_back(w.y); verts.push_back(w.z); }
		for (size_t k = 0; k + 2 < idx.size(); k += 3)
		{
			if (idx[k] >= vertices.size() || idx[k + 1] >= vertices.size() || idx[k + 2] >= vertices.size()) continue;
			indices.push_back(base + (int32)idx[k]); indices.push_back(base + (int32)idx[k + 1]); indices.push_back(base + (int32)idx[k + 2]);
		}
	}

	void NavMesh::AddObject(GameObject* go, const bool withChildren)
	{
		if (!go) return;
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t c = 0; c < comps.size(); c++)
		{
			if (!comps[c] || comps[c]->GetComponentType() != ComponentType::RenderingComponent) continue;
			RenderingComponent* rc = static_cast<RenderingComponent*>(comps[c].get());
			for (RenderingMesh* m : rc->GetMeshes(0))
			{
				if (!m || !m->Geometry || m->GetDrawingType() != DrawingType::Triangles) continue;
				ModelGeometry* mg = dynamic_cast<ModelGeometry*>(m->Geometry);
				if (mg && !mg->tBonesID.empty()) continue;         // (a skinned mesh walks; it is not walked on)
				const std::vector<Vec3> &v = m->Geometry->GetVertexData();
				const std::vector<__INDEX_C_TYPE__> &ix = m->Geometry->GetIndexData();
				std::vector<uint32> idx(ix.begin(), ix.end());
				AddTriangles(v, idx, go->GetWorldTransformation() * m->Pivot);
			}
		}
		if (withChildren)
			for (const std::shared_ptr<GameObject> &ch : go->GetChildren()) AddObject(ch.get(), true);
	}

	void NavMesh::AddScene(SceneGraph* scene, const bool staticOnly)
	{
		if (!scene) return;
		// (every object, children included, once: the list holds them all)
		std::vector<std::shared_ptr<GameObject> > &all = scene->GetAllGameObjectList();
		for (size_t i = 0; i < all.size(); i++)
			if (all[i] && (!staticOnly || all[i]->IsStatic())) AddObject(all[i].get(), false);
	}

	void NavMesh::AddLink(const Vec3 &a, const Vec3 &b, const f32 radius, const bool bidirectional)
	{
		Link l; l.a = a; l.b = b; l.radius = radius > 0.05f ? radius : 0.05f; l.both = bidirectional;
		links.push_back(l);
	}

	uint32 NavMesh::GetTileCount() const
	{
		if (!navMesh) return 0;
		const dtNavMesh* nm = navMesh;
		uint32 n = 0;
		for (int i = 0; i < nm->getMaxTiles(); i++) { const dtMeshTile* t = nm->getTile(i); if (t && t->header) n++; }
		return n;
	}

	uint32 NavMesh::GetPolygonCount() const
	{
		if (!navMesh) return 0;
		const dtNavMesh* nm = navMesh;
		uint32 n = 0;
		for (int i = 0; i < nm->getMaxTiles(); i++) { const dtMeshTile* t = nm->getTile(i); if (t && t->header) n += (uint32)t->header->polyCount; }
		return n;
	}

	bool NavMesh::Build(const NavMeshSettings &s)
	{
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		settings = s;
		dtFreeNavMeshQuery(query); query = NULL;
		dtFreeNavMesh(navMesh); navMesh = NULL;
		const int nverts = (int)(verts.size() / 3), ntris = (int)(indices.size() / 3);
		if (ntris == 0) { fprintf(stderr, "NavMesh: nothing to build from (no triangles)\n"); return false; }

		f32 bmin[3], bmax[3];
		rcCalcBounds(&verts[0], nverts, bmin, bmax);

		const f32 cs = s.cellSize > 0.01f ? s.cellSize : 0.01f, ch = s.cellHeight > 0.01f ? s.cellHeight : 0.01f;
		const int tileCells = (int)ceilf((s.tileSize > 1.f ? s.tileSize : 1.f) / cs);
		const f32 tileWorld = tileCells * cs;
		int gw = 0, gh = 0;
		rcCalcGridSize(bmin, bmax, cs, &gw, &gh);
		const int tw = (gw + tileCells - 1) / tileCells, th = (gh + tileCells - 1) / tileCells;

		rcConfig base;
		memset(&base, 0, sizeof(base));
		base.cs = cs; base.ch = ch;
		base.walkableSlopeAngle = s.agentMaxSlope;
		base.walkableHeight = (int)ceilf(s.agentHeight / ch);
		base.walkableClimb = (int)floorf(s.agentMaxClimb / ch);
		base.walkableRadius = (int)ceilf(s.agentRadius / cs);
		base.maxEdgeLen = (int)(s.edgeMaxLength / cs);
		base.maxSimplificationError = s.edgeMaxError;
		base.minRegionArea = (int)rcSqr(s.regionMinSize);
		base.mergeRegionArea = (int)rcSqr(s.regionMergeSize);
		base.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
		base.tileSize = tileCells;
		base.borderSize = base.walkableRadius + 3;
		base.width = tileCells + base.borderSize * 2;
		base.height = tileCells + base.borderSize * 2;
		base.detailSampleDist = s.detailSampleDistance < 0.9f ? 0.f : cs * s.detailSampleDistance;
		base.detailSampleMaxError = ch * s.detailSampleMaxError;

		// Which triangles each tile needs (its own square and the border round it): asking every
		// tile about every triangle is tiles x triangles, and a big world has many of both.
		const f32 border = base.borderSize * cs;
		std::vector<std::vector<int> > tileTris((size_t)tw * th);
		for (int t = 0; t < ntris; t++)
		{
			const f32* a = &verts[indices[t * 3] * 3]; const f32* b = &verts[indices[t * 3 + 1] * 3]; const f32* c = &verts[indices[t * 3 + 2] * 3];
			const f32 minx = fminf(a[0], fminf(b[0], c[0])) - border, maxx = fmaxf(a[0], fmaxf(b[0], c[0])) + border;
			const f32 minz = fminf(a[2], fminf(b[2], c[2])) - border, maxz = fmaxf(a[2], fmaxf(b[2], c[2])) + border;
			int x0 = (int)floorf((minx - bmin[0]) / tileWorld), x1 = (int)floorf((maxx - bmin[0]) / tileWorld);
			int z0 = (int)floorf((minz - bmin[2]) / tileWorld), z1 = (int)floorf((maxz - bmin[2]) / tileWorld);
			x0 = x0 < 0 ? 0 : x0; z0 = z0 < 0 ? 0 : z0; x1 = x1 >= tw ? tw - 1 : x1; z1 = z1 >= th ? th - 1 : z1;
			for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) tileTris[(size_t)z * tw + x].push_back(t);
		}

		std::vector<std::pair<unsigned char*, int> > tileData((size_t)tw * th, std::make_pair((unsigned char*)NULL, 0));
		std::vector<Link> lk(links);
		JobSystem::Instance().ParallelFor((uint32)(tw * th), 1, [&](const uint32 begin, const uint32 end) {
			rcContext ctx(false);                              // (one each: a context is not shared)
			std::vector<int> tris; std::vector<unsigned char> areas;
			for (uint32 ti = begin; ti < end; ti++)
			{
				const std::vector<int> &mine = tileTris[ti];
				if (mine.empty()) continue;
				const int tx = (int)(ti % tw), tz = (int)(ti / tw);
				rcConfig cfg = base;
				rcVcopy(cfg.bmin, bmin); rcVcopy(cfg.bmax, bmax);
				cfg.bmin[0] = bmin[0] + tx * tileWorld; cfg.bmin[2] = bmin[2] + tz * tileWorld;
				cfg.bmax[0] = bmin[0] + (tx + 1) * tileWorld; cfg.bmax[2] = bmin[2] + (tz + 1) * tileWorld;
				cfg.bmin[0] -= border; cfg.bmin[2] -= border; cfg.bmax[0] += border; cfg.bmax[2] += border;

				TileBuild b;
				b.solid = rcAllocHeightfield();
				if (!b.solid || !rcCreateHeightfield(&ctx, *b.solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) continue;
				tris.resize(mine.size() * 3); areas.assign(mine.size(), 0);
				for (size_t k = 0; k < mine.size(); k++) { tris[k * 3] = indices[mine[k] * 3]; tris[k * 3 + 1] = indices[mine[k] * 3 + 1]; tris[k * 3 + 2] = indices[mine[k] * 3 + 2]; }
				rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, &verts[0], nverts, &tris[0], (int)mine.size(), &areas[0]);
				if (!rcRasterizeTriangles(&ctx, &verts[0], nverts, &tris[0], &areas[0], (int)mine.size(), *b.solid, cfg.walkableClimb)) continue;
				rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *b.solid);
				rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *b.solid);
				rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *b.solid);
				b.chf = rcAllocCompactHeightfield();
				if (!b.chf || !rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *b.solid, *b.chf)) continue;
				if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *b.chf)) continue;
				if (!rcBuildDistanceField(&ctx, *b.chf)) continue;
				if (!rcBuildRegions(&ctx, *b.chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea)) continue;
				b.cset = rcAllocContourSet();
				if (!b.cset || !rcBuildContours(&ctx, *b.chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *b.cset)) continue;
				if (b.cset->nconts == 0) continue;
				b.pmesh = rcAllocPolyMesh();
				if (!b.pmesh || !rcBuildPolyMesh(&ctx, *b.cset, cfg.maxVertsPerPoly, *b.pmesh)) continue;
				b.dmesh = rcAllocPolyMeshDetail();
				if (!b.dmesh || !rcBuildPolyMeshDetail(&ctx, *b.pmesh, *b.chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *b.dmesh)) continue;
				if (b.pmesh->nverts >= 0xffff) continue;           // (Detour indexes a tile's vertices in 16 bits)
				for (int p = 0; p < b.pmesh->npolys; p++)
					if (b.pmesh->areas[p] == RC_WALKABLE_AREA) b.pmesh->flags[p] = kWalk;

				// The links that start in this tile (Detour keeps each with its start).
				std::vector<f32> lv; std::vector<f32> lr; std::vector<unsigned short> lf; std::vector<unsigned char> la, ld; std::vector<unsigned int> lid;
				for (size_t l = 0; l < lk.size(); l++)
				{
					const Vec3 &a = lk[l].a;
					if (a.x < cfg.bmin[0] + border || a.x >= cfg.bmax[0] - border || a.z < cfg.bmin[2] + border || a.z >= cfg.bmax[2] - border) continue;
					lv.push_back(a.x); lv.push_back(a.y); lv.push_back(a.z);
					lv.push_back(lk[l].b.x); lv.push_back(lk[l].b.y); lv.push_back(lk[l].b.z);
					lr.push_back(lk[l].radius); lf.push_back(kWalk); la.push_back(RC_WALKABLE_AREA);
					ld.push_back(lk[l].both ? DT_OFFMESH_CON_BIDIR : 0); lid.push_back((unsigned int)(1000 + l));
				}

				dtNavMeshCreateParams params;
				memset(&params, 0, sizeof(params));
				params.verts = b.pmesh->verts; params.vertCount = b.pmesh->nverts;
				params.polys = b.pmesh->polys; params.polyAreas = b.pmesh->areas; params.polyFlags = b.pmesh->flags;
				params.polyCount = b.pmesh->npolys; params.nvp = b.pmesh->nvp;
				params.detailMeshes = b.dmesh->meshes; params.detailVerts = b.dmesh->verts; params.detailVertsCount = b.dmesh->nverts;
				params.detailTris = b.dmesh->tris; params.detailTriCount = b.dmesh->ntris;
				if (!lr.empty())
				{
					params.offMeshConVerts = &lv[0]; params.offMeshConRad = &lr[0]; params.offMeshConFlags = &lf[0];
					params.offMeshConAreas = &la[0]; params.offMeshConDir = &ld[0]; params.offMeshConUserID = &lid[0];
					params.offMeshConCount = (int)lr.size();
				}
				params.walkableHeight = s.agentHeight; params.walkableRadius = s.agentRadius; params.walkableClimb = s.agentMaxClimb;
				params.tileX = tx; params.tileY = tz; params.tileLayer = 0;
				rcVcopy(params.bmin, b.pmesh->bmin); rcVcopy(params.bmax, b.pmesh->bmax);
				params.cs = cfg.cs; params.ch = cfg.ch;
				params.buildBvTree = true;
				unsigned char* data = NULL; int size = 0;
				if (dtCreateNavMeshData(&params, &data, &size)) tileData[ti] = std::make_pair(data, size);
			}
		});

		// One mesh of tw x th tiles, each tile's polygons numbered in the bits the tiles leave.
		dtNavMeshParams np;
		memset(&np, 0, sizeof(np));
		rcVcopy(np.orig, bmin);
		np.tileWidth = tileWorld; np.tileHeight = tileWorld;
		uint32 tileBits = Ilog2(NextPow2((uint32)(tw * th)));
		if (tileBits > 14) tileBits = 14;
		const uint32 polyBits = 22 - tileBits;
		np.maxTiles = 1 << tileBits; np.maxPolys = 1 << polyBits;
		navMesh = dtAllocNavMesh();
		if (!navMesh || dtStatusFailed(navMesh->init(&np)))
		{
			for (std::pair<unsigned char*, int> &d : tileData) if (d.first) dtFree(d.first);
			dtFreeNavMesh(navMesh); navMesh = NULL;
			return false;
		}
		uint32 added = 0;
		for (std::pair<unsigned char*, int> &d : tileData)
		{
			if (!d.first) continue;
			if (dtStatusSucceed(navMesh->addTile(d.first, d.second, DT_TILE_FREE_DATA, 0, NULL))) added++;
			else dtFree(d.first);
		}
		buildMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
		if (getenv("PYROS_NAV_TRACE"))
			fprintf(stderr, "[nav] %d triangles -> %u tiles (%dx%d grid), %u polygons, %zu links, %.1f ms\n",
				ntris, added, tw, th, GetPolygonCount(), links.size(), buildMs);
		if (added == 0) { dtFreeNavMesh(navMesh); navMesh = NULL; fprintf(stderr, "NavMesh: nothing walkable\n"); return false; }
		return InitQuery();
	}

	bool NavMesh::InitQuery()
	{
		dtFreeNavMeshQuery(query);
		query = dtAllocNavMeshQuery();
		if (!query || dtStatusFailed(query->init(navMesh, 4096))) { dtFreeNavMeshQuery(query); query = NULL; return false; }
		return true;
	}

	bool NavMesh::NearestPoint(const Vec3 &p, Vec3 &out, const Vec3 &extent) const
	{
		if (!query) return false;
		dtQueryFilter filter; filter.setIncludeFlags(kWalk);
		const f32 pos[3] = { p.x, p.y, p.z }, ext[3] = { extent.x, extent.y, extent.z };
		dtPolyRef ref = 0; f32 nearest[3];
		if (dtStatusFailed(query->findNearestPoly(pos, ext, &filter, &ref, nearest)) || !ref) return false;
		out = Vec3(nearest[0], nearest[1], nearest[2]);
		return true;
	}

	bool NavMesh::FindPath(const Vec3 &from, const Vec3 &to, std::vector<Vec3> &path, const Vec3 &extent) const
	{
		path.clear(); lastLinkFlags.clear();
		if (!query) return false;
		dtQueryFilter filter; filter.setIncludeFlags(kWalk);
		const f32 ext[3] = { extent.x, extent.y, extent.z };
		const f32 a[3] = { from.x, from.y, from.z }, b[3] = { to.x, to.y, to.z };
		dtPolyRef startRef = 0, endRef = 0; f32 sp[3], ep[3];
		query->findNearestPoly(a, ext, &filter, &startRef, sp);
		query->findNearestPoly(b, ext, &filter, &endRef, ep);
		if (!startRef) return false;
		const bool endOnMesh = endRef != 0;                  // (else: as near as it gets, and false)
		if (!endRef) { endRef = startRef; dtVcopy(ep, sp); }
		std::vector<dtPolyRef> polys(kMaxPathPolys);
		int npolys = 0;
		const dtStatus st = query->findPath(startRef, endRef, sp, ep, &filter, &polys[0], &npolys, kMaxPathPolys);
		if (dtStatusFailed(st) || npolys == 0) return false;
		const bool reached = endOnMesh && polys[npolys - 1] == endRef && !dtStatusDetail(st, DT_PARTIAL_RESULT);
		// (got as near as it can: the closest point of the last polygon reached)
		if (polys[npolys - 1] != endRef) query->closestPointOnPoly(polys[npolys - 1], b, ep, NULL);
		std::vector<f32> straight(kMaxPathPolys * 3);
		std::vector<unsigned char> flags(kMaxPathPolys);
		std::vector<dtPolyRef> refs(kMaxPathPolys);
		int n = 0;
		if (dtStatusFailed(query->findStraightPath(sp, ep, &polys[0], npolys, &straight[0], &flags[0], &refs[0], &n, kMaxPathPolys))) return false;
		for (int i = 0; i < n; i++)
		{
			path.push_back(Vec3(straight[i * 3], straight[i * 3 + 1], straight[i * 3 + 2]));
			lastLinkFlags.push_back((flags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ? 1 : 0);
		}
		return reached;
	}

	bool NavMesh::Raycast(const Vec3 &from, const Vec3 &to, Vec3 &hit) const
	{
		hit = from;
		if (!query) return false;
		dtQueryFilter filter; filter.setIncludeFlags(kWalk);
		const f32 ext[3] = { 2.f, 4.f, 2.f };
		const f32 a[3] = { from.x, from.y, from.z }, b[3] = { to.x, to.y, to.z };
		dtPolyRef startRef = 0; f32 sp[3];
		if (dtStatusFailed(query->findNearestPoly(a, ext, &filter, &startRef, sp)) || !startRef) return false;
		f32 t = 0.f, normal[3];
		dtPolyRef polys[256]; int npolys = 0;
		if (dtStatusFailed(query->raycast(startRef, sp, b, &filter, &t, normal, polys, &npolys, 256))) return false;
		if (t >= 1.f || t == FLT_MAX) { hit = to; return true; }
		f32 p[3] = { sp[0] + (b[0] - sp[0]) * t, sp[1] + (b[1] - sp[1]) * t, sp[2] + (b[2] - sp[2]) * t };
		if (npolys > 0) { f32 h = p[1]; if (dtStatusSucceed(query->getPolyHeight(polys[npolys - 1], p, &h))) p[1] = h; }
		hit = Vec3(p[0], p[1], p[2]);
		return false;
	}

	bool NavMesh::RandomPoint(Vec3 &out, const Vec3 &around, const f32 radius) const
	{
		if (!query) return false;
		dtQueryFilter filter; filter.setIncludeFlags(kWalk);
		dtPolyRef ref = 0; f32 pt[3];
		if (radius > 0.f)
		{
			const f32 c[3] = { around.x, around.y, around.z }, ext[3] = { 2.f, 4.f, 2.f };
			dtPolyRef startRef = 0; f32 sp[3];
			if (dtStatusFailed(query->findNearestPoly(c, ext, &filter, &startRef, sp)) || !startRef) return false;
			if (dtStatusFailed(query->findRandomPointAroundCircle(startRef, sp, radius, &filter, Frand, &ref, pt))) return false;
		}
		else if (dtStatusFailed(query->findRandomPoint(&filter, Frand, &ref, pt))) return false;
		out = Vec3(pt[0], pt[1], pt[2]);
		return true;
	}

	void NavMesh::GetDebugTriangles(std::vector<Vec3> &out) const
	{
		out.clear();
		if (!navMesh) return;
		const dtNavMesh* nm = navMesh;
		for (int i = 0; i < nm->getMaxTiles(); i++)
		{
			const dtMeshTile* tile = nm->getTile(i);
			if (!tile || !tile->header) continue;
			for (int p = 0; p < tile->header->polyCount; p++)
			{
				const dtPoly* poly = &tile->polys[p];
				if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
				const dtPolyDetail* pd = &tile->detailMeshes[p];
				for (int j = 0; j < pd->triCount; j++)
				{
					const unsigned char* t = &tile->detailTris[(pd->triBase + j) * 4];
					for (int k = 0; k < 3; k++)
					{
						const f32* v = t[k] < poly->vertCount ? &tile->verts[poly->verts[t[k]] * 3] : &tile->detailVerts[(pd->vertBase + t[k] - poly->vertCount) * 3];
						out.push_back(Vec3(v[0], v[1], v[2]));
					}
				}
			}
		}
	}

	// File: magic, version, the settings it was built with, the mesh's parameters, then each tile
	// (its reference and its data, as Detour made it).
	bool NavMesh::Save(const std::string &path) const
	{
		if (!navMesh) return false;
		FILE* f = fopen(path.c_str(), "wb");
		if (!f) return false;
		const dtNavMesh* nm = navMesh;
		uint32 count = GetTileCount();
		fwrite(&kFileMagic, sizeof(uint32), 1, f); fwrite(&kFileVersion, sizeof(uint32), 1, f);
		fwrite(&settings, sizeof(NavMeshSettings), 1, f);
		fwrite(nm->getParams(), sizeof(dtNavMeshParams), 1, f);
		fwrite(&count, sizeof(uint32), 1, f);
		for (int i = 0; i < nm->getMaxTiles(); i++)
		{
			const dtMeshTile* t = nm->getTile(i);
			if (!t || !t->header || !t->dataSize) continue;
			const dtTileRef ref = nm->getTileRef(t);
			const int32 size = t->dataSize;
			fwrite(&ref, sizeof(dtTileRef), 1, f); fwrite(&size, sizeof(int32), 1, f);
			fwrite(t->data, (size_t)size, 1, f);
		}
		const bool ok = ferror(f) == 0;
		fclose(f);
		return ok;
	}

	bool NavMesh::Load(const std::string &path)
	{
		FILE* f = fopen(path.c_str(), "rb");
		if (!f) return false;
		uint32 magic = 0, version = 0, count = 0;
		NavMeshSettings s; dtNavMeshParams np;
		bool ok = fread(&magic, sizeof(uint32), 1, f) == 1 && magic == kFileMagic
			&& fread(&version, sizeof(uint32), 1, f) == 1 && version == kFileVersion
			&& fread(&s, sizeof(NavMeshSettings), 1, f) == 1
			&& fread(&np, sizeof(dtNavMeshParams), 1, f) == 1
			&& fread(&count, sizeof(uint32), 1, f) == 1;
		dtNavMesh* nm = ok ? dtAllocNavMesh() : NULL;
		if (ok && (!nm || dtStatusFailed(nm->init(&np)))) ok = false;
		for (uint32 i = 0; ok && i < count; i++)
		{
			dtTileRef ref = 0; int32 size = 0;
			if (fread(&ref, sizeof(dtTileRef), 1, f) != 1 || fread(&size, sizeof(int32), 1, f) != 1 || size <= 0) { ok = false; break; }
			unsigned char* data = (unsigned char*)dtAlloc((size_t)size, DT_ALLOC_PERM);
			if (!data || fread(data, (size_t)size, 1, f) != 1) { dtFree(data); ok = false; break; }
			if (dtStatusFailed(nm->addTile(data, size, DT_TILE_FREE_DATA, ref, NULL))) { dtFree(data); ok = false; break; }
		}
		fclose(f);
		if (!ok) { dtFreeNavMesh(nm); return false; }
		dtFreeNavMeshQuery(query); query = NULL;
		dtFreeNavMesh(navMesh);
		navMesh = nm; settings = s;
		return InitQuery();
	}
}
