#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>
#include <iostream>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
namespace p3d {

	DebugRenderer::DebugRenderer()
	{
		DebugMaterial = new GenericShaderMaterial(ShaderUsage::DebugRendering);
		DebugMaterial->EnableDepthTest(DepthTest::Equal);
		pushedMatrix = false;
		temp = Matrix();

		VertexLinesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		ColorLinesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		VertexTrianglesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		ColorTrianglesBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		PointsBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		ColorPointsBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		PointsSizeBF = new GeometryBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
	}

	DebugRenderer::~DebugRenderer()
	{
		delete DebugMaterial;
		delete VertexLinesBF;
		delete ColorLinesBF;
		delete VertexTrianglesBF;
		delete ColorTrianglesBF;
		delete PointsBF;
		delete ColorPointsBF;
		delete PointsSizeBF;
	}

	void DebugRenderer::ClearScreen()
	{
		GLCHECKER(glClear(GL_COLOR_BUFFER_BIT));
	}
	void DebugRenderer::SetViewPort(const int32 &w0, const int32 h0, const int32 w1, const int32 h1)
	{
		glViewport(w0, h0, w1, h1);
	}

	void DebugRenderer::ClearBuffers()
	{
		// clean values
		vertexLines.clear();
		colorLines.clear();
		vertexTriangles.clear();
		colorTriangles.clear();
	}

	void DebugRenderer::Render(const Matrix &camera, const Matrix &projection)
	{
		#ifndef GLES2
			GLuint vao=0;
			glGenVertexArrays(1,&vao);
			glBindVertexArray(vao);
		#endif


		// Updating Buffers
		if (VertexLinesBF->ID>=0)
			VertexLinesBF->Update(&vertexLines[0], vertexLines.size() * sizeof(Vec3));
		else
		   	VertexLinesBF->Init(&vertexLines[0], vertexLines.size() * sizeof(Vec3));

		if (ColorLinesBF->ID>=0)
		   	ColorLinesBF->Update(&colorLines[0], colorLines.size() * sizeof(Vec4));
		else
		   	ColorLinesBF->Init(&colorLines[0], colorLines.size() * sizeof(Vec4));

		if (VertexTrianglesBF->ID>=0)
			VertexTrianglesBF->Update(&vertexTriangles[0], vertexTriangles.size() * sizeof(Vec3));
		else
			VertexTrianglesBF->Init(&vertexTriangles[0], vertexTriangles.size() * sizeof(Vec3));

		if (ColorTrianglesBF->ID>=0)
			ColorTrianglesBF->Update(&colorTriangles[0], colorTriangles.size() * sizeof(Vec4));
		else
			ColorTrianglesBF->Init(&colorTriangles[0], colorTriangles.size() * sizeof(Vec4));

		if (PointsBF->ID>=0)
			PointsBF->Update(&points[0], points.size() * sizeof(Vec3));
		else
			PointsBF->Init(&points[0], points.size() * sizeof(Vec3));

		if (ColorPointsBF->ID>=0)
			ColorPointsBF->Update(&colorPoints[0], colorPoints.size() * sizeof(Vec4));
		else
			ColorPointsBF->Init(&colorPoints[0], colorPoints.size() * sizeof(Vec4));

		if (PointsSizeBF->ID>=0)
			PointsSizeBF->Update(&pointsSize[0], pointsSize.size());
		else
			PointsSizeBF->Init(&pointsSize[0], pointsSize.size());


		projectionMatrix = projection;
		viewMatrix = camera;

		GLCHECKER(glEnable(GL_DEPTH_TEST));
		GLCHECKER(glDepthFunc(GL_LEQUAL));

		GLCHECKER(glUseProgram(DebugMaterial->GetShader()));

		int32 vertexHandle = Shader::GetAttributeLocation(DebugMaterial->GetShader(), "aPosition");
		int32 colorHandle = Shader::GetAttributeLocation(DebugMaterial->GetShader(), "aColor");
		int32 pointSizeHandle = Shader::GetAttributeLocation(DebugMaterial->GetShader(), "aSize");

		int32 projectionHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uProjectionMatrix");
		int32 viewMatrixHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uViewMatrix");
		int32 modelMatrixHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uModelMatrix");
		int32 opacityHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uOpacity");

		Shader::SendUniform(Uniform("uProjectionMatrix", Uniforms::DataType::Matrix, &projectionMatrix.m[0]), projectionHandle);
		Shader::SendUniform(Uniform("uViewMatrix", Uniforms::DataType::Matrix, &viewMatrix.m[0]), viewMatrixHandle);
		Shader::SendUniform(Uniform("uModelMatrix", Uniforms::DataType::Matrix, &modelMatrix.m[0]), modelMatrixHandle);

		f32 opacity = 1.f;
		Shader::SendUniform(Uniform("uOpacity", Uniforms::DataType::Float, &opacity), opacityHandle);

		if (vertexLines.size()>=0)
		{
			// Send Attributes
			if (vertexLines.size() > 0 && vertexHandle>=0) {
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, VertexLinesBF->ID));
				GLCHECKER(glEnableVertexAttribArray(vertexHandle))
				GLCHECKER(glVertexAttribPointer(
					vertexHandle,
					3,
					GL_FLOAT,
					GL_FALSE,
					sizeof(Vec3),
					BUFFER_OFFSET(0))
				);
			}

			if (colorLines.size() > 0 && colorHandle>=0) {
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, ColorLinesBF->ID));
				GLCHECKER(glEnableVertexAttribArray(colorHandle));
				GLCHECKER(glVertexAttribPointer(
					colorHandle,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(Vec4),
					BUFFER_OFFSET(0))
				);
			}

			// Draw Quad
			if (vertexLines.size() > 0)
				GLCHECKER(glDrawArrays(GL_LINES, 0, vertexLines.size()));

			// Disable Attributes
			if (colorLines.size() > 0 && colorHandle>=0)
				GLCHECKER(glDisableVertexAttribArray(colorHandle));
			if (vertexLines.size() > 0 && vertexHandle>=0)
				GLCHECKER(glDisableVertexAttribArray(vertexHandle));

				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, 0));
		}

		if (vertexTriangles.size()>=0)
		{

			// Send Attributes
			if (vertexTriangles.size() > 0 && vertexHandle>=0)
			{
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, VertexTrianglesBF->ID));
				GLCHECKER(glEnableVertexAttribArray(vertexHandle));
				GLCHECKER(glVertexAttribPointer(
					vertexHandle,
					3,
					GL_FLOAT,
					GL_FALSE,
					sizeof(Vec3),
					BUFFER_OFFSET(0))
				);
			}
			if (colorTriangles.size() > 0 && colorHandle>=0)
			{
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, ColorTrianglesBF->ID));
				GLCHECKER(glEnableVertexAttribArray(colorHandle));
				GLCHECKER(glVertexAttribPointer(
					colorHandle,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(Vec4),
					BUFFER_OFFSET(0))
				);
			}

			// Draw Quad
			if (vertexTriangles.size() > 0)
				GLCHECKER(glDrawArrays(GL_TRIANGLES, 0, vertexTriangles.size()));

			// Disable Attributes
			if (colorTriangles.size() > 0 && colorHandle>=0)
				GLCHECKER(glDisableVertexAttribArray(colorHandle));

			if (vertexTriangles.size() > 0 && vertexHandle>=0)
				GLCHECKER(glDisableVertexAttribArray(vertexHandle));

		}


		GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, 0));

		// Draw Points
#if !defined(GLES2) && !defined(GLES3)
		if (points.size()>=0)
		{

			// Send Attributes
			if (points.size() > 0 && vertexHandle>=0) {
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, PointsBF->ID));
				GLCHECKER(glEnableVertexAttribArray(vertexHandle));
				GLCHECKER(glVertexAttribPointer(
					vertexHandle,
					3,
					GL_FLOAT,
					GL_FALSE,
					sizeof(Vec3),
					BUFFER_OFFSET(0))
				);
			}

			if (colorPoints.size() > 0 && colorHandle>=0) {
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, ColorPointsBF->ID));
				GLCHECKER(glEnableVertexAttribArray(colorHandle));
				GLCHECKER(glVertexAttribPointer(
					colorHandle,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(Vec4),
					BUFFER_OFFSET(0))
				);
			}

			if (pointsSize.size() > 0) {
				GLCHECKER(glEnableVertexAttribArray(pointSizeHandle));
				GLCHECKER(glVertexAttribPointer(
					vertexHandle,
					1,
					GL_FLOAT,
					GL_FALSE,
					sizeof(float),
					BUFFER_OFFSET(0))
				);
			}

			if (points.size() > 0) {
				/*
				GLCHECKER(glEnable(GL_POINT_SPRITE));
				GLCHECKER(glEnable(GL_VERTEX_PROGRAM_POINT_SIZE));
				GLCHECKER(glDrawArrays(GL_POINTS, 0, points.size()));
				GLCHECKER(glDisable(GL_VERTEX_PROGRAM_POINT_SIZE));
				GLCHECKER(glDisable(GL_POINT_SPRITE));
				*/
			}

			// Disable Attributes
			if (pointsSize.size() > 0 && pointSizeHandle>=0)
				GLCHECKER(glDisableVertexAttribArray(pointSizeHandle));
			if (colorPoints.size() > 0 && colorHandle>=0)
				GLCHECKER(glDisableVertexAttribArray(colorHandle));
			if (points.size() > 0 && vertexHandle>=0)
				GLCHECKER(glDisableVertexAttribArray(vertexHandle));

				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, 0));
		}

#endif

		GLCHECKER(glUseProgram(0));

		// clean values
		vertexLines.clear();
		colorLines.clear();
		vertexTriangles.clear();
		colorTriangles.clear();
		points.clear();
		colorPoints.clear();
		pointsSize.clear();

		GLCHECKER(glDepthFunc(GL_LESS));
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
