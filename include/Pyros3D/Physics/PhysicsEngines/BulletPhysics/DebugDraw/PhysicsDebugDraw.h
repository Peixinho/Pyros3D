#ifndef PHYSICS_DEBUG_DRAW_H
#define PHYSICS_DEBUG_DRAW_H

#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Physics/PhysicsEngines/BulletPhysics/BulletPhysics.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

    using namespace Uniforms;

    class PhysicsDebugDraw : public btIDebugDraw
    {
		int m_debugMode;

        public:

            PhysicsDebugDraw();
            virtual ~PhysicsDebugDraw();

            virtual void drawLine(const btVector3& from,const btVector3& to,const btVector3& fromColor, const btVector3& toColor);

            virtual void drawLine(const btVector3& from,const btVector3& to,const btVector3& color);

            virtual void drawSphere (const btVector3& p, btScalar radius, const btVector3& color);

            virtual void drawTriangle(const btVector3& a,const btVector3& b,const btVector3& c,const btVector3& color,btScalar alpha);

            virtual void drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color);

            virtual void reportErrorWarning(const char* warningString);

            virtual void draw3dText(const btVector3& location,const char* textString);

            virtual void setDebugMode(int debugMode);

            virtual int  getDebugMode() const { return m_debugMode;}
            
			void ClearBuffers();
			void Render(const Matrix &camera, const Matrix &projection);

		protected:

			DebugRenderer* debugRenderer;
            
    };

};

#endif//PHYSICS_DEBUG_DRAW_H
