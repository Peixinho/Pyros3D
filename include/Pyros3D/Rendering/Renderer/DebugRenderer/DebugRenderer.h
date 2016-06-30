#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	using namespace Uniforms;

	class DebugRenderer
	{

	public:

		DebugRenderer();
		virtual ~DebugRenderer();

		virtual void drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &fromColor, const Vec4 &toColor);

		virtual void drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &color);

		virtual void drawSphere(const Vec3 &p, f32 radius, const Vec4 &color);

		virtual void drawTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec4 &color);

		void ClearBuffers();
		void Render(const Matrix &camera, const Matrix &projection);

	private:

		IMaterial* DebugMaterial;
		Matrix projectionMatrix;
		Matrix viewMatrix;
		Matrix modelMatrix;

		std::vector<Vec3> vertexLines;
		std::vector<Vec4> colorLines;
		std::vector<Vec3> vertexTriangles;
		std::vector<Vec4> colorTriangles;

	};

};

#endif /* DEBUG_RENDERER_H */
