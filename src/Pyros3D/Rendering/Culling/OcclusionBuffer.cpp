//============================================================================
// Name        : OcclusionBuffer.cpp
// Description : What the big things in view hide. See OcclusionBuffer.h.
//============================================================================

#include <set>
#include <Pyros3D/Rendering/Culling/OcclusionBuffer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace p3d {

	namespace {
		int g_enabled = -1;
		const f32 kFar = std::numeric_limits<f32>::infinity();

		// The level an occluder is drawn with: its coarsest.
		uint32 CoarsestLevel(RenderingComponent* rc)
		{
			const uint32 n = rc->GetLODSize();
			for (uint32 l = n; l-- > 0;) if (!rc->GetMeshes(l).empty()) return l;
			return 0;
		}
	}

	void OcclusionBuffer::SetEnabled(const bool on) { g_enabled = on ? 1 : 0; }
	bool OcclusionBuffer::GetEnabled()
	{
		if (g_enabled < 0) { const char* e = getenv("PYROS_OCCLUSION"); g_enabled = (e && e[0] == '0') ? 0 : 1; }
		return g_enabled == 1;
	}

	void OcclusionBuffer::Begin(const Matrix &view, const Matrix &projection, const Vec3 &eyeAt, const f32 nearAt)
	{
		frame++;
		viewProjection = projection * view;
		eye = eyeAt;
		nearPlane = std::max(nearAt, 1e-3f);
		// (pixels per unit of x/w and y/w: what a sphere's radius becomes on the picture)
		focalX = projection.m[0] * 0.5f * (f32)kWidth;
		focalY = projection.m[5] * 0.5f * (f32)kHeight;
		if (depth.empty())
		{
			uint32 w = kWidth, h = kHeight;
			while (true) { depth.push_back(std::vector<f32>((size_t)w * h)); if (w == 1 && h == 1) break; w = std::max(1u, w / 2); h = std::max(1u, h / 2); }
		}
		std::fill(depth[0].begin(), depth[0].end(), kFar);
		screen.clear();
		trianglesDrawn = occludersDrawn = 0;
		ready = false;
		// (what was kept for occluders not drawn for a while goes)
		if ((frame & 255) == 0)
			for (std::map<RenderingComponent*, Kept>::iterator i = kept.begin(); i != kept.end();)
				if (i->second.lastUsed + 600 < frame) kept.erase(i++); else ++i;
		// (and what was known of things that may be gone: asked again)
		if ((frame & 4095) == 0) canOcclude.clear();
	}

	// PYROS_OCCLUSION_TRACE=1: why each candidate (static, big enough) does or does not occlude -
	// once each, the first sixty, and how many were not static or too small
	namespace {
		const bool g_occlusionTrace = std::getenv("PYROS_OCCLUSION_TRACE") != NULL;
		void Why(RenderingComponent* rc, const char* why, const size_t tris)
		{
			static int lines = 0;
			if (!g_occlusionTrace || lines >= 60) return;
			lines++;
			fprintf(stderr, "[occluder] %s: %s (%zu triangles)\n", rc && rc->GetOwner() ? rc->GetOwner()->GetName().c_str() : "?", why, tris);
		}
	}

	bool OcclusionBuffer::CanOcclude(RenderingComponent* rc, const f32 sphereRadius)
	{
		if (rc == NULL || rc->GetOwner() == NULL) return false;
		// Still: marked static, or not moved of late (asleep in the scene's walk) - the triangles kept
		// are made again whenever its place changes, so what matters is that it is not moving now. Only
		// the static flag was asked: in Erangel nothing but the terrain carries it, and nothing occluded.
		const bool still = rc->GetOwner()->IsStatic() || !rc->GetOwner()->IsAwake();
		if (!still || sphereRadius < 1.5f)
		{
			if (g_occlusionTrace)
			{
				static std::set<RenderingComponent*> seen;
				static uint32 notStatic = 0, small = 0;
				if (seen.insert(rc).second)
				{
					(!still ? notStatic : small)++;
					if ((notStatic + small) % 500 == 0)
						fprintf(stderr, "[occluder] so far %u not static, %u under 1.5 m\n", notStatic, small);
					if (!still && sphereRadius >= 1.5f && notStatic <= 20)
						fprintf(stderr, "[occluder] %s: moving (radius %.1f)\n", rc->GetOwner()->GetName().c_str(), sphereRadius);
				}
			}
			return false;                                                // (a thing smaller than a door hides little)
		}
		std::map<RenderingComponent*, bool>::iterator known = canOcclude.find(rc);
		if (known != canOcclude.end()) return known->second;
		bool &answer = canOcclude[rc];
		answer = false;
		if (rc->IsInstanced()) { Why(rc, "instanced", 0); return false; }
		const std::vector<RenderingMesh*> &meshes = rc->GetMeshes(CoarsestLevel(rc));
		size_t tris = 0;
		for (size_t m = 0; m < meshes.size(); m++)
		{
			RenderingMesh* rm = meshes[m];
			if (rm == NULL || rm->Geometry == NULL || !rm->Material) { Why(rc, "no geometry or material", 0); return false; }
			// (see through it, or cut out: what it covers may show)
			if (rm->Material->IsTransparent() || rm->Geometry->materialProperties.haveBones || !rm->MapBoneIDs.empty()) { Why(rc, rm->Material->IsTransparent() ? "a material seen through" : "skinned", 0); return false; }
			// (only a plain material: cut-out leaves, a fence, a tree's card - holes to see through; and
			// a material of its own shader may move its vertices)
			GenericShaderMaterial* g = dynamic_cast<GenericShaderMaterial*>(rm->Material.get());
			if (g == NULL || (g->GetOptions() & (ShaderUsage::AlphaTest | ShaderUsage::VertexWind))) { Why(rc, g == NULL ? "a material of its own shader" : "cut out or in the wind", 0); return false; }
			tris += rm->Geometry->GetIndexData().size() / 3;
		}
		answer = tris > 0 && tris <= kMaxOccluderTriangles;
		Why(rc, answer ? "occludes" : "too many triangles at its coarsest level", tris);
		return answer;
	}

	uint32 OcclusionBuffer::Draw(RenderingComponent* rc)
	{
		if (trianglesDrawn >= kTriangleBudget || occludersDrawn >= kMaxOccluders) return 0;
		const Matrix &world = rc->GetOwner()->GetWorldTransformation();
		Kept &k = kept[rc];
		k.lastUsed = frame;
		if (k.triangles.empty() || memcmp(&k.world, &world, sizeof(Matrix)) != 0)
		{
			k.world = world;
			k.triangles.clear();
			const std::vector<RenderingMesh*> &meshes = rc->GetMeshes(CoarsestLevel(rc));
			for (size_t m = 0; m < meshes.size(); m++)
			{
				const std::vector<__INDEX_C_TYPE__> &idx = meshes[m]->Geometry->GetIndexData();
				const std::vector<Vec3> &pos = meshes[m]->Geometry->GetVertexData();
				for (size_t i = 0; i + 2 < idx.size(); i += 3)
				{
					if (idx[i] >= pos.size() || idx[i + 1] >= pos.size() || idx[i + 2] >= pos.size()) continue;
					for (int v = 0; v < 3; v++) k.triangles.push_back((world * Vec4(pos[idx[i + v]], 1.f)).xyz());
				}
			}
		}
		const uint32 n = (uint32)(k.triangles.size() / 3);
		for (uint32 t = 0; t < n; t++)
		{
			const Vec4 a = viewProjection * Vec4(k.triangles[t * 3 + 0], 1.f);
			const Vec4 b = viewProjection * Vec4(k.triangles[t * 3 + 1], 1.f);
			const Vec4 c = viewProjection * Vec4(k.triangles[t * 3 + 2], 1.f);
			DrawTriangle(a, b, c);
		}
		trianglesDrawn += n;
		occludersDrawn++;
		return n;
	}

	void OcclusionBuffer::DrawTriangle(const Vec4 &a, const Vec4 &b, const Vec4 &c)
	{
		// (wholly beyond one side of the view: nothing to draw)
		if ((a.x > a.w && b.x > b.w && c.x > c.w) || (a.x < -a.w && b.x < -b.w && c.x < -c.w)) return;
		if ((a.y > a.w && b.y > b.w && c.y > c.w) || (a.y < -a.w && b.y < -b.w && c.y < -c.w)) return;
		const bool ina = a.w >= nearPlane, inb = b.w >= nearPlane, inc = c.w >= nearPlane;
		if (ina && inb && inc) { Raster(a, b, c); return; }
		if (!ina && !inb && !inc) return;
		// Cut at the near plane (the wall beside the camera is the occluder that matters most):
		// what is left is a triangle or a quad.
		const Vec4 v[3] = { a, b, c };
		Vec4 out[4]; int n = 0;
		for (int i = 0; i < 3; i++)
		{
			const Vec4 &p = v[i], &q = v[(i + 1) % 3];
			const bool pin = p.w >= nearPlane, qin = q.w >= nearPlane;
			if (pin) out[n++] = p;
			if (pin != qin)
			{
				const f32 t = (nearPlane - p.w) / (q.w - p.w);
				out[n++] = Vec4(p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, p.z + (q.z - p.z) * t, nearPlane);
			}
		}
		if (n >= 3) Raster(out[0], out[1], out[2]);
		if (n == 4) Raster(out[0], out[2], out[3]);
	}

	void OcclusionBuffer::Raster(const Vec4 &a, const Vec4 &b, const Vec4 &c)
	{
		// To the picture: x right, y down; depth is the view distance (w), drawn through 1/w,
		// which is what is linear across the screen. Only put in the list here: drawn in Finish.
		const f32 W = (f32)kWidth, H = (f32)kHeight;
		ScreenTriangle t;
		t.x[0] = (a.x / a.w * 0.5f + 0.5f) * W; t.y[0] = (0.5f - a.y / a.w * 0.5f) * H; t.iw[0] = 1.f / a.w;
		t.x[1] = (b.x / b.w * 0.5f + 0.5f) * W; t.y[1] = (0.5f - b.y / b.w * 0.5f) * H; t.iw[1] = 1.f / b.w;
		t.x[2] = (c.x / c.w * 0.5f + 0.5f) * W; t.y[2] = (0.5f - c.y / c.w * 0.5f) * H; t.iw[2] = 1.f / c.w;
		// Facing away (in this picture's y-down winding): what is behind its front faces is nearer
		// to nothing - a closed thing's front faces give its nearest depth. (A wall of one face, seen
		// from behind, hides nothing then: which errs the right way.)
		const f32 area = (t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.y[1] - t.y[0]) * (t.x[2] - t.x[0]);
		if (!(area < -1e-6f)) return;
		const f32 fx0 = std::min(t.x[0], std::min(t.x[1], t.x[2])), fx1 = std::max(t.x[0], std::max(t.x[1], t.x[2]));
		const f32 fy0 = std::min(t.y[0], std::min(t.y[1], t.y[2])), fy1 = std::max(t.y[0], std::max(t.y[1], t.y[2]));
		if (!(fx1 >= 0.f && fy1 >= 0.f && fx0 <= W && fy0 <= H)) return;           // (and NaN)
		t.minX = (int16)std::max(0.f, floorf(fx0)); t.maxX = (int16)std::min(W - 1.f, ceilf(fx1));
		t.minY = (int16)std::max(0.f, floorf(fy0)); t.maxY = (int16)std::min(H - 1.f, ceilf(fy1));
		if (t.minX > t.maxX || t.minY > t.maxY) return;
		screen.push_back(t);
	}

	void OcclusionBuffer::RasterRows(const uint32 rowBegin, const uint32 rowEnd)
	{
		f32* d = depth[0].data();
		for (size_t n = 0; n < screen.size(); n++)
		{
			const ScreenTriangle &t = screen[n];
			const int y0 = std::max((int)rowBegin, (int)t.minY), y1 = std::min((int)rowEnd - 1, (int)t.maxY);
			if (y0 > y1) continue;
			const f32 ax = t.x[0], ay = t.y[0], bx = t.x[1], by = t.y[1], cx = t.x[2], cy = t.y[2];
			const f32 inv = 1.f / ((bx - ax) * (cy - ay) - (by - ay) * (cx - ax));
			// edge functions, stepped along a row: e(x + 1) = e(x) + de/dx
			const f32 d0x = -(cy - by) * inv, d1x = -(ay - cy) * inv;
			for (int y = y0; y <= y1; y++)
			{
				const f32 py = (f32)y + 0.5f, px = (f32)t.minX + 0.5f;
				f32 w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) * inv;
				f32 w1 = ((cx - px) * (ay - py) - (cy - py) * (ax - px)) * inv;
				f32* row = d + (size_t)y * kWidth;
				for (int x = t.minX; x <= t.maxX; x++, w0 += d0x, w1 += d1x)
				{
					const f32 w2 = 1.f - w0 - w1;
					if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
					const f32 z = 1.f / (w0 * t.iw[0] + w1 * t.iw[1] + w2 * t.iw[2]);
					if (z < row[x]) row[x] = z;
				}
			}
		}
	}

	void OcclusionBuffer::Finish()
	{
		// The triangles drawn, in bands of rows on every core (a band its own rows: nothing shared).
		{
			const uint32 bands = 16, rows = kHeight / bands;
			JobSystem::Instance().ParallelFor(bands, 1, [this, rows](uint32 begin, uint32 end) {
				for (uint32 b = begin; b < end; b++) RasterRows(b * rows, (b + 1) * rows);
			});
		}
		uint32 w = kWidth, h = kHeight;
		for (size_t l = 1; l < depth.size(); l++)
		{
			const uint32 nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
			const std::vector<f32> &src = depth[l - 1];
			std::vector<f32> &dst = depth[l];
			for (uint32 y = 0; y < nh; y++)
				for (uint32 x = 0; x < nw; x++)
				{
					f32 m = 0.f;
					for (uint32 j = 0; j < 2; j++)
						for (uint32 i = 0; i < 2; i++)
						{
							const uint32 sx = std::min(w - 1, x * 2 + i), sy = std::min(h - 1, y * 2 + j);
							m = std::max(m, src[(size_t)sy * w + sx]);
						}
					dst[(size_t)y * nw + x] = m;
				}
			w = nw; h = nh;
		}
		ready = occludersDrawn > 0;
	}

	bool OcclusionBuffer::Hidden(const Vec3 &centre, const f32 radius) const
	{
		if (!ready) return false;
		const Vec4 c = viewProjection * Vec4(centre, 1.f);
		const f32 nearest = c.w - radius;
		if (nearest <= nearPlane) return false;                              // (round the camera, or behind it)
		// Its rectangle on the picture: the centre's place, grown by the radius as seen from its
		// nearest point (wider than the sphere's own outline - which errs the right way), and a pixel.
		const f32 sx = (c.x / c.w * 0.5f + 0.5f) * (f32)kWidth, sy = (0.5f - c.y / c.w * 0.5f) * (f32)kHeight;
		const f32 rx = radius * focalX / nearest + 1.f, ry = radius * focalY / nearest + 1.f;
		int x0 = (int)floorf(sx - rx), x1 = (int)ceilf(sx + rx), y0 = (int)floorf(sy - ry), y1 = (int)ceilf(sy + ry);
		// (partly off the picture: the part on it is what is asked; wholly off, the frustum's matter)
		x0 = std::max(x0, 0); y0 = std::max(y0, 0); x1 = std::min(x1, (int)kWidth - 1); y1 = std::min(y1, (int)kHeight - 1);
		if (x0 > x1 || y0 > y1) return false;
		// The level at which the rectangle is a few texels across.
		uint32 level = 0;
		while (level + 1 < depth.size() && ((x1 - x0) >> level) > 4 && ((y1 - y0) >> level) > 4) level++;
		const uint32 lw = std::max(1u, kWidth >> level), lh = std::max(1u, kHeight >> level);
		const std::vector<f32> &d = depth[level];
		const int lx0 = x0 >> level, lx1 = std::min((int)lw - 1, x1 >> level), ly0 = y0 >> level, ly1 = std::min((int)lh - 1, y1 >> level);
		for (int y = ly0; y <= ly1; y++)
			for (int x = lx0; x <= lx1; x++)
				if (!(d[(size_t)y * lw + x] < nearest)) return false;    // (something there is not in front of it)
		return true;
	}
}
