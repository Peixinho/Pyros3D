//============================================================================
// Name        : Occluder2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A shape that blocks 2D light
//============================================================================

#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <cmath>
#include <atomic>

namespace p3d {

	static std::atomic<int> gLiveOccluder2D(0);
	uint32 Occluder2D::LiveCount() { const int n = gLiveOccluder2D.load(); return n > 0 ? (uint32)n : 0; }

	Occluder2D::Occluder2D(const uint32 shape, const Vec2 &size)
	{
		gLiveOccluder2D++;
		shapeType = shape;
		this->size = size;
		enabled = true;
	}

	Occluder2D::~Occluder2D() { gLiveOccluder2D--; }

	namespace {
		// One shape, in world space, appended as line segments.
		void EmitShape(std::vector<Vec4> &out, const uint32 shape, const Vec2 &h,
			const Vec3 &c, const f32 angle)
		{
			if (shape == Occluder2DShape::Circle)
			{
				// An inscribed octagon: a circle has no edges, and the segment
				// budget is the scarce thing in the shader's loop.
				static const int N = 8;
				for (int k = 0; k < N; k++)
				{
					const f32 a0 = (f32)k / N * 6.2831853f;
					const f32 a1 = (f32)(k + 1) / N * 6.2831853f;
					out.push_back(Vec4(c.x + cosf(a0) * h.x, c.y + sinf(a0) * h.x,
						c.x + cosf(a1) * h.x, c.y + sinf(a1) * h.x));
				}
				return;
			}
			const f32 ca = cosf(angle), sa = sinf(angle);
			const f32 xs[4] = { -h.x, h.x, h.x, -h.x };
			const f32 ys[4] = { -h.y, -h.y, h.y, h.y };
			for (int k = 0; k < 4; k++)
			{
				const int n = (k + 1) % 4;
				out.push_back(Vec4(
					c.x + (ca * xs[k] - sa * ys[k]), c.y + (sa * xs[k] + ca * ys[k]),
					c.x + (ca * xs[n] - sa * ys[n]), c.y + (sa * xs[n] + ca * ys[n])));
			}
		}
	}

	namespace {

		// One occluding shape as it was gathered: its segments, and the
		// circle that holds them, which is all the relevance test needs.
		struct Caster {
			std::vector<Vec4> segments;
			Vec2 centre;
			f32 radius;
			f32 score;
		};

		void Bound(Caster &c)
		{
			Vec2 lo(1e30f, 1e30f), hi(-1e30f, -1e30f);
			for (size_t i = 0; i < c.segments.size(); i++)
			{
				const Vec4 &s = c.segments[i];
				lo.x = std::min(lo.x, std::min(s.x, s.z)); hi.x = std::max(hi.x, std::max(s.x, s.z));
				lo.y = std::min(lo.y, std::min(s.y, s.w)); hi.y = std::max(hi.y, std::max(s.y, s.w));
			}
			c.centre = Vec2((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f);
			c.radius = 0.5f * sqrtf((hi.x - lo.x) * (hi.x - lo.x) + (hi.y - lo.y) * (hi.y - lo.y));
			c.score = 0.f;
		}

		// A convex outline as its edges, placed in the world.
		void EmitPolygon(std::vector<Vec4> &out, const std::vector<Vec2> &pts, const Vec2 &offset,
			const Vec3 &c, const f32 angle)
		{
			const f32 ca = cosf(angle), sa = sinf(angle);
			for (size_t k = 0; k < pts.size(); k++)
			{
				const Vec2 a(pts[k].x + offset.x, pts[k].y + offset.y);
				const Vec2 &q = pts[(k + 1) % pts.size()];
				const Vec2 b(q.x + offset.x, q.y + offset.y);
				out.push_back(Vec4(c.x + (ca * a.x - sa * a.y), c.y + (sa * a.x + ca * a.y),
					c.x + (ca * b.x - sa * b.y), c.y + (sa * b.x + ca * b.y)));
			}
		}

		f32 WorldAngle(GameObject* go)
		{
			const Matrix &w = go->GetWorldTransformation();
			return atan2f(w.m[1], w.m[0]);
		}

	}

	void Occluder2D::PublishSceneOccluders(SceneGraph* Scene)
	{
		PublishSceneOccluders(Scene, NULL);
	}

	void Occluder2D::PublishSceneOccluders(SceneGraph* Scene, const Vec4* viewRect)
	{
		std::vector<Vec4> segments;
		// Nothing anywhere that could cast a 2D shadow: the scene is not gone
		// through for one. (It was, every frame - every object and every
		// component of a 3D game - and a twentieth of the frame went on it.)
		if (Scene != NULL && (Occluder2D::LiveCount() > 0 || Physics2D::LiveCount() > 0))
		{
			// Recursive for the same reason Physics2DWorld::Sync is: an occluder
			// inside a layer is a child, and the flat list has no children in it.
			std::vector<GameObject*> all;
			Scene->CollectGameObjectsRecursive(all);

			// The lights a shadow could be cast FROM. A directional light has
			// no place in the plane and casts no 2D shadow, so only the ones
			// with a position and a reach count.
			struct Lamp { Vec2 at; f32 reach; };
			std::vector<Lamp> lamps;
			{
				const std::vector<IComponent*> &lights = ILightComponent::GetLightsOnScene(Scene);
				for (size_t i = 0; i < lights.size(); i++)
				{
					ILightComponent* l = static_cast<ILightComponent*>(lights[i]);
					if (!l || !l->GetOwner() || !l->IsActive()) continue;
					if (l->GetLightType() != LIGHT_TYPE::POINT && l->GetLightType() != LIGHT_TYPE::SPOT) continue;
					const Vec3 p = l->GetOwner()->GetWorldPosition();
					Lamp lamp;
					lamp.at = Vec2(p.x, p.y);
					lamp.reach = l->GetLightType() == LIGHT_TYPE::POINT
						? static_cast<PointLight*>(l)->GetLightRadius() : static_cast<SpotLight*>(l)->GetLightRadius();
					// A light whose whole reach is off screen lights nothing
					// that is drawn, so nothing has to block it.
					if (viewRect != NULL)
					{
						if (lamp.at.x + lamp.reach < viewRect->x || lamp.at.x - lamp.reach > viewRect->z
							|| lamp.at.y + lamp.reach < viewRect->y || lamp.at.y - lamp.reach > viewRect->w) continue;
					}
					lamps.push_back(lamp);
				}
			}

			std::vector<Caster> casters;
			for (size_t i = 0; i < all.size() && !lamps.empty(); i++)
			{
				if (!all[i]) continue;
				const Vec3 c = all[i]->GetWorldPosition();
				const f32 a = WorldAngle(all[i]);
				const std::vector<std::shared_ptr<IComponent> > &comps = all[i]->GetComponents();
				for (size_t k = 0; k < comps.size(); k++)
				{
					if (!comps[k]) continue;
					Caster made;
					if (comps[k]->GetComponentType() == ComponentType::Occluder2D)
					{
						Occluder2D* o = static_cast<Occluder2D*>(comps[k].get());
						if (o->IsEnabled()) EmitShape(made.segments, o->GetShapeType(), o->GetSize(), c, a);
					}
					else if (comps[k]->GetComponentType() == ComponentType::Physics2D)
					{
						Physics2D* p = static_cast<Physics2D*>(comps[k].get());
						// A sensor is not a thing: a trigger volume that cast
						// a shadow would draw the outline of every checkpoint.
						if (!p->CastsShadow() || p->IsSensor() || p->IsCompound()) continue;
						// Where the SHAPE is, which is the object's place
						// plus its offset turned with it.
						const Vec2 off = p->GetOffset();
						const f32 ca = cosf(a), sa = sinf(a);
						const Vec3 sc(c.x + (ca * off.x - sa * off.y), c.y + (sa * off.x + ca * off.y), c.z);
						if (p->GetShapeType() == Shape2DType::Circle)
							EmitShape(made.segments, Occluder2DShape::Circle, p->GetSize(), sc, a);
						else if (p->GetShapeType() == Shape2DType::Polygon && p->GetPolygon().size() >= 3)
							EmitPolygon(made.segments, p->GetPolygon(), off, c, a);
						else if (p->GetShapeType() == Shape2DType::Chain && p->GetPolygon().size() >= 2)
						{
							if (p->IsChainLoop() && p->GetPolygon().size() >= 3)
								EmitPolygon(made.segments, p->GetPolygon(), off, c, a);
							else
							{
								// An open line, there and back: the shader
								// reads two crossings as "something in the
								// way" (one is the lit thing's own outline),
								// and a line is crossed once.
								std::vector<Vec2> thin = p->GetPolygon();
								for (size_t k = p->GetPolygon().size() - 1; k-- > 1;) thin.push_back(p->GetPolygon()[k]);
								EmitPolygon(made.segments, thin, off, c, a);
							}
						}
						else
							// A capsule casts as the box around it: its round
							// ends would be eight more segments each, for a
							// difference at the shadow's corners nobody sees.
							EmitShape(made.segments, Occluder2DShape::Box, p->GetSize(), sc, a);
					}
					if (made.segments.empty()) continue;
					Bound(made);
					// Only what some light on screen can reach casts a
					// shadow anybody sees. This is what makes the budget a
					// budget for the SCREEN: a level may have a hundred
					// pillars as long as few of them stand near a light in
					// view at once. Scored by how far inside the nearest
					// light's reach it is, for when even those are too many.
					f32 best = -1.f;
					for (size_t l = 0; l < lamps.size(); l++)
					{
						const f32 dx = made.centre.x - lamps[l].at.x, dy = made.centre.y - lamps[l].at.y;
						const f32 d = sqrtf(dx * dx + dy * dy) - made.radius;
						if (d > lamps[l].reach) continue;
						const f32 inside = lamps[l].reach > 0.f ? 1.f - (d > 0.f ? d : 0.f) / lamps[l].reach : 0.f;
						if (inside > best) best = inside;
					}
					if (best < 0.f) continue;
					made.score = best;
					casters.push_back(made);
				}
			}

			// Whole shapes or none: half a box's edges would cast a shadow
			// with a slot in it.
			std::stable_sort(casters.begin(), casters.end(),
				[](const Caster &x, const Caster &y) { return x.score > y.score; });
			const size_t budget = IRenderer::MaxOccluders2D();
			size_t dropped = 0;
			for (size_t i = 0; i < casters.size(); i++)
			{
				if (segments.size() + casters[i].segments.size() > budget) { dropped++; continue; }
				segments.insert(segments.end(), casters[i].segments.begin(), casters[i].segments.end());
			}
			if (dropped > 0)
			{
				// Once per count, not per frame: it is about the scene.
				static size_t lastReported = 0;
				if (dropped != lastReported)
				{
					lastReported = dropped;
					echo("WARNING: " + std::to_string(dropped) + " 2D shadow caster(s) near lights on screen did not fit the "
						+ std::to_string(budget) + "-segment budget and cast no shadow - the ones furthest from a light. "
						"A box is 4 segments, a circle 8.");
				}
			}
		}
		IRenderer::SetOccluders2D(segments);
	}

};
