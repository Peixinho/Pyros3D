#include <Pyros3D/Physics/PhysicsEngines/Box3D/DebugDraw/PhysicsDebugDraw.h>

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

	}

	PhysicsDebugDraw::PhysicsDebugDraw()
	{
		debugRenderer = new DebugRenderer();
	}

	PhysicsDebugDraw::~PhysicsDebugDraw()
	{
		delete debugRenderer;
	}

	b3DebugDraw PhysicsDebugDraw::CreateDraw()
	{
		b3DebugDraw draw = b3DefaultDebugDraw();
		draw.context = this;
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
		debugRenderer->ClearBuffers();
	}

	void PhysicsDebugDraw::Render(const Matrix &camera, const Matrix &projection)
	{
		debugRenderer->Render(camera, projection);
	}

	void PhysicsDebugDraw::DrawSegment(b3Pos p1, b3Pos p2, b3HexColor color, void* context)
	{
		PhysicsDebugDraw* self = static_cast<PhysicsDebugDraw*>(context);
		const Vec4 c = HexToVec4(color);
		self->debugRenderer->drawLine(PosToVec3(p1), PosToVec3(p2), c, c);
	}

	void PhysicsDebugDraw::DrawPoint(b3Pos p, float size, b3HexColor color, void* context)
	{
		PhysicsDebugDraw* self = static_cast<PhysicsDebugDraw*>(context);
		self->debugRenderer->drawSphere(PosToVec3(p), size * 0.05f, HexToVec4(color));
	}

	void PhysicsDebugDraw::DrawSphere(b3Pos p, float radius, b3HexColor color, float alpha, void* context)
	{
		PhysicsDebugDraw* self = static_cast<PhysicsDebugDraw*>(context);
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
