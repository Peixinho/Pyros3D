//============================================================================
// Name        : Physics2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box2D-backed rigid body for 2D scenes
//============================================================================

#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <box2d/box2d.h>
#include <atomic>
#include <cmath>

namespace p3d {

	static std::atomic<int> gLivePhysics2D(0);
	uint32 Physics2D::LiveCount() { const int n = gLivePhysics2D.load(); return n > 0 ? (uint32)n : 0; }

	Physics2D::Physics2D(const uint32 bodyType, const uint32 shape, const Vec2 &size)
	{
		gLivePhysics2D++;
		this->bodyType = bodyType;
		this->shapeType = shape;
		this->size = size;
		density = 1.f;
		friction = 0.3f;
		restitution = 0.f;
		fixedRotation = false;
		castsShadow = true;
		offset = Vec2(0.f, 0.f);
		sensor = false;
		oneWay = false;
		layer = 0;
		mask = 0xFFFFFFFFu;
		gravityScale = 1.f;
		linearDamping = 0.f;
		angularDamping = 0.f;
		bullet = false;
		shapesDirty = false;
		haveBody = false;
		bodyIndex = 0;
		bodyWorld = 0;
		bodyGeneration = 0;
	}

	Physics2D::~Physics2D() { gLivePhysics2D--; }

	void Physics2D::SetCompoundBoxes(const std::vector<Vec4> &boxes)
	{
		SetCompoundShapes(boxes, compoundPolys);
	}

	namespace {
		bool SamePolys(const std::vector<Physics2D::Poly2D> &a,
			const std::vector<Physics2D::Poly2D> &b)
		{
			if (a.size() != b.size()) return false;
			for (size_t i = 0; i < a.size(); i++)
			{
				if (a[i].size() != b[i].size()) return false;
				for (size_t k = 0; k < a[i].size(); k++)
					if (a[i][k].x != b[i][k].x || a[i][k].y != b[i][k].y) return false;
			}
			return true;
		}
	}

	void Physics2D::SetCompoundShapes(const std::vector<Vec4> &boxes,
		const std::vector<Poly2D> &polys)
	{
		SetCompoundShapes(boxes, polys, compoundChains);
	}

	void Physics2D::SetCompoundShapes(const std::vector<Vec4> &boxes,
		const std::vector<Poly2D> &polys, const std::vector<Poly2D> &chains)
	{
		// Compared rather than assigned blindly: TileMap2D recomputes its
		// merge whenever the grid changes, and most edits do not change the
		// collider set at all. Rebuilding an unchanged body would destroy and
		// recreate every contact on it - anything standing on the floor would
		// be re-seated, and a body mid-contact would lose the contact events
		// it was about to receive.
		if (boxes.size() == compoundBoxes.size())
		{
			bool same = true;
			for (size_t i = 0; i < boxes.size() && same; i++)
				same = (boxes[i].x == compoundBoxes[i].x && boxes[i].y == compoundBoxes[i].y
					&& boxes[i].z == compoundBoxes[i].z && boxes[i].w == compoundBoxes[i].w);
			// The polygons have to agree too, or editing only a slope would
			// compare equal on the boxes and never rebuild the body.
			if (same && SamePolys(polys, compoundPolys)
				&& SamePolys(chains, compoundChains)) return;
		}
		compoundBoxes = boxes;
		compoundPolys = polys;
		compoundChains = chains;
		shapesDirty = true;
	}

	namespace {
		b2BodyId IdOf(const Physics2D* p)
		{
			b2BodyId id;
			id.index1 = p->GetBodyIndex();
			id.world0 = p->GetBodyWorld();
			id.generation = p->GetBodyGeneration();
			return id;
		}
	}

	// --- settings ---------------------------------------------------------------

	namespace {
		// Runs over every shape of a body. A body has one, or - a tilemap's -
		// a few dozen; never enough to matter.
		template <typename F>
		void ForEachShape(const b2BodyId body, F f)
		{
			const int n = b2Body_GetShapeCount(body);
			if (n <= 0) return;
			std::vector<b2ShapeId> shapes((size_t)n);
			const int got = b2Body_GetShapes(body, &shapes[0], n);
			for (int i = 0; i < got; i++) f(shapes[(size_t)i]);
		}
	}

	void Physics2D::SetBodyType(const uint32 t)
	{
		if (t == bodyType) return;
		bodyType = t;
		if (HaveBody())
			b2Body_SetType(IdOf(this), t == Body2DType::Static ? b2_staticBody
				: (t == Body2DType::Kinematic ? b2_kinematicBody : b2_dynamicBody));
	}

	void Physics2D::SetShapeType(const uint32 t)
	{
		if (t == shapeType) return;
		shapeType = t;
		shapesDirty = true;
	}

	void Physics2D::SetSize(const Vec2 &s)
	{
		if (s.x == size.x && s.y == size.y) return;
		size = s;
		shapesDirty = true;
	}

	void Physics2D::SetOffset(const Vec2 &o)
	{
		if (o.x == offset.x && o.y == offset.y) return;
		offset = o;
		shapesDirty = true;
	}

	namespace {
		f32 Cross2(const Vec2 &o, const Vec2 &a, const Vec2 &b)
		{
			return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
		}
		f32 SignedArea2(const std::vector<Vec2> &p)
		{
			f32 a = 0.f;
			for (size_t i = 0; i < p.size(); i++)
			{
				const Vec2 &u = p[i], &v = p[(i + 1) % p.size()];
				a += u.x * v.y - v.x * u.y;
			}
			return a;
		}
		// Proper crossing of two segments (touching at an end does not count).
		bool SegmentsCross(const Vec2 &a, const Vec2 &b, const Vec2 &c, const Vec2 &d)
		{
			const f32 d1 = Cross2(a, b, c), d2 = Cross2(a, b, d), d3 = Cross2(c, d, a), d4 = Cross2(c, d, b);
			return ((d1 > 0.f && d2 < 0.f) || (d1 < 0.f && d2 > 0.f)) && ((d3 > 0.f && d4 < 0.f) || (d3 < 0.f && d4 > 0.f));
		}
		bool PointInTriangle(const Vec2 &p, const Vec2 &a, const Vec2 &b, const Vec2 &c)
		{
			const f32 d1 = Cross2(a, b, p), d2 = Cross2(b, c, p), d3 = Cross2(c, a, p);
			const bool neg = (d1 < 0.f) || (d2 < 0.f) || (d3 < 0.f);
			const bool pos = (d1 > 0.f) || (d2 > 0.f) || (d3 > 0.f);
			return !(neg && pos);
		}
		bool IsConvexCCW(const std::vector<Vec2> &p)
		{
			for (size_t i = 0; i < p.size(); i++)
				if (Cross2(p[i], p[(i + 1) % p.size()], p[(i + 2) % p.size()]) < -1e-6f) return false;
			return true;
		}
	}

	bool Physics2D::SetPolygon(const std::vector<Vec2> &points)
	{
		if (points.size() < 3 || points.size() > (size_t)MaxOutlinePoints) return false;
		// An outline with area, whose edges do not cross: what "a shape"
		// means. Which way round it is drawn does not matter.
		if (fabsf(SignedArea2(points)) < 1e-6f) return false;
		const size_t n = points.size();
		for (size_t i = 0; i < n; i++)
		{
			const Vec2 &a = points[i], &b = points[(i + 1) % n];
			if (fabsf(a.x - b.x) < 1e-6f && fabsf(a.y - b.y) < 1e-6f) return false;
			for (size_t k = i + 2; k < n; k++)
			{
				if (i == 0 && k == n - 1) continue;       // neighbours, round the end
				if (SegmentsCross(a, b, points[k], points[(k + 1) % n])) return false;
			}
		}
		// The solver has to be able to make something of it too.
		std::vector<std::vector<Vec2> > pieces;
		DecomposePolygon(points, pieces);
		if (pieces.empty()) return false;
		polygon = points;
		shapesDirty = true;
		return true;
	}

	bool Physics2D::SetChain(const std::vector<Vec2> &points, const bool loop)
	{
		if (points.size() < 2 || points.size() > (size_t)MaxOutlinePoints) return false;
		if (loop && points.size() < 3) return false;
		for (size_t i = 0; i + 1 < points.size(); i++)
			if (fabsf(points[i].x - points[i + 1].x) < 1e-6f && fabsf(points[i].y - points[i + 1].y) < 1e-6f) return false;
		polygon = points;
		chainLoop = loop;
		shapesDirty = true;
		return true;
	}

	void Physics2D::DecomposePolygon(const std::vector<Vec2> &outline, std::vector<std::vector<Vec2> > &out)
	{
		out.clear();
		if (outline.size() < 3) return;
		std::vector<Vec2> p = outline;
		if (SignedArea2(p) < 0.f) std::reverse(p.begin(), p.end());
		if (fabsf(SignedArea2(p)) < 1e-6f) return;

		if (p.size() <= 8 && IsConvexCCW(p)) { out.push_back(p); return; }

		// Ears: a corner that turns left with no other point inside it is
		// cut off as a triangle, until three points are left.
		std::vector<int> idx(p.size());
		for (size_t i = 0; i < p.size(); i++) idx[i] = (int)i;
		std::vector<std::vector<int> > tris;
		int guard = (int)p.size() * (int)p.size() + 16;
		while (idx.size() > 3 && guard-- > 0)
		{
			bool cut = false;
			for (size_t i = 0; i < idx.size(); i++)
			{
				const int ia = idx[(i + idx.size() - 1) % idx.size()], ib = idx[i], ic = idx[(i + 1) % idx.size()];
				if (Cross2(p[ia], p[ib], p[ic]) <= 1e-7f) continue;       // turns right, or straight
				bool empty = true;
				for (size_t k = 0; k < idx.size() && empty; k++)
				{
					const int q = idx[k];
					if (q == ia || q == ib || q == ic) continue;
					if (PointInTriangle(p[q], p[ia], p[ib], p[ic])) empty = false;
				}
				if (!empty) continue;
				std::vector<int> t; t.push_back(ia); t.push_back(ib); t.push_back(ic);
				tris.push_back(t);
				idx.erase(idx.begin() + (std::ptrdiff_t)i);
				cut = true;
				break;
			}
			if (!cut)
			{
				// Nothing left that is an ear by the tolerances (a run of
				// points in a line): drop the straightest corner and go on.
				size_t flat = 0;
				f32 least = 1e30f;
				for (size_t i = 0; i < idx.size(); i++)
				{
					const f32 c = fabsf(Cross2(p[idx[(i + idx.size() - 1) % idx.size()]], p[idx[i]], p[idx[(i + 1) % idx.size()]]));
					if (c < least) { least = c; flat = i; }
				}
				idx.erase(idx.begin() + (std::ptrdiff_t)flat);
			}
		}
		if (idx.size() == 3 && Cross2(p[idx[0]], p[idx[1]], p[idx[2]]) > 1e-7f) tris.push_back(idx);

		// Joined back together across a shared edge wherever what results is
		// still convex and still small enough: an L is two pieces, not four.
		std::vector<std::vector<int> > polys = tris;
		bool merged = true;
		while (merged)
		{
			merged = false;
			for (size_t a = 0; a < polys.size() && !merged; a++)
				for (size_t b = a + 1; b < polys.size() && !merged; b++)
				{
					const std::vector<int> &A = polys[a], &B = polys[b];
					if (A.size() + B.size() - 2 > 8) continue;
					for (size_t i = 0; i < A.size() && !merged; i++)
					{
						const int a0 = A[i], a1 = A[(i + 1) % A.size()];
						for (size_t k = 0; k < B.size() && !merged; k++)
						{
							if (B[k] != a1 || B[(k + 1) % B.size()] != a0) continue;
							// A from a1 round to a0, then B from a0 round to a1, the shared edge gone.
							std::vector<int> joined;
							for (size_t s2 = 0; s2 < A.size(); s2++) joined.push_back(A[(i + 1 + s2) % A.size()]);
							for (size_t s2 = 2; s2 < B.size(); s2++) joined.push_back(B[(k + s2) % B.size()]);
							std::vector<Vec2> pts(joined.size());
							for (size_t q = 0; q < joined.size(); q++) pts[q] = p[joined[q]];
							if (!IsConvexCCW(pts)) continue;
							polys[a] = joined;
							polys.erase(polys.begin() + (std::ptrdiff_t)b);
							merged = true;
						}
					}
				}
		}
		for (size_t i = 0; i < polys.size(); i++)
		{
			std::vector<Vec2> pts(polys[i].size());
			for (size_t q = 0; q < polys[i].size(); q++) pts[q] = p[polys[i][q]];
			out.push_back(pts);
		}
	}

	void Physics2D::ApplyMaterial()
	{
		if (!HaveBody()) return;
		const f32 d = density, f = friction, r = restitution;
		ForEachShape(IdOf(this), [d, f, r](const b2ShapeId s) {
			b2Shape_SetDensity(s, d, false);
			b2Shape_SetFriction(s, f);
			b2Shape_SetRestitution(s, r);
		});
		b2Body_ApplyMassFromShapes(IdOf(this));
	}

	void Physics2D::SetDensity(const f32 d) { if (d != density) { density = d; ApplyMaterial(); } }
	void Physics2D::SetFriction(const f32 f) { if (f != friction) { friction = f; ApplyMaterial(); } }
	void Physics2D::SetRestitution(const f32 r) { if (r != restitution) { restitution = r; ApplyMaterial(); } }

	void Physics2D::SetFixedRotation(const bool f)
	{
		if (f == fixedRotation) return;
		fixedRotation = f;
		if (HaveBody())
		{
			b2MotionLocks locks = b2Body_GetMotionLocks(IdOf(this));
			locks.angularZ = f;
			b2Body_SetMotionLocks(IdOf(this), locks);
		}
	}

	void Physics2D::SetSensor(const bool s) { if (s != sensor) { sensor = s; shapesDirty = true; } }
	void Physics2D::SetOneWay(const bool o) { if (o != oneWay) { oneWay = o; shapesDirty = true; } }

	void Physics2D::ApplyFilter()
	{
		if (!HaveBody()) return;
		b2Filter filter = b2DefaultFilter();
		filter.categoryBits = (uint64_t)1 << (layer & 31u);
		filter.maskBits = (uint64_t)mask;
		ForEachShape(IdOf(this), [filter](const b2ShapeId s) { b2Shape_SetFilter(s, filter); });
		// Whatever it was resting on may no longer hold it.
		b2Body_SetAwake(IdOf(this), true);
	}

	void Physics2D::SetLayer(const uint32 l)
	{
		const uint32 v = l > 31 ? 31 : l;
		if (v == layer) return;
		layer = v;
		ApplyFilter();
	}

	void Physics2D::SetMask(const uint32 m)
	{
		if (m == mask) return;
		mask = m;
		ApplyFilter();
	}

	void Physics2D::SetCollidesWith(const uint32 otherLayer, const bool collides)
	{
		if (otherLayer > 31) return;
		SetMask(collides ? (mask | (1u << otherLayer)) : (mask & ~(1u << otherLayer)));
	}

	void Physics2D::SetGravityScale(const f32 g)
	{
		if (g == gravityScale) return;
		gravityScale = g;
		// Woken as well: a body floating at rest is asleep, and a sleeping
		// body is not asked what gravity it feels.
		if (HaveBody()) { b2Body_SetGravityScale(IdOf(this), g); b2Body_SetAwake(IdOf(this), true); }
	}

	void Physics2D::SetLinearDamping(const f32 d)
	{
		if (d == linearDamping) return;
		linearDamping = d;
		if (HaveBody()) b2Body_SetLinearDamping(IdOf(this), d);
	}

	void Physics2D::SetAngularDamping(const f32 d)
	{
		if (d == angularDamping) return;
		angularDamping = d;
		if (HaveBody()) b2Body_SetAngularDamping(IdOf(this), d);
	}

	void Physics2D::SetBullet(const bool b)
	{
		if (b == bullet) return;
		bullet = b;
		if (HaveBody()) b2Body_SetBullet(IdOf(this), b);
	}

	// --- runtime ------------------------------------------------------------------

	void Physics2D::ApplyForceAt(const Vec2 &force, const Vec2 &worldPoint)
	{
		if (!HaveBody()) return;
		// (kept for the world to hand to its steps - see pendingForce; off the
		// centre of mass, it also turns the body)
		const auto c = b2Body_GetWorldCenter(IdOf(this));
		const f32 rx = worldPoint.x - (f32)c.x, ry = worldPoint.y - (f32)c.y;
		pendingForce.x += force.x; pendingForce.y += force.y;
		pendingTorque += rx * force.y - ry * force.x;
	}

	void Physics2D::ApplyImpulseAt(const Vec2 &impulse, const Vec2 &worldPoint)
	{
		if (!HaveBody()) return;
		b2Vec2 i; i.x = impulse.x; i.y = impulse.y;
		b2Pos p; p.x = worldPoint.x; p.y = worldPoint.y;
		b2Body_ApplyLinearImpulse(IdOf(this), i, p, true);
	}

	void Physics2D::ApplyTorque(const f32 torque)
	{
		if (HaveBody()) pendingTorque += torque;
	}

	void Physics2D::ApplyAngularImpulse(const f32 impulse)
	{
		if (HaveBody()) b2Body_ApplyAngularImpulse(IdOf(this), impulse, true);
	}

	Vec2 Physics2D::GetPosition() const
	{
		if (HaveBody())
		{
			const b2Pos p = b2Body_GetPosition(IdOf(this));
			return Vec2((f32)p.x, (f32)p.y);
		}
		if (Owner)
		{
			const Vec3 w = Owner->GetWorldPosition();
			return Vec2(w.x, w.y);
		}
		return Vec2(0.f, 0.f);
	}

	f32 Physics2D::GetAngle() const
	{
		if (HaveBody()) return b2Rot_GetAngle(b2Body_GetRotation(IdOf(this)));
		return Owner ? Owner->GetRotation().z : 0.f;
	}

	void Physics2D::MoveTo(const Vec2 &position, const f32 angle, const f32 dt)
	{
		if (!HaveBody() || !(dt > 0.f)) return;
		b2WorldTransform target;
		target.p.x = position.x;
		target.p.y = position.y;
		target.q = b2MakeRot(angle);
		b2Body_SetTargetTransform(IdOf(this), target, dt, true);
	}

	f32 Physics2D::GetMass() const
	{
		return HaveBody() ? b2Body_GetMass(IdOf(this)) : 0.f;
	}

	bool Physics2D::IsAwake() const
	{
		return HaveBody() && b2Body_IsAwake(IdOf(this));
	}

	namespace {
		// The up-most surface normal a body is resting against, pointing
		// from the surface to the body. (0, 0) when it touches nothing.
		Vec2 BestGroundNormal(const Physics2D* p, uint32* touching, b2BodyId* under = NULL, b2Pos* at = NULL)
		{
			Vec2 best(0.f, 0.f);
			if (touching) *touching = 0;
			if (!p->HaveBody()) return best;
			const b2BodyId body = IdOf(p);
			b2ContactData data[16];
			const int n = b2Body_GetContactData(body, data, 16);
			bool any = false;
			for (int i = 0; i < n; i++)
			{
				const b2Manifold &m = data[i].manifold;
				if (m.pointCount == 0) continue;
				// A manifold exists a little before the shapes meet; only
				// one that is actually touching counts as standing on it.
				bool touchingNow = false;
				for (int k = 0; k < m.pointCount; k++)
					if (m.points[k].separation < 0.02f) touchingNow = true;
				if (!touchingNow) continue;
				if (touching) (*touching)++;
				// The manifold normal points from shape A to shape B. Turned
				// so that it points at THIS body.
				const bool isA = B2_ID_EQUALS(b2Shape_GetBody(data[i].shapeIdA), body);
				const Vec2 n2 = isA ? Vec2(-m.normal.x, -m.normal.y) : Vec2(m.normal.x, m.normal.y);
				if (!any || n2.y > best.y)
				{
					best = n2;
					any = true;
					if (under) *under = isA ? b2Shape_GetBody(data[i].shapeIdB) : b2Shape_GetBody(data[i].shapeIdA);
					if (at) *at = b2OffsetPos(b2Body_GetWorldCenter(b2Shape_GetBody(data[i].shapeIdA)), m.points[0].anchorA);
				}
			}
			return best;
		}
	}

	bool Physics2D::IsGrounded(const f32 minNormalY) const
	{
		return BestGroundNormal(this, NULL).y >= minNormalY;
	}

	Vec2 Physics2D::GetGroundNormal() const
	{
		const Vec2 n = BestGroundNormal(this, NULL);
		return n.y > 0.f ? n : Vec2(0.f, 0.f);
	}

	Physics2D* Physics2D::GetGroundBody(const f32 minNormalY) const
	{
		b2BodyId under = b2_nullBodyId;
		const Vec2 n = BestGroundNormal(this, NULL, &under, NULL);
		if (n.y < minNormalY || !b2Body_IsValid(under)) return NULL;
		return (Physics2D*)b2Body_GetUserData(under);
	}

	Vec2 Physics2D::GetGroundVelocity(const f32 minNormalY) const
	{
		b2BodyId under = b2_nullBodyId;
		b2Pos at = b2Pos_zero;
		const Vec2 n = BestGroundNormal(this, NULL, &under, &at);
		if (n.y < minNormalY || !b2Body_IsValid(under)) return Vec2(0.f, 0.f);
		const b2Vec2 v = b2Body_GetWorldPointVelocity(under, at);
		return Vec2(v.x, v.y);
	}

	uint32 Physics2D::GetContactCount() const
	{
		uint32 touching = 0;
		BestGroundNormal(this, &touching);
		return touching;
	}

	void Physics2D::SetLinearVelocity(const Vec2 &v)
	{
		if (!HaveBody()) return;
		b2Vec2 bv; bv.x = v.x; bv.y = v.y;
		b2Body_SetLinearVelocity(IdOf(this), bv);
	}

	Vec2 Physics2D::GetLinearVelocity() const
	{
		if (!HaveBody()) return Vec2(0.f, 0.f);
		const b2Vec2 v = b2Body_GetLinearVelocity(IdOf(this));
		return Vec2(v.x, v.y);
	}

	void Physics2D::ApplyForce(const Vec2 &force)
	{
		if (!HaveBody()) return;
		// (kept for the world to hand to its steps - see pendingForce)
		pendingForce.x += force.x; pendingForce.y += force.y;
	}

	void Physics2D::ApplyImpulse(const Vec2 &impulse)
	{
		if (!HaveBody()) return;
		b2Vec2 i; i.x = impulse.x; i.y = impulse.y;
		b2Body_ApplyLinearImpulseToCenter(IdOf(this), i, true);
	}

	void Physics2D::SetAngularVelocity(const f32 radiansPerSecond)
	{
		if (!HaveBody()) return;
		b2Body_SetAngularVelocity(IdOf(this), radiansPerSecond);
	}

	f32 Physics2D::GetAngularVelocity() const
	{
		if (!HaveBody()) return 0.f;
		return b2Body_GetAngularVelocity(IdOf(this));
	}

	void Physics2D::SetTransform(const Vec2 &position, const f32 angle)
	{
		if (!HaveBody()) return;
		b2Pos p; p.x = position.x; p.y = position.y;
		b2Body_SetTransform(IdOf(this), p, b2MakeRot(angle));
	}

	void Physics2D::Wake()
	{
		if (!HaveBody()) return;
		b2Body_SetAwake(IdOf(this), true);
	}

	bool Physics2D::HaveBody() const
	{
		return haveBody && b2Body_IsValid(IdOf(this));
	}

	void Physics2D::SetBodyHandle(const int32 index, const uint16 world, const uint16 generation)
	{
		bodyIndex = index;
		bodyWorld = world;
		bodyGeneration = generation;
		haveBody = true;
	}

};
