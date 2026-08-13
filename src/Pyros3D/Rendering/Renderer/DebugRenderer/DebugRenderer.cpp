#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <cstring>
#include <iostream>

namespace p3d {

	// Same reasoning as PostEffectsManager's ResolvePostEffectsDevice()
	// (PostEffectsManager.cpp): borrow the already-active device rather
	// than always constructing a GLRenderDevice, which has no valid glad
	// function pointers in a Vulkan- or Metal-only process. The editor hit
	// this immediately on Vulkan - DebugRenderer::Render() called
	// CreateVertexArray() and jumped to address 0.
	static MaybeOwningDevicePtr ResolveDebugRendererDevice()
	{
		if (IsActiveRenderDeviceSet())
			return MaybeOwningDevicePtr(&GetActiveRenderDevice(), MaybeOwningDeviceDeleter{false});
		return MaybeOwningDevicePtr(new GLRenderDevice(), MaybeOwningDeviceDeleter{true});
	}

	DebugRenderer::DebugRenderer() : device(ResolveDebugRendererDevice())
	{
		DebugMaterial = new GenericShaderMaterial(ShaderUsage::DebugRendering);
		// Editor gizmos / selection bounds / physics wireframes must read
		// through scene geometry. Depth-tested LEqual hid the transform
		// gizmo behind meshes; ClearBufferBit(Depth) before the flush was
		// tried and rejected in SceneEditor because it rewrites the
		// renderer's persistent clear mask. Same contract as
		// GameObjectHelper's overlay materials.
		DebugMaterial->DisableDepthTest();
		DebugMaterial->DisableDepthWrite();
		DebugMaterial->SetCullFace(CullFace::DoubleSided);
		pushedMatrix = false;
		temp = Matrix();
		holdsSharedUbos = false;

		VertexLinesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		ColorLinesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		VertexTrianglesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		ColorTrianglesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		PointsBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		ColorPointsBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		PointsSizeBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);

		linesVao = trianglesVao = pointsVao = 0;
		linesVaoBuilt = trianglesVaoBuilt = pointsVaoBuilt = false;
		linesPipeline = trianglesPipeline = pointsPipeline = 0;
		pipelinesBuiltForTarget = 0;
		pipelinesBuiltForSwapchainGeneration = 0;

		IRenderer::RetainSharedUniformBuffers(device.get());
		holdsSharedUbos = true;
	}

	DebugRenderer::~DebugRenderer()
	{
		DestroyGpuResources();
		if (holdsSharedUbos)
		{
			IRenderer::ReleaseSharedUniformBuffers(device.get());
			holdsSharedUbos = false;
		}
		delete DebugMaterial;
		if (VertexLinesBF) delete VertexLinesBF;
		if (ColorLinesBF) delete ColorLinesBF;
		if (VertexTrianglesBF) delete VertexTrianglesBF;
		if (ColorTrianglesBF) delete ColorTrianglesBF;
		if (PointsBF) delete PointsBF;
		if (ColorPointsBF) delete ColorPointsBF;
		if (PointsSizeBF) delete PointsSizeBF;
	}

	void DebugRenderer::DestroyGpuResources()
	{
		if (linesPipeline != 0)
		{
			device->DestroyPipeline(linesPipeline);
			linesPipeline = 0;
		}
		if (trianglesPipeline != 0)
		{
			device->DestroyPipeline(trianglesPipeline);
			trianglesPipeline = 0;
		}
		if (pointsPipeline != 0)
		{
			device->DestroyPipeline(pointsPipeline);
			pointsPipeline = 0;
		}
		if (linesVao != 0)
		{
			device->DeleteVertexArray(linesVao);
			linesVao = 0;
			linesVaoBuilt = false;
		}
		if (trianglesVao != 0)
		{
			device->DeleteVertexArray(trianglesVao);
			trianglesVao = 0;
			trianglesVaoBuilt = false;
		}
		if (pointsVao != 0)
		{
			device->DeleteVertexArray(pointsVao);
			pointsVao = 0;
			pointsVaoBuilt = false;
		}
		pipelinesBuiltForTarget = 0;
		pipelinesBuiltForSwapchainGeneration = 0;
	}

	void DebugRenderer::EnsureGpuResources()
	{
		// GeometryBuffer handles are only valid after the first Init();
		// callers update CPU vectors first so Init/Update runs before this.
		if (!linesVaoBuilt && VertexLinesBF->ID >= 0 && ColorLinesBF->ID >= 0)
		{
			linesVao = device->CreateVertexArray();
			CommandBufferHandle cmd = device->BeginCommandBuffer();
			device->BindVertexArray(cmd, linesVao);
			device->BindArrayBuffer(VertexLinesBF->ID);
			device->SetFloatVertexAttribute(device->GetAttributeLocation(DebugMaterial->GetShader(), "aPosition"), 3, sizeof(Vec3), 0);
			device->BindArrayBuffer(ColorLinesBF->ID);
			device->SetFloatVertexAttribute(device->GetAttributeLocation(DebugMaterial->GetShader(), "aColor"), 4, sizeof(Vec4), 0);
			device->EndCommandBuffer(cmd);
			linesVaoBuilt = true;
		}

		if (!trianglesVaoBuilt && VertexTrianglesBF->ID >= 0 && ColorTrianglesBF->ID >= 0)
		{
			trianglesVao = device->CreateVertexArray();
			CommandBufferHandle cmd = device->BeginCommandBuffer();
			device->BindVertexArray(cmd, trianglesVao);
			device->BindArrayBuffer(VertexTrianglesBF->ID);
			device->SetFloatVertexAttribute(device->GetAttributeLocation(DebugMaterial->GetShader(), "aPosition"), 3, sizeof(Vec3), 0);
			device->BindArrayBuffer(ColorTrianglesBF->ID);
			device->SetFloatVertexAttribute(device->GetAttributeLocation(DebugMaterial->GetShader(), "aColor"), 4, sizeof(Vec4), 0);
			device->EndCommandBuffer(cmd);
			trianglesVaoBuilt = true;
		}

		if (!pointsVaoBuilt && PointsBF->ID >= 0 && ColorPointsBF->ID >= 0)
		{
			pointsVao = device->CreateVertexArray();
			CommandBufferHandle cmd = device->BeginCommandBuffer();
			device->BindVertexArray(cmd, pointsVao);
			device->BindArrayBuffer(PointsBF->ID);
			device->SetFloatVertexAttribute(device->GetAttributeLocation(DebugMaterial->GetShader(), "aPosition"), 3, sizeof(Vec3), 0);
			device->BindArrayBuffer(ColorPointsBF->ID);
			device->SetFloatVertexAttribute(device->GetAttributeLocation(DebugMaterial->GetShader(), "aColor"), 4, sizeof(Vec4), 0);
			device->EndCommandBuffer(cmd);
			pointsVaoBuilt = true;
		}
	}

	DeviceHandle DebugRenderer::EnsurePipeline(const uint32 drawingType)
	{
		const DeviceHandle target = device->GetCurrentRenderTarget();
		const uint32 generation = device->GetSwapchainGeneration();
		const bool stale = (linesPipeline != 0 || trianglesPipeline != 0 || pointsPipeline != 0) &&
			(pipelinesBuiltForTarget != target ||
			 (target == 0 && pipelinesBuiltForSwapchainGeneration != generation));
		if (stale)
		{
			if (linesPipeline != 0) { device->DestroyPipeline(linesPipeline); linesPipeline = 0; }
			if (trianglesPipeline != 0) { device->DestroyPipeline(trianglesPipeline); trianglesPipeline = 0; }
			if (pointsPipeline != 0) { device->DestroyPipeline(pointsPipeline); pointsPipeline = 0; }
		}

		DeviceHandle &pipeline = (drawingType == DrawingType::Lines) ? linesPipeline :
			(drawingType == DrawingType::Triangles) ? trianglesPipeline : pointsPipeline;
		if (pipeline != 0)
			return pipeline;

		IRenderDevice::PipelineDesc pdesc;
		pdesc.shaderProgram = DebugMaterial->GetShader();
		pdesc.depthTest = DebugMaterial->IsDepthTesting();
		// Mode is ignored while depthTest is false (Metal forces Always);
		// keep Always so a later EnableDepthTest without an arg still draws
		// on top if pipelines are rebuilt.
		pdesc.depthTestMode = DepthTest::Always;
		pdesc.depthWrite = DebugMaterial->IsDepthWritting();
		pdesc.cullFace = DebugMaterial->GetCullFace();
		pdesc.drawingType = drawingType;
		pdesc.blendingEnabled = true;
		pdesc.blendSrcFactor = BlendFunc::Src_Alpha;
		pdesc.blendDstFactor = BlendFunc::One_Minus_Src_Alpha;
		pdesc.blendEquation = BlendEq::Add;

		IRenderDevice::VertexBufferLayoutDesc positionLayout;
		positionLayout.stride = sizeof(Vec3);
		{
			IRenderDevice::VertexAttributeDesc attr;
			attr.name = "aPosition";
			attr.type = Buffer::Attribute::Type::Vec3;
			attr.offset = 0;
			attr.divisor = 0;
			positionLayout.attributes.push_back(attr);
		}
		IRenderDevice::VertexBufferLayoutDesc colorLayout;
		colorLayout.stride = sizeof(Vec4);
		{
			IRenderDevice::VertexAttributeDesc attr;
			attr.name = "aColor";
			attr.type = Buffer::Attribute::Type::Vec4;
			attr.offset = 0;
			attr.divisor = 0;
			colorLayout.attributes.push_back(attr);
		}
		pdesc.vertexLayout.push_back(positionLayout);
		pdesc.vertexLayout.push_back(colorLayout);

		pipeline = device->CreatePipeline(pdesc);
		pipelinesBuiltForTarget = target;
		pipelinesBuiltForSwapchainGeneration = generation;

		// Same UBO block binds BindMesh() does for PyrosShader materials.
		const uint32 program = DebugMaterial->GetShader();
		device->BindUniformBlockIfPresent(program, "GlobalMatrices", 0);
		device->BindUniformBlockIfPresent(program, "ObjectMatrixUniforms", 18);
		device->BindUniformBlockIfPresent(program, "MaterialUniforms", 22);

		return pipeline;
	}

	void DebugRenderer::ClearScreen()
	{
		device->Clear(device->TranslateBufferBit(Buffer_Bit::Color));
	}
	void DebugRenderer::SetViewPort(const int32 &w0, const int32 h0, const int32 w1, const int32 h1)
	{
		device->SetViewport(w0, h0, w1, h1);
	}

	void DebugRenderer::ClearBuffers()
	{
		vertexLines.clear();
		colorLines.clear();
		vertexTriangles.clear();
		colorTriangles.clear();
		points.clear();
		colorPoints.clear();
		pointsSize.clear();
	}

	void DebugRenderer::Render(const Matrix &camera, const Matrix &projection)
	{
		const bool hasLines = !vertexLines.empty();
		const bool hasTriangles = !vertexTriangles.empty();
		const bool hasPoints = !points.empty();
		if (!hasLines && !hasTriangles && !hasPoints)
		{
			ClearBuffers();
			return;
		}

		// Caller must already have an open render pass (editor: inside
		// CaptureFrame; examples: during ForwardRenderer's frame). Do not
		// BeginFrame/EndFrame here - nesting EndFrame after Forward already
		// started a swapchain frame ends the frame early on Vulkan/Metal.

		if (hasLines)
		{
			if (VertexLinesBF->ID >= 0)
				VertexLinesBF->Update(&vertexLines[0], vertexLines.size() * sizeof(Vec3));
			else
				VertexLinesBF->Init(&vertexLines[0], vertexLines.size() * sizeof(Vec3));

			if (ColorLinesBF->ID >= 0)
				ColorLinesBF->Update(&colorLines[0], colorLines.size() * sizeof(Vec4));
			else
				ColorLinesBF->Init(&colorLines[0], colorLines.size() * sizeof(Vec4));
		}

		if (hasTriangles)
		{
			if (VertexTrianglesBF->ID >= 0)
				VertexTrianglesBF->Update(&vertexTriangles[0], vertexTriangles.size() * sizeof(Vec3));
			else
				VertexTrianglesBF->Init(&vertexTriangles[0], vertexTriangles.size() * sizeof(Vec3));

			if (ColorTrianglesBF->ID >= 0)
				ColorTrianglesBF->Update(&colorTriangles[0], colorTriangles.size() * sizeof(Vec4));
			else
				ColorTrianglesBF->Init(&colorTriangles[0], colorTriangles.size() * sizeof(Vec4));
		}

		if (hasPoints)
		{
			if (PointsBF->ID >= 0)
				PointsBF->Update(&points[0], points.size() * sizeof(Vec3));
			else
				PointsBF->Init(&points[0], points.size() * sizeof(Vec3));

			if (ColorPointsBF->ID >= 0)
				ColorPointsBF->Update(&colorPoints[0], colorPoints.size() * sizeof(Vec4));
			else
				ColorPointsBF->Init(&colorPoints[0], colorPoints.size() * sizeof(Vec4));
		}

		EnsureGpuResources();

		projectionMatrix = projection;
		viewMatrix = camera;
		modelMatrix.identity();

		// Upload through the shared IRenderer UBO handles (bindings 0/18/22)
		// - same contract as RenderObject(), required on Vulkan/Metal where
		// loose SendUniformMatrix is a no-op.
		//
		// Do NOT rewrite MaterialUniformsUBO here. The editor flushes debug
		// lines after the lit scene pass; zeroing UseLights in the shared
		// material block made the next frame's shadowed materials sample as
		// unlit (full albedo → looks "brighter") until something forced a
		// full material reupload. Only view/proj/model are needed for lines.
		Matrix globalMatricesData[2] = {
			device->TranslateProjectionMatrix(projectionMatrix),
			viewMatrix
		};
		device->ReplaceUniformBuffer(IRenderer::GetSharedGlobalMatricesUBO(), sizeof(Matrix) * 2, globalMatricesData);

		struct { Matrix model; Vec4 wind; } objectMatrixData;
		objectMatrixData.model = modelMatrix;
		objectMatrixData.wind = Vec4(0.f, 0.f, 0.f, 0.f);
		device->ReplaceUniformBuffer(IRenderer::GetSharedObjectMatrixUniformsUBO(), sizeof(objectMatrixData), &objectMatrixData);

		IRenderer::MarkSharedGlobalMatricesDirty();

		CommandBufferHandle cmd = device->BeginCommandBuffer();

		if (hasLines && linesVaoBuilt)
		{
			DeviceHandle pipeline = EnsurePipeline(DrawingType::Lines);
			if (pipeline != 0)
			{
				device->BindVertexArray(cmd, linesVao);
				device->BindPipeline(cmd, pipeline);
				device->DrawArrays(device->TranslateDrawType(DrawingType::Lines), 0, (uint32)vertexLines.size());
			}
		}

		if (hasTriangles && trianglesVaoBuilt)
		{
			DeviceHandle pipeline = EnsurePipeline(DrawingType::Triangles);
			if (pipeline != 0)
			{
				device->BindVertexArray(cmd, trianglesVao);
				device->BindPipeline(cmd, pipeline);
				device->DrawArrays(device->TranslateDrawType(DrawingType::Triangles), 0, (uint32)vertexTriangles.size());
			}
		}

		if (hasPoints && pointsVaoBuilt)
		{
			DeviceHandle pipeline = EnsurePipeline(DrawingType::Points);
			if (pipeline != 0)
			{
				device->BindVertexArray(cmd, pointsVao);
				device->BindPipeline(cmd, pipeline);
				device->DrawArrays(device->TranslateDrawType(DrawingType::Points), 0, (uint32)points.size());
			}
		}

		device->EndCommandBuffer(cmd);

		ClearBuffers();
	}

	void DebugRenderer::drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &fromColor, const Vec4 &toColor)
	{
		vertexLines.push_back(temp * from);
		vertexLines.push_back(temp * to);
		colorLines.push_back(fromColor);
		colorLines.push_back(toColor);
	}

	void DebugRenderer::drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &color)
	{
		drawLine(from, to, color, color);
	}

	void DebugRenderer::drawPoint(const Vec3 &point, const f32 size, const Vec4 &color)
	{
		points.push_back(point);
		colorPoints.push_back(Vec4(color));
		pointsSize.push_back(size);
	}

	void DebugRenderer::drawSphere(const Vec3 &p, f32 radius, const Vec4 &color)
	{
		Vec3 pos = p;

		uint32 latitudeBands = 8;
		uint32 longitudeBands = 8;
		std::vector<Vec3> v;
		for (uint32 latNumber = 0; latNumber <= latitudeBands; latNumber++) {
			f32 theta = (f32)(latNumber * PI / latitudeBands);
			f32 sinTheta = sinf(theta);
			f32 cosTheta = cosf(theta);
			for (uint32 longNumber = 0; longNumber <= longitudeBands; longNumber++) {
				f32 phi = (f32)(longNumber * 2 * PI / longitudeBands);
				f32 sinPhi = sinf(phi);
				f32 cosPhi = cosf(phi);
				f32 x = cosPhi * sinTheta;
				f32 y = cosTheta;
				f32 z = sinPhi * sinTheta;
				v.push_back(pos + Vec3(radius * x, radius * y, radius * z));
			}
		}

		for (uint32 latNumber = 0; latNumber < latitudeBands; latNumber++) {
			for (uint32 longNumber = 0; longNumber < longitudeBands; longNumber++) {
				uint32 first = (latNumber * (longitudeBands + 1)) + longNumber;
				uint32 second = first + longitudeBands + 1;
				drawLine(v[first], v[second], color);
			}
		}
	}

	void DebugRenderer::drawCylinder(const f32 radius, const f32 height, const Vec4 &color)
	{
		uint32 segmentsH = 1;
		uint32 segmentsW = 12;

		int jMin, jMax;

		std::vector <Vec3> aRowT, aRowB;

		std::vector <std::vector<Vec3> > aVtc;

		jMin = 0;
		jMax = segmentsH;

		for (int j = jMin; j <= jMax; ++j) {
			f32 z = -height + 2 * height*(f32)(j - jMin) / (f32)(jMax - jMin);
			std::vector <Vec3> aRow;

			for (size_t i = 0; i < segmentsW; ++i) {
				f32 verangle = (f32)(2 * (f32)i / segmentsW*PI);
				f32 x = radius * sin(verangle);
				f32 y = radius * cos(verangle);
				Vec3 oVtx = Vec3(y, z, x);
				aRow.push_back(oVtx);
			}
			aVtc.push_back(aRow);
		}

		aVtc.push_back(aRowT);

		for (size_t j = 1; j <= segmentsH; ++j) {
			for (size_t i = 0; i < segmentsW; ++i) {
				Vec3 a = aVtc[j][i];
				Vec3 b = aVtc[j][(i - 1 + segmentsW) % segmentsW];
				Vec3 c = aVtc[j - 1][(i - 1 + segmentsW) % segmentsW];
				Vec3 d = aVtc[j - 1][i];

				int i2;
				(i == 0 ? i2 = segmentsW : i2 = i);

				f32 vab = (f32)j / segmentsH;
				f32 vcd = (f32)(j - 1) / segmentsH;
				f32 uad = (f32)i2 / (f32)segmentsW;
				f32 ubc = (f32)(i2 - 1) / (f32)segmentsW;

				Vec2 aUV = Vec2(uad, -vab);
				Vec2 bUV = Vec2(ubc, -vab);
				Vec2 cUV = Vec2(ubc, -vcd);
				Vec2 dUV = Vec2(uad, -vcd);

				drawLine(c, b, color);
				drawLine(a, b, color);
				drawLine(a, d, color);
				drawLine(d, c, color);

			}
		}
	}

	void DebugRenderer::drawCone(const f32 radius, const f32 height, const Vec4 &color)
	{
		uint32 segmentsW = 6;
		uint32 segmentsH = 6;

		int jMin;

		f32 _height = height / 2.f;

		std::vector <std::vector <Vec3> >aVtc;

		jMin = 1;
		segmentsH += 1;
		Vec3 bottom = Vec3(0, -_height * 2, 0);
		std::vector <Vec3> aRowB;
		for (size_t i = 0; i < segmentsW; ++i) {
			aRowB.push_back(bottom);
		}
		aVtc.push_back(aRowB);


		for (size_t j = jMin; j < segmentsH; ++j) {
			f32 z = -height + 2 * height * (f32)(j - jMin) / (f32)(segmentsH - jMin);

			std::vector <Vec3> aRow;
			for (size_t i = 0; i < segmentsW; ++i) {
				f32 verangle = (f32)(2 * (f32)i / (f32)segmentsW*PI);
				f32 ringradius = radius * (f32)(segmentsH - j) / (f32)(segmentsH - jMin);
				f32 x = ringradius * sin(verangle);
				f32 y = ringradius * cos(verangle);

				Vec3 oVtx = Vec3(y, z, x);

				aRow.push_back(oVtx);
			}
			aVtc.push_back(aRow);
		}

		Vec3 top = Vec3(0, height, 0);
		std::vector <Vec3> aRowT;

		for (size_t i = 0; i < segmentsW; ++i)
			aRowT.push_back(top);

		aVtc.push_back(aRowT);

		for (size_t j = 1; j <= segmentsH; ++j) {
			for (size_t i = 0; i < segmentsW; ++i) {
				Vec3 a = aVtc[j][i];
				Vec3 b = aVtc[j][(i - 1 + segmentsW) % segmentsW];
				Vec3 c = aVtc[j - 1][(i - 1 + segmentsW) % segmentsW];
				Vec3 d = aVtc[j - 1][i];

				int i2 = i;
				if (i == 0) i2 = segmentsW;

				f32 vab = (f32)j / (f32)segmentsH;
				f32 vcd = (f32)(j - 1) / (f32)segmentsH;
				f32 uad = (f32)i2 / (f32)segmentsW;
				f32 ubc = (f32)(i2 - 1) / (f32)segmentsW;

				Vec2 aUV = Vec2(uad, -vab);
				Vec2 bUV = Vec2(ubc, -vab);
				Vec2 cUV = Vec2(ubc, -vcd);
				Vec2 dUV = Vec2(uad, -vcd);

				drawLine(c, b, color);
				drawLine(b, a, color);

				drawLine(a, d, color);
				drawLine(d, c, color);
			}
		}
		segmentsH -= 1;
	}

	void DebugRenderer::drawTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec4 &color)
	{
		colorTriangles.push_back(color);
		colorTriangles.push_back(color);
		colorTriangles.push_back(color);

		vertexTriangles.push_back(temp * a);
		vertexTriangles.push_back(temp * b);
		vertexTriangles.push_back(temp * c);
	}

	void DebugRenderer::pushMatrix(const Matrix &m)
	{
		temp = m;
		pushedMatrix = true;
	}

	void DebugRenderer::popMatrix()
	{
		temp.identity();
		pushedMatrix = false;
	}

};
