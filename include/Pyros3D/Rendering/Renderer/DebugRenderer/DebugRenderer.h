#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	class PYROS3D_API DebugRenderer
	{

	public:

		DebugRenderer();
		virtual ~DebugRenderer();

		virtual void drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &fromColor, const Vec4 &toColor);
		virtual void drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &color);
		virtual void drawSphere(const Vec3 &p, f32 radius, const Vec4 &color);
		virtual void drawCone(const f32 radius, const f32 height, const Vec4 &color);
		virtual void drawCylinder(const f32 radius, const f32 height, const Vec4 &color);
		virtual void drawPoint(const Vec3 &point, const f32 size, const Vec4 &color);

		void pushMatrix(const Matrix &m);
		void popMatrix();

		virtual void drawTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec4 &color);

		// Drops what was batched
		void ClearBuffers();
		// What was batched since the last take, handed over and let go: lines (pairs) and triangles (threes) in world
		// space, a colour each - the renderer draws them over its picture (SceneRenderer::AddOverlay). A point comes as a
		// small cross of lines.
		void TakeBatches(std::vector<Vec3> &lines, std::vector<Vec4> &lineColors, std::vector<Vec3> &triangles, std::vector<Vec4> &triangleColors);

	private:

		bool pushedMatrix;
		Matrix temp;

		std::vector<Vec3> vertexLines;
		std::vector<Vec4> colorLines;
		std::vector<Vec3> vertexTriangles;
		std::vector<Vec4> colorTriangles;
		std::vector<Vec3> points; // w is the size of the point
		std::vector<Vec4> colorPoints;
		std::vector<f32> pointsSize;

	};

};

#endif /* DEBUG_RENDERER_H */
