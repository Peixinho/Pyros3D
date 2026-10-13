//============================================================================
// Name        : Extrusion2D
// Description : See Extrusion2D.h.
//============================================================================

#include <Pyros3D/Rendering/Components/Extrusion2D/Extrusion2D.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h>
#include <algorithm>
#include <cmath>

namespace p3d {

	namespace {

		// The two surfaces, as geometry the renderer draws: the skin, then the faces.
		class ExtrusionRenderable : public Renderable {
		public:
			PrimitiveGeometry* skin;
			PrimitiveGeometry* faces;
			bool sent;

			ExtrusionRenderable() : sent(false)
			{
				skin = new PrimitiveGeometry();
				faces = new PrimitiveGeometry();
				Geometries.push_back(skin);
				Geometries.push_back(faces);
				Fill(ExtrusionMesh2D(), ExtrusionMesh2D());
			}

			static void Put(PrimitiveGeometry* g, const ExtrusionMesh2D &m, const bool again)
			{
				if (again) g->Dispose();
				g->tVertex = m.vertex;
				g->tNormal = m.normal;
				g->tTexcoord = m.texcoord;
				g->index = m.index;
				if (g->index.empty())
				{
					// Nothing to draw is still something to hand the device:
					// a triangle with no area.
					g->tVertex.assign(3, Vec3(0.f, 0.f, 0.f));
					g->tNormal.assign(3, Vec3(0.f, 0.f, 1.f));
					g->tTexcoord.assign(3, Vec2(0.f, 0.f));
					g->index.push_back(0); g->index.push_back(1); g->index.push_back(2);
				}
				g->CreateBuffers(false);
				// (and to the device: disposing took the old buffers with it, and nothing else sends new ones)
				g->SendBuffers();
			}

			void Fill(const ExtrusionMesh2D &s, const ExtrusionMesh2D &f)
			{
				Put(skin, s, sent);
				Put(faces, f, sent);
				sent = true;
				minBounds = Vec3(1e30f, 1e30f, 1e30f);
				maxBounds = Vec3(-1e30f, -1e30f, -1e30f);
				const std::vector<Vec3>* sets[2] = { &skin->tVertex, &faces->tVertex };
				for (int k = 0; k < 2; k++)
					for (size_t i = 0; i < sets[k]->size(); i++)
					{
						const Vec3 &v = (*sets[k])[i];
						minBounds = Vec3(std::min(minBounds.x, v.x), std::min(minBounds.y, v.y), std::min(minBounds.z, v.z));
						maxBounds = Vec3(std::max(maxBounds.x, v.x), std::max(maxBounds.y, v.y), std::max(maxBounds.z, v.z));
					}
				BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
				BoundingSphereRadius = minBounds.distance(maxBounds) * 0.5f;
			}

			virtual void CalculateBounding() {}
		};

		std::shared_ptr<IMaterial> LitMaterial(const std::string &texturePath, const Vec4 &color)
		{
			std::shared_ptr<Texture> tex;
			if (!texturePath.empty())
			{
				tex = std::make_shared<Texture>();
				if (!tex->LoadTexture(texturePath, TextureType::Texture, true))
				{
					echo("WARNING: Extrusion2D could not load its image '" + texturePath + "' - drawn plain");
					tex.reset();
				}
				else tex->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
			}
			std::shared_ptr<GenericShaderMaterial> mat = std::make_shared<GenericShaderMaterial>(
				ShaderUsage::Color | ShaderUsage::Diffuse | (tex ? ShaderUsage::Texture : 0)
				| ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow | ShaderUsage::SpotShadow);
			mat->SetColor(color);
			if (tex) mat->SetColorMap(tex);
			return mat;
		}

		f32 Area2(const std::vector<Vec2> &p)
		{
			f32 a = 0.f;
			for (size_t i = 0; i < p.size(); i++)
			{
				const Vec2 &u = p[i], &v = p[(i + 1) % p.size()];
				a += u.x * v.y - v.x * u.y;
			}
			return a;
		}

		void Quad(ExtrusionMesh2D &m, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d,
			const Vec3 &na, const Vec3 &nb, const Vec3 &nc, const Vec3 &nd,
			const Vec2 &ta, const Vec2 &tb, const Vec2 &tc, const Vec2 &td)
		{
			const __INDEX_C_TYPE__ base = (__INDEX_C_TYPE__)m.vertex.size();
			m.vertex.push_back(a); m.vertex.push_back(b); m.vertex.push_back(c); m.vertex.push_back(d);
			m.normal.push_back(na); m.normal.push_back(nb); m.normal.push_back(nc); m.normal.push_back(nd);
			m.texcoord.push_back(ta); m.texcoord.push_back(tb); m.texcoord.push_back(tc); m.texcoord.push_back(td);
			m.index.push_back(base); m.index.push_back(base + 1); m.index.push_back(base + 2);
			m.index.push_back(base + 2); m.index.push_back(base + 3); m.index.push_back(base);
		}
	}

	// ----- the outline ------------------------------------------------------------------

	void Extrusion2D::SmoothPath(const std::vector<Vec2> &pts, const bool closed, const uint32 pieces, std::vector<Vec2> &out)
	{
		out.clear();
		const size_t n = pts.size();
		if (n < 2) { out = pts; return; }
		if (pieces == 0 || n < 3) { out = pts; return; }
		const size_t spans = closed ? n : n - 1;
		for (size_t i = 0; i < spans; i++)
		{
			// Catmull-Rom from pts[i] to pts[i + 1], with the neighbours as
			// its tangents: through every point, with no corner at it. Past
			// an open end the missing neighbour is the end mirrored, which
			// leaves the curve arriving straight.
			const Vec2 &p1 = pts[i], &p2 = pts[(i + 1) % n];
			const Vec2 p0 = closed ? pts[(i + n - 1) % n] : (i > 0 ? pts[i - 1] : Vec2(2.f * p1.x - p2.x, 2.f * p1.y - p2.y));
			const Vec2 p3 = closed ? pts[(i + 2) % n] : (i + 2 < n ? pts[i + 2] : Vec2(2.f * p2.x - p1.x, 2.f * p2.y - p1.y));
			for (uint32 k = 0; k < pieces; k++)
			{
				const f32 t = (f32)k / (f32)pieces, t2 = t * t, t3 = t2 * t;
				out.push_back(Vec2(
					0.5f * ((2.f * p1.x) + (-p0.x + p2.x) * t + (2.f * p0.x - 5.f * p1.x + 4.f * p2.x - p3.x) * t2 + (-p0.x + 3.f * p1.x - 3.f * p2.x + p3.x) * t3),
					0.5f * ((2.f * p1.y) + (-p0.y + p2.y) * t + (2.f * p0.y - 5.f * p1.y + 4.f * p2.y - p3.y) * t2 + (-p0.y + 3.f * p1.y - 3.f * p2.y + p3.y) * t3)));
			}
		}
		if (!closed) out.push_back(pts[n - 1]);
	}

	bool Extrusion2D::Triangulate(const std::vector<Vec2> &polygon, std::vector<uint32> &triangles)
	{
		triangles.clear();
		const size_t n = polygon.size();
		if (n < 3) return false;
		// Ears, cut off one at a time, of the polygon taken counter-clockwise.
		std::vector<uint32> left(n);
		const bool ccw = Area2(polygon) > 0.f;
		for (size_t i = 0; i < n; i++) left[i] = (uint32)(ccw ? i : n - 1 - i);
		size_t guard = n * n + 16;
		while (left.size() > 3 && guard-- > 0)
		{
			bool cut = false;
			for (size_t i = 0; i < left.size(); i++)
			{
				const uint32 ia = left[(i + left.size() - 1) % left.size()], ib = left[i], ic = left[(i + 1) % left.size()];
				const Vec2 &a = polygon[ia], &b = polygon[ib], &c = polygon[ic];
				const f32 cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
				if (cross <= 1e-9f) continue;                         // a dent, or flat: not an ear
				bool empty = true;
				for (size_t k = 0; k < left.size() && empty; k++)
				{
					const uint32 ip = left[k];
					if (ip == ia || ip == ib || ip == ic) continue;
					const Vec2 &p = polygon[ip];
					const f32 d1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
					const f32 d2 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
					const f32 d3 = (a.x - c.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);
					if (d1 >= 0.f && d2 >= 0.f && d3 >= 0.f) empty = false;
				}
				if (!empty) continue;
				triangles.push_back(ia); triangles.push_back(ib); triangles.push_back(ic);
				left.erase(left.begin() + i);
				cut = true;
				break;
			}
			if (!cut) break;                                          // (crossed over itself: what is left is left out)
		}
		if (left.size() == 3) { triangles.push_back(left[0]); triangles.push_back(left[1]); triangles.push_back(left[2]); }
		return !triangles.empty();
	}

	bool Extrusion2D::BuildMesh(const std::vector<Vec2> &pathIn, const bool closed, const f32 depth, const f32 floorY,
		const f32 tileIn, ExtrusionMesh2D &skin, ExtrusionMesh2D &faces, const bool skinAcross)
	{
		skin.Clear();
		faces.Clear();
		std::vector<Vec2> p = pathIn;
		if (p.size() < 2 || (closed && p.size() < 3) || !(depth > 0.f)) return false;
		const f32 tile = tileIn > 1e-4f ? tileIn : 1.f;
		const f32 h = depth * 0.5f;
		// Which way round decides which side is out. An open outline runs
		// left to right, so its left - up - is the outside; a closed one
		// runs clockwise, so that its left is too.
		if (closed) { if (Area2(p) > 0.f) std::reverse(p.begin(), p.end()); }
		else if (p.back().x < p.front().x) std::reverse(p.begin(), p.end());
		const size_t n = p.size();
		const size_t spans = closed ? n : n - 1;

		// The skin: a strip across the depth along every stretch. A corner
		// sharper than about forty degrees stays a corner; a gentler one is
		// shaded as a curve.
		std::vector<Vec2> spanNormal(spans);
		for (size_t i = 0; i < spans; i++)
		{
			const Vec2 &a = p[i], &b = p[(i + 1) % n];
			const f32 dx = b.x - a.x, dy = b.y - a.y;
			const f32 len = sqrtf(dx * dx + dy * dy);
			spanNormal[i] = len > 1e-9f ? Vec2(-dy / len, dx / len) : Vec2(0.f, 1.f);
		}
		f32 along = 0.f;
		for (size_t i = 0; i < spans; i++)
		{
			const Vec2 &a = p[i], &b = p[(i + 1) % n];
			const f32 len = sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
			if (len < 1e-9f) continue;
			Vec2 na = spanNormal[i], nb = spanNormal[i];
			if (closed || i > 0)
			{
				const Vec2 &prev = spanNormal[(i + spans - 1) % spans];
				if (prev.x * na.x + prev.y * na.y > 0.77f) { na = Vec2(prev.x + na.x, prev.y + na.y); const f32 l = sqrtf(na.x * na.x + na.y * na.y); na = Vec2(na.x / l, na.y / l); }
			}
			if (closed || i + 1 < spans)
			{
				const Vec2 &next = spanNormal[(i + 1) % spans];
				if (next.x * nb.x + next.y * nb.y > 0.77f) { nb = Vec2(next.x + nb.x, next.y + nb.y); const f32 l = sqrtf(nb.x * nb.x + nb.y * nb.y); nb = Vec2(nb.x / l, nb.y / l); }
			}
			const f32 u0 = along / tile, u1 = (along + len) / tile, v0 = 0.f, v1 = skinAcross ? 1.f : depth / tile;
			Quad(skin, Vec3(a.x, a.y, h), Vec3(b.x, b.y, h), Vec3(b.x, b.y, -h), Vec3(a.x, a.y, -h),
				Vec3(na.x, na.y, 0.f), Vec3(nb.x, nb.y, 0.f), Vec3(nb.x, nb.y, 0.f), Vec3(na.x, na.y, 0.f),
				Vec2(u0, v0), Vec2(u1, v0), Vec2(u1, v1), Vec2(u0, v1));
			along += len;
		}

		// The faces: the shape itself, towards the camera and away from it.
		const Vec3 front(0.f, 0.f, 1.f), back(0.f, 0.f, -1.f);
		if (closed)
		{
			std::vector<uint32> tri;
			if (Triangulate(p, tri))
			{
				for (int side = 0; side < 2; side++)
				{
					const __INDEX_C_TYPE__ base = (__INDEX_C_TYPE__)faces.vertex.size();
					for (size_t i = 0; i < n; i++)
					{
						faces.vertex.push_back(Vec3(p[i].x, p[i].y, side == 0 ? h : -h));
						faces.normal.push_back(side == 0 ? front : back);
						faces.texcoord.push_back(Vec2(p[i].x / tile, p[i].y / tile));
					}
					// (Triangulate hands back counter-clockwise triangles: as they are towards the camera, turned over away from it)
					for (size_t t = 0; t + 2 < tri.size(); t += 3)
					{
						faces.index.push_back(base + tri[t]);
						faces.index.push_back(base + tri[t + (side == 0 ? 1 : 2)]);
						faces.index.push_back(base + tri[t + (side == 0 ? 2 : 1)]);
					}
				}
			}
		}
		else
		{
			f32 lowest = floorY;
			for (size_t i = 0; i < n; i++) if (p[i].y < lowest) lowest = p[i].y;
			const f32 fy = lowest;       // (a floor above the outline's lowest point would turn the face inside out there)
			for (size_t i = 0; i + 1 < n; i++)
			{
				const Vec2 &a = p[i], &b = p[i + 1];
				if (fabsf(b.x - a.x) < 1e-9f) continue;
				const Vec2 ta(a.x / tile, a.y / tile), tb(b.x / tile, b.y / tile), fa(a.x / tile, fy / tile), fb(b.x / tile, fy / tile);
				Quad(faces, Vec3(a.x, fy, h), Vec3(b.x, fy, h), Vec3(b.x, b.y, h), Vec3(a.x, a.y, h), front, front, front, front, fa, fb, tb, ta);
				Quad(faces, Vec3(b.x, fy, -h), Vec3(a.x, fy, -h), Vec3(a.x, a.y, -h), Vec3(b.x, b.y, -h), back, back, back, back, fb, fa, ta, tb);
			}
			// and its two ends
			const Vec2 &s = p[0], &e = p[n - 1];
			const Vec3 west(-1.f, 0.f, 0.f), east(1.f, 0.f, 0.f);
			if (s.y - fy > 1e-6f)
				Quad(faces, Vec3(s.x, fy, -h), Vec3(s.x, fy, h), Vec3(s.x, s.y, h), Vec3(s.x, s.y, -h), west, west, west, west,
					Vec2(0.f, fy / tile), Vec2(depth / tile, fy / tile), Vec2(depth / tile, s.y / tile), Vec2(0.f, s.y / tile));
			if (e.y - fy > 1e-6f)
				Quad(faces, Vec3(e.x, fy, h), Vec3(e.x, fy, -h), Vec3(e.x, e.y, -h), Vec3(e.x, e.y, h), east, east, east, east,
					Vec2(0.f, fy / tile), Vec2(depth / tile, fy / tile), Vec2(depth / tile, e.y / tile), Vec2(0.f, e.y / tile));
		}
		return !skin.index.empty();
	}

	// ----- the component ----------------------------------------------------------------------

	Extrusion2D::Extrusion2D()
		: RenderingComponent(std::make_shared<ExtrusionRenderable>(), LitMaterial(std::string(), Vec4(0.55f, 0.42f, 0.30f, 1.f)))
	{
		closed = false;
		collider = true;
		smooth = 6;
		depth = 4.f;
		floorY = -3.f;
		tile = 3.f;
		skinColor = Vec4(0.55f, 0.42f, 0.30f, 1.f);
		faceColor = Vec4(0.36f, 0.27f, 0.19f, 1.f);
		dirty = pathDirty = materialsDirty = true;
		// (its bounds are whatever was last drawn: a level's ground is in view whenever the level is)
		DisableCullTest();
	}

	Extrusion2D::~Extrusion2D() {}

	void Extrusion2D::SetPoints(const std::vector<Vec2> &pts) { points = pts; dirty = pathDirty = true; }
	void Extrusion2D::AddPoint(const Vec2 &p, const int32 at)
	{
		if (at < 0 || (size_t)at >= points.size()) points.push_back(p);
		else points.insert(points.begin() + at, p);
		dirty = pathDirty = true;
	}
	void Extrusion2D::SetPoint(const uint32 index, const Vec2 &p)
	{
		if (index < points.size()) { points[index] = p; dirty = pathDirty = true; }
	}
	void Extrusion2D::RemovePoint(const uint32 index)
	{
		if (index < points.size() && points.size() > 2) { points.erase(points.begin() + index); dirty = pathDirty = true; }
	}
	void Extrusion2D::SetClosed(const bool c) { if (c != closed) { closed = c; dirty = pathDirty = true; } }
	void Extrusion2D::SetSmooth(const uint32 pieces)
	{
		const uint32 v = pieces > 16 ? 16 : pieces;
		if (v != smooth) { smooth = v; dirty = pathDirty = true; }
	}
	void Extrusion2D::SetDepth(const f32 d) { const f32 v = d > 0.01f ? d : 0.01f; if (v != depth) { depth = v; dirty = true; } }
	void Extrusion2D::SetFloor(const f32 y) { if (y != floorY) { floorY = y; dirty = true; } }
	void Extrusion2D::SetTile(const f32 unitsPerRepeat) { const f32 v = unitsPerRepeat > 0.01f ? unitsPerRepeat : 0.01f; if (v != tile) { tile = v; dirty = true; } }
	void Extrusion2D::SetSkinTexture(const std::string &resolvedPath, const std::string &authoredPath)
	{
		if (authoredPath != skinPath || resolvedPath != skinResolved) { skinPath = authoredPath; skinResolved = resolvedPath; dirty = materialsDirty = true; }
	}
	void Extrusion2D::SetFaceTexture(const std::string &resolvedPath, const std::string &authoredPath)
	{
		if (authoredPath != facePath || resolvedPath != faceResolved) { facePath = authoredPath; faceResolved = resolvedPath; dirty = materialsDirty = true; }
	}
	void Extrusion2D::SetColors(const Vec4 &skin, const Vec4 &face) { skinColor = skin; faceColor = face; dirty = materialsDirty = true; }
	void Extrusion2D::SetSkinAcross(const bool once) { if (once != skinAcross) { skinAcross = once; dirty = true; } }
	void Extrusion2D::SetCollider(const bool on) { if (on != collider) { collider = on; dirty = true; } }

	const std::vector<Vec2> &Extrusion2D::GetPath()
	{
		if (pathDirty)
		{
			SmoothPath(points, closed, smooth, path);
			pathDirty = false;
		}
		return path;
	}

	f32 Extrusion2D::HeightAt(const f32 x)
	{
		const std::vector<Vec2> &p = GetPath();
		if (p.empty()) return 0.f;
		const bool forward = p.back().x >= p.front().x;
		const Vec2 &first = forward ? p.front() : p.back(), &last = forward ? p.back() : p.front();
		if (x <= first.x) return first.y;
		if (x >= last.x) return last.y;
		f32 best = first.y;
		bool found = false;
		for (size_t i = 0; i + 1 < p.size(); i++)
		{
			const Vec2 &a = p[i], &b = p[i + 1];
			const f32 lo = std::min(a.x, b.x), hi = std::max(a.x, b.x);
			if (x < lo || x > hi || hi - lo < 1e-9f) continue;
			const f32 y = a.y + (b.y - a.y) * ((x - a.x) / (b.x - a.x));
			if (!found || y > best) { best = y; found = true; }      // (where it folds back over itself: the top)
		}
		return best;
	}

	void Extrusion2D::MakeMaterials()
	{
		std::vector<RenderingMesh*> &meshes = GetMeshes(0);
		if (meshes.size() > 0 && meshes[0]) meshes[0]->Material = LitMaterial(skinResolved, skinColor);
		if (meshes.size() > 1 && meshes[1]) meshes[1]->Material = LitMaterial(faceResolved, faceColor);
		materialsDirty = false;
	}

	void Extrusion2D::Apply()
	{
		if (!dirty) return;
		dirty = false;
		const std::vector<Vec2> &p = GetPath();
		ExtrusionMesh2D skin, faces;
		BuildMesh(p, closed, depth, floorY, tile, skin, faces, skinAcross);
		static_cast<ExtrusionRenderable*>(GetRenderable())->Fill(skin, faces);
		if (materialsDirty) MakeMaterials();
		// What is touched is what is seen: the body on the same object, as a chain along the outline.
		if (collider && Owner && p.size() >= 2)
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
			for (size_t c = 0; c < comps.size(); c++)
				if (comps[c] && comps[c]->GetComponentType() == ComponentType::Physics2D)
				{
					Physics2D* body = static_cast<Physics2D*>(comps[c].get());
					body->SetShapeType(Shape2DType::Chain);
					if (!body->SetChain(p, closed && p.size() >= 3))
						echo("WARNING: Extrusion2D - its outline could not be made a collider (too many points: smooth it less, or use fewer)");
					break;
				}
		}
		RefreshMeshCollider();
	}

	// (called at the end of Apply: a 3D mesh collider built from this shape is built again from it)
	void Extrusion2D::RefreshMeshCollider()
	{
		if (!Owner) return;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t c = 0; c < comps.size(); c++)
		{
			PhysicsTriangleMesh* mesh = dynamic_cast<PhysicsTriangleMesh*>(comps[c].get());
			if (mesh && mesh->IsFromRenderable()) mesh->Rebuild(this);
		}
	}

	void Extrusion2D::Update(const f64)
	{
		Apply();
	}

};
