#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Other/Export.h>

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

		void ClearScreen();
		void SetViewPort(const int32 &w0, const int32 h0, const int32 w1, const int32 h1);
		void ClearBuffers();
		void Render(const Matrix &camera, const Matrix &projection);

	private:

		IMaterial* DebugMaterial;
		Matrix projectionMatrix;
		Matrix viewMatrix;
		Matrix modelMatrix;

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
