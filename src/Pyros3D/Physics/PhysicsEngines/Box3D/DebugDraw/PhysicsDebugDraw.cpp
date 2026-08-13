#include <Pyros3D/Physics/PhysicsEngines/Box3D/DebugDraw/PhysicsDebugDraw.h>

#include <box3d/collision.h>
#include <iostream>

namespace p3d {

	namespace {

		Vec4 HexToVec4(b3HexColor color, float alpha = 1.f)
		{
			const uint32 rgb = (uint32)color & 0x00FFFFFFu;
			return Vec4(
				((rgb >> 16) & 0xFF) / 255.f,
				((rgb >> 8) & 0xFF) / 255.f,
				(rgb & 0xFF) / 255.f,
				alpha
			);
		}

		Vec3 PosToVec3(b3Pos p)
		{
			return Vec3((f32)p.x, (f32)p.y, (f32)p.z);
		}

		Vec3 B3ToVec3(b3Vec3 v)
		{
			return Vec3(v.x, v.y, v.z);
		}

		b3Transform WorldToB3Transform(b3WorldTransform transform)
		{
#if defined(BOX3D_DOUBLE_PRECISION)
			b3Transform out;
			out.p = b3InvTransformWorldPoint(transform, transform.p);
			out.q = transform.q;
			out.p.x = (f32)transform.p.x;
			out.p.y = (f32)transform.p.y;
			out.p.z = (f32)transform.p.z;
			return out;
#else
			return transform;
#endif
		}

		void DrawHullWireframe(DebugRenderer* debug, const b3HullData* hull, b3Transform xf, const Vec4& color)
		{
			if (!debug || !hull || hull->faceCount <= 0) return;

			const b3HullHalfEdge* edges = b3GetHullEdges(hull);
			const b3HullFace* faces = b3GetHullFaces(hull);
			const b3Vec3* points = b3GetHullPoints(hull);
			if (!edges || !faces || !points) return;

			for (int fi = 0; fi < hull->faceCount; ++fi)
			{
				uint8_t start = faces[fi].edge;
				uint8_t e = start;
				do
				{
					const uint8_t next = edges[e].next;
					b3Vec3 p0 = b3TransformPoint(xf, points[edges[e].origin]);
					b3Vec3 p1 = b3TransformPoint(xf, points[edges[next].origin]);
					debug->drawLine(B3ToVec3(p0), B3ToVec3(p1), color);
					e = next;
				} while (e != start);
			}
		}

		void DrawCapsuleWireframe(DebugRenderer* debug, const b3Capsule* capsule, b3Transform xf, const Vec4& color)
		{
			if (!debug || !capsule) return;

			b3Vec3 c1 = b3TransformPoint(xf, capsule->center1);
			b3Vec3 c2 = b3TransformPoint(xf, capsule->center2);
			debug->drawLine(B3ToVec3(c1), B3ToVec3(c2), color);

			const f32 r = capsule->radius;
			debug->drawSphere(B3ToVec3(c1), r, color);
			debug->drawSphere(B3ToVec3(c2), r, color);
		}

	}

	PhysicsDebugDraw::PhysicsDebugDraw()
		: debugRenderer(nullptr), ownsDebugRenderer(false)
	{
	}

	PhysicsDebugDraw::~PhysicsDebugDraw()
	{
		if (ownsDebugRenderer)
			delete debugRenderer;
		debugRenderer = nullptr;
		ownsDebugRenderer = false;
	}

	void PhysicsDebugDraw::SetDebugRenderer(DebugRenderer* renderer)
	{
		if (ownsDebugRenderer)
			delete debugRenderer;
		debugRenderer = renderer;
		ownsDebugRenderer = false;
	}

	void PhysicsDebugDraw::EnsureDebugRenderer()
	{
		if (debugRenderer != nullptr)
			return;
		debugRenderer = new DebugRenderer();
		ownsDebugRenderer = true;
	}

	void* PhysicsDebugDraw::CreateDebugShapeCallback(const b3DebugShape* debugShape, void* userContext)
	{
		(void)userContext;
		if (!debugShape) return NULL;
		b3DebugShape* stored = new b3DebugShape();
		*stored = *debugShape;
		return stored;
	}

	void PhysicsDebugDraw::DestroyDebugShapeCallback(void* userShape, void* userContext)
	{
		(void)userContext;
		delete static_cast<b3DebugShape*>(userShape);
	}

	b3DebugDraw PhysicsDebugDraw::CreateDraw()
	{
		b3DebugDraw draw = b3DefaultDebugDraw();
		draw.context = this;
		draw.DrawShapeFcn = DrawShape;
		draw.DrawSegmentFcn = DrawSegment;
		draw.DrawPointFcn = DrawPoint;
		draw.DrawSphereFcn = DrawSphere;
		draw.DrawStringFcn = DrawString;
		draw.drawShapes = true;
		draw.drawJoints = true;
		draw.drawContacts = true;
		draw.drawBounds = false;
		return draw;
	}

	void PhysicsDebugDraw::ClearBuffers()
	{
		EnsureDebugRenderer();
		debugRenderer->ClearBuffers();
	}

	void PhysicsDebugDraw::Render(const Matrix &camera, const Matrix &projection)
	{
		EnsureDebugRenderer();
		debugRenderer->Render(camera, projection);
	}

	bool PhysicsDebugDraw::DrawShape(void* userShape, b3WorldTransform transform, b3HexColor color, void* context)
	{
		PhysicsDebugDraw* self = static_cast<PhysicsDebugDraw*>(context);
		if (!self || !userShape) return true;

		self->EnsureDebugRenderer();
		const b3DebugShape* shape = static_cast<const b3DebugShape*>(userShape);
		const Vec4 c = HexToVec4(color, 0.85f);
		const b3Transform xf = WorldToB3Transform(transform);

		switch (shape->type)
		{
		case b3_sphereShape:
			if (shape->sphere)
			{
				b3Vec3 center = b3TransformPoint(xf, shape->sphere->center);
				self->debugRenderer->drawSphere(B3ToVec3(center), shape->sphere->radius, c);
			}
			break;
		case b3_capsuleShape:
			if (shape->capsule)
				DrawCapsuleWireframe(self->debugRenderer, shape->capsule, xf, c);
			break;
		case b3_hullShape:
			if (shape->hull)
				DrawHullWireframe(self->debugRenderer, shape->hull, xf, c);
			break;
		default:
			break;
		}

		return true;
	}

	void PhysicsDebugDraw::DrawSegment(b3Pos p1, b3Pos p2, b3HexColor color, void* context)
	{
		PhysicsDebugDraw* self = static_cast<PhysicsDebugDraw*>(context);
		self->EnsureDebugRenderer();
		const Vec4 c = HexToVec4(color);
		self->debugRenderer->drawLine(PosToVec3(p1), PosToVec3(p2), c, c);
	}

	void PhysicsDebugDraw::DrawPoint(b3Pos p, float size, b3HexColor color, void* context)
	{
		PhysicsDebugDraw* self = static_cast<PhysicsDebugDraw*>(context);
		self->EnsureDebugRenderer();
		self->debugRenderer->drawSphere(PosToVec3(p), size * 0.05f, HexToVec4(color));
	}

	void PhysicsDebugDraw::DrawSphere(b3Pos p, float radius, b3HexColor color, float alpha, void* context)
	{
		PhysicsDebugDraw* self = static_cast<PhysicsDebugDraw*>(context);
		self->EnsureDebugRenderer();
		self->debugRenderer->drawSphere(PosToVec3(p), radius, HexToVec4(color, alpha));
	}

	void PhysicsDebugDraw::DrawString(b3Pos p, const char* s, b3HexColor color, void* context)
	{
		(void)p;
		(void)color;
		(void)context;
		if (s) std::cout << s << std::endl;
	}

};
