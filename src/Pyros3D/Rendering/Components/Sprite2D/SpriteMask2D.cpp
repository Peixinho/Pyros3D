//============================================================================
// Name        : SpriteMask2D.cpp
// Description : See the header.
//============================================================================

#include <Pyros3D/Rendering/Components/Sprite2D/SpriteMask2D.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace p3d {

	namespace {
		std::atomic<int> gLiveMask2D(0);
		std::vector<SpriteMask2D*> &Masks()
		{
			static std::vector<SpriteMask2D*> all;
			return all;
		}
		std::mutex &MasksLock()
		{
			static std::mutex m;
			return m;
		}
	}

	uint32 SpriteMask2D::LiveCount() { const int n = gLiveMask2D.load(); return n > 0 ? (uint32)n : 0; }

	SpriteMask2D::SpriteMask2D()
		: shape(SpriteMaskShape2D::Rect), size(1.f, 1.f), offset(0.f, 0.f), enabled(true), revision(1)
	{
		gLiveMask2D++;
		std::lock_guard<std::mutex> lock(MasksLock());
		Masks().push_back(this);
	}

	SpriteMask2D::~SpriteMask2D()
	{
		gLiveMask2D--;
		std::lock_guard<std::mutex> lock(MasksLock());
		std::vector<SpriteMask2D*> &all = Masks();
		all.erase(std::remove(all.begin(), all.end(), this), all.end());
	}

	void SpriteMask2D::SetShape(const int32 s)
	{
		const int32 v = (s == SpriteMaskShape2D::Circle || s == SpriteMaskShape2D::Polygon) ? s : SpriteMaskShape2D::Rect;
		if (v != shape) { shape = v; revision++; }
	}
	void SpriteMask2D::SetSize(const Vec2 &s)
	{
		const Vec2 v(s.x > 0.f ? s.x : 0.f, s.y > 0.f ? s.y : 0.f);
		if (v.x != size.x || v.y != size.y) { size = v; revision++; }
	}
	void SpriteMask2D::SetOffset(const Vec2 &o)
	{
		if (o.x != offset.x || o.y != offset.y) { offset = o; revision++; }
	}
	void SpriteMask2D::SetEnabled(const bool e)
	{
		if (e != enabled) { enabled = e; revision++; }
	}

	bool SpriteMask2D::SetPolygon(const std::vector<Vec2> &points)
	{
		if (points.size() < 3 || points.size() > 16) return false;
		// Convex: every corner turns the same way, and it has area.
		int sign = 0;
		f32 area = 0.f;
		const size_t n = points.size();
		for (size_t i = 0; i < n; i++)
		{
			const Vec2 &a = points[i], &b = points[(i + 1) % n], &c = points[(i + 2) % n];
			area += a.x * b.y - b.x * a.y;
			const f32 cr = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
			if (fabsf(cr) < 1e-7f) continue;
			const int sg = cr > 0.f ? 1 : -1;
			if (sign == 0) sign = sg;
			else if (sg != sign) return false;
		}
		if (sign == 0 || fabsf(area) < 1e-7f) return false;
		polygon = points;
		revision++;
		return true;
	}

	void SpriteMask2D::WorldOutline(std::vector<Vec2> &out) const
	{
		out.clear();
		if (!Owner) return;
		std::vector<Vec2> local;
		if (shape == SpriteMaskShape2D::Circle)
		{
			const f32 r = size.x * 0.5f;
			if (!(r > 0.f)) return;
			// Thirty-two sides: round at any size a sprite is drawn at.
			for (int i = 0; i < 32; i++)
			{
				const f32 a = (f32)i / 32.f * 6.2831853f;
				local.push_back(Vec2(offset.x + cosf(a) * r, offset.y + sinf(a) * r));
			}
		}
		else if (shape == SpriteMaskShape2D::Polygon)
		{
			if (polygon.size() < 3) return;
			for (size_t i = 0; i < polygon.size(); i++) local.push_back(Vec2(offset.x + polygon[i].x, offset.y + polygon[i].y));
		}
		else
		{
			const f32 hx = size.x * 0.5f, hy = size.y * 0.5f;
			if (!(hx > 0.f) || !(hy > 0.f)) return;
			local.push_back(Vec2(offset.x - hx, offset.y - hy)); local.push_back(Vec2(offset.x + hx, offset.y - hy));
			local.push_back(Vec2(offset.x + hx, offset.y + hy)); local.push_back(Vec2(offset.x - hx, offset.y + hy));
		}
		const Matrix &w = Owner->GetWorldTransformation();
		f32 area = 0.f;
		for (size_t i = 0; i < local.size(); i++)
			out.push_back(Vec2(w.m[0] * local[i].x + w.m[4] * local[i].y + w.m[12], w.m[1] * local[i].x + w.m[5] * local[i].y + w.m[13]));
		for (size_t i = 0; i < out.size(); i++)
		{ const Vec2 &a = out[i], &b = out[(i + 1) % out.size()]; area += a.x * b.y - b.x * a.y; }
		// A mirrored owner turns the outline over.
		if (area < 0.f) std::reverse(out.begin(), out.end());
		if (fabsf(area) < 1e-9f) out.clear();
	}

	SpriteMask2D* SpriteMask2D::Find(GameObject* go, const std::string &name)
	{
		if (!go || gLiveMask2D.load() <= 0) return NULL;
		if (name.empty())
		{
			for (GameObject* at = go; at != NULL; at = at->GetParent())
			{
				const std::vector<std::shared_ptr<IComponent> > &comps = at->GetComponents();
				for (size_t i = 0; i < comps.size(); i++)
					if (comps[i] && comps[i]->GetComponentType() == ComponentType::SpriteMask2D
						&& static_cast<SpriteMask2D*>(comps[i].get())->IsEnabled())
						return static_cast<SpriteMask2D*>(comps[i].get());
			}
			return NULL;
		}
		SceneGraph* scene = go->GetOwningScene();
		std::lock_guard<std::mutex> lock(MasksLock());
		const std::vector<SpriteMask2D*> &all = Masks();
		for (size_t i = 0; i < all.size(); i++)
		{
			GameObject* o = all[i]->GetOwner();
			if (o && all[i]->IsEnabled() && o->GetOwningScene() == scene && o->GetName() == name) return all[i];
		}
		return NULL;
	}

};
