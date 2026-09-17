//============================================================================
// Name        : Physics2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box2D-backed rigid body for 2D scenes
//============================================================================

#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <box2d/box2d.h>

namespace p3d {

	Physics2D::Physics2D(const uint32 bodyType, const uint32 shape, const Vec2 &size)
	{
		this->bodyType = bodyType;
		this->shapeType = shape;
		this->size = size;
		density = 1.f;
		friction = 0.3f;
		restitution = 0.f;
		fixedRotation = false;
		castsShadow = true;
		shapesDirty = false;
		haveBody = false;
		bodyIndex = 0;
		bodyWorld = 0;
		bodyGeneration = 0;
	}

	Physics2D::~Physics2D() {}

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

	void Physics2D::SetLinearVelocity(const Vec2 &v)
	{
		if (!haveBody) return;
		b2Vec2 bv; bv.x = v.x; bv.y = v.y;
		b2Body_SetLinearVelocity(IdOf(this), bv);
	}

	Vec2 Physics2D::GetLinearVelocity() const
	{
		if (!haveBody) return Vec2(0.f, 0.f);
		const b2Vec2 v = b2Body_GetLinearVelocity(IdOf(this));
		return Vec2(v.x, v.y);
	}

	void Physics2D::ApplyForce(const Vec2 &force)
	{
		if (!haveBody) return;
		b2Vec2 f; f.x = force.x; f.y = force.y;
		// wake = true: a force applied to a sleeping body that stays asleep is
		// a bug report waiting to happen.
		b2Body_ApplyForceToCenter(IdOf(this), f, true);
	}

	void Physics2D::ApplyImpulse(const Vec2 &impulse)
	{
		if (!haveBody) return;
		b2Vec2 i; i.x = impulse.x; i.y = impulse.y;
		b2Body_ApplyLinearImpulseToCenter(IdOf(this), i, true);
	}

	void Physics2D::SetAngularVelocity(const f32 radiansPerSecond)
	{
		if (!haveBody) return;
		b2Body_SetAngularVelocity(IdOf(this), radiansPerSecond);
	}

	f32 Physics2D::GetAngularVelocity() const
	{
		if (!haveBody) return 0.f;
		return b2Body_GetAngularVelocity(IdOf(this));
	}

	void Physics2D::SetTransform(const Vec2 &position, const f32 angle)
	{
		if (!haveBody) return;
		b2Pos p; p.x = position.x; p.y = position.y;
		b2Body_SetTransform(IdOf(this), p, b2MakeRot(angle));
	}

	void Physics2D::Wake()
	{
		if (!haveBody) return;
		b2Body_SetAwake(IdOf(this), true);
	}

	void Physics2D::SetBodyHandle(const int32 index, const uint16 world, const uint16 generation)
	{
		bodyIndex = index;
		bodyWorld = world;
		bodyGeneration = generation;
		haveBody = true;
	}

};
