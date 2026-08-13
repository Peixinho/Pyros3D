//============================================================================
// Name        : PhysicsDebugDraw.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box3D Physics Debug Draw
//============================================================================

#ifndef PHYSICS_DEBUG_DRAW_H
#define	PHYSICS_DEBUG_DRAW_H

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

		// When set, physics debug draws into this renderer instead of an
		// owned one. Caller retains ownership (SceneEditor shares its
		// gizmo/bounds DebugRenderer). Do not delete via ~PhysicsDebugDraw.
		void SetDebugRenderer(DebugRenderer* renderer);
		bool OwnsDebugRenderer() const { return ownsDebugRenderer; }
		bool HasDebugRenderer() const { return debugRenderer != nullptr; }
		void EnsureDebugRenderer();

		void ClearBuffers();
		void Render(const Matrix &camera, const Matrix &projection);

		static void* CreateDebugShapeCallback(const b3DebugShape* debugShape, void* userContext);
		static void DestroyDebugShapeCallback(void* userShape, void* userContext);

	protected:

		DebugRenderer* debugRenderer;
		bool ownsDebugRenderer;

		static bool DrawShape(void* userShape, b3WorldTransform transform, b3HexColor color, void* context);
		static void DrawSegment(b3Pos p1, b3Pos p2, b3HexColor color, void* context);
		static void DrawPoint(b3Pos p, float size, b3HexColor color, void* context);
		static void DrawSphere(b3Pos p, float radius, b3HexColor color, float alpha, void* context);
		static void DrawString(b3Pos p, const char* s, b3HexColor color, void* context);
	};

};

#endif // PHYSICS_DEBUG_DRAW_H
