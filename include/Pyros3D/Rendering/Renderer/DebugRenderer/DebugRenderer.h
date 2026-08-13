#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <memory>

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

		void EnsureGpuResources();
		void DestroyGpuResources();
		DeviceHandle EnsurePipeline(const uint32 drawingType);

		IMaterial* DebugMaterial;
		Matrix projectionMatrix;
		Matrix viewMatrix;
		Matrix modelMatrix;

		bool pushedMatrix;
		Matrix temp;
		bool holdsSharedUbos;

		std::vector<Vec3> vertexLines;
		std::vector<Vec4> colorLines;
		std::vector<Vec3> vertexTriangles;
		std::vector<Vec4> colorTriangles;
		std::vector<Vec3> points; // w is the size of the point
		std::vector<Vec4> colorPoints;
		std::vector<f32> pointsSize;

		// Buffers
		GeometryBuffer* VertexLinesBF = nullptr;
		GeometryBuffer* ColorLinesBF = nullptr;
		GeometryBuffer* VertexTrianglesBF = nullptr;
		GeometryBuffer* ColorTrianglesBF = nullptr;
		GeometryBuffer* PointsBF = nullptr;
		GeometryBuffer* ColorPointsBF = nullptr;
		GeometryBuffer* PointsSizeBF = nullptr;

		// Stable VAOs + pipelines (same model as IRenderer::BindMesh /
		// PostEffectsManager). Built once; pipelines rebuilt when the
		// render target / swapchain generation changes.
		DeviceHandle linesVao;
		DeviceHandle trianglesVao;
		DeviceHandle pointsVao;
		bool linesVaoBuilt;
		bool trianglesVaoBuilt;
		bool pointsVaoBuilt;
		DeviceHandle linesPipeline;
		DeviceHandle trianglesPipeline;
		DeviceHandle pointsPipeline;
		DeviceHandle pipelinesBuiltForTarget;
		uint32 pipelinesBuiltForSwapchainGeneration;

		// See IRenderDevice.h - same seam IRenderer uses, since
		// DebugRenderer doesn't inherit IRenderer and has its own small
		// GL call surface.
		// MaybeOwningDevicePtr, not unique_ptr: this used to always own a
		// freshly constructed GLRenderDevice, which jumps through a NULL
		// glad pointer the moment it is used in a Vulkan or Metal process
		// (no GL context exists). Borrow the active device instead - the
		// same fix PostEffectsManager already carries.
		MaybeOwningDevicePtr device;

	};

};

#endif /* DEBUG_RENDERER_H */
