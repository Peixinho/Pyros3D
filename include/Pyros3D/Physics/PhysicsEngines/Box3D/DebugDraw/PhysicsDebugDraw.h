//============================================================================
// Name        : PhysicsDebugDraw.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box3D Physics Debug Draw
//============================================================================

#ifndef PHYSICS_DEBUG_DRAW_H
#define PHYSICS_DEBUG_DRAW_H

#include <box3d/box3d.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	class PYROS3D_API PhysicsDebugDraw
	{
	public:

		PhysicsDebugDraw();
		~PhysicsDebugDraw();

		b3DebugDraw CreateDraw();

		void ClearBuffers();
		void Render(const Matrix &camera, const Matrix &projection);

	protected:

		DebugRenderer* debugRenderer;

		static void DrawSegment(b3Pos p1, b3Pos p2, b3HexColor color, void* context);
		static void DrawPoint(b3Pos p, float size, b3HexColor color, void* context);
		static void DrawSphere(b3Pos p, float radius, b3HexColor color, float alpha, void* context);
		static void DrawString(b3Pos p, const char* s, b3HexColor color, void* context);
	};

};

#endif // PHYSICS_DEBUG_DRAW_H
