#include <Pyros3D/Physics/PhysicsEngines/BulletPhysics/DebugDraw/PhysicsDebugDraw.h>
#include <Pyros3D/Other/PyrosGL.h>

#include <iostream>

namespace p3d {

	PhysicsDebugDraw::PhysicsDebugDraw() :m_debugMode(0)
	{
		debugRenderer = new DebugRenderer();
	}

	PhysicsDebugDraw::~PhysicsDebugDraw()
	{
		delete debugRenderer;
	}

	void PhysicsDebugDraw::ClearBuffers()
	{
		debugRenderer->ClearBuffers();
	}

	void PhysicsDebugDraw::Render(const Matrix &camera, const Matrix &projection)
	{
		debugRenderer->Render(camera, projection);
	}

	void PhysicsDebugDraw::drawLine(const btVector3& from, const btVector3& to, const btVector3& fromColor, const btVector3& toColor)
	{
		debugRenderer->drawLine(
			Vec3(from.getX(), from.getY(), from.getZ()),
			Vec3(to.getX(), to.getY(), to.getZ()),
			Vec4(fromColor.getX(), fromColor.getY(), fromColor.getZ(), 1.f),
			Vec4(toColor.getX(), toColor.getY(), toColor.getZ(), 1.f)
			);
	}

	void PhysicsDebugDraw::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
	{
		drawLine(from, to, color, color);
	}

	void PhysicsDebugDraw::drawSphere(const btVector3& p, btScalar radius, const btVector3& color)
	{

		debugRenderer->drawSphere(Vec3(p.getX(), p.getY(), p.getZ()), radius, Vec4(color.getX(), color.getY(), color.getZ(), 1.f));
	}

	void PhysicsDebugDraw::drawTriangle(const btVector3& a, const btVector3& b, const btVector3& c, const btVector3& color, btScalar alpha)
	{
		debugRenderer->drawTriangle(
			Vec3(a.getX(), a.getY(), a.getZ()),
			Vec3(b.getX(), b.getY(), b.getZ()),
			Vec3(c.getX(), c.getY(), c.getZ()),
			Vec4(color.getX(), color.getY(), color.getZ(), alpha)
			);
	}

	void PhysicsDebugDraw::setDebugMode(int debugMode)
	{
		m_debugMode = debugMode;
	}

	void PhysicsDebugDraw::draw3dText(const btVector3& location, const char* textString)
	{
		std::cout << textString << std::endl;
	}

	void PhysicsDebugDraw::reportErrorWarning(const char* warningString)
	{
		std::cout << warningString << std::endl;
	}

	void PhysicsDebugDraw::drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
	{
		btVector3 to = pointOnB + normalOnB * 1;//distance;
		const btVector3&from = pointOnB;
		drawLine(from, to, color, color);
	}

};