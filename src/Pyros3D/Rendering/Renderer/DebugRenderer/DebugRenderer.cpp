#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

#include <iostream>

namespace p3d {

	DebugRenderer::DebugRenderer()
	{
		DebugMaterial = new GenericShaderMaterial(ShaderUsage::DebugRendering);
	}

	DebugRenderer::~DebugRenderer()
	{
		delete DebugMaterial;
	}

	void DebugRenderer::ClearBuffers()
	{
		// clean values
		vertexLines.clear();
		colorLines.clear();
		vertexTriangles.clear();
		colorTriangles.clear();
		vertexQuadStrip.clear();
		colorQuadStrip.clear();
	}

	void DebugRenderer::Render(const Matrix &camera, const Matrix &projection)
	{
		projectionMatrix = projection;
		viewMatrix = camera;

		GLCHECKER(glUseProgram(DebugMaterial->GetShader()));

		uint32 vertexHandle = Shader::GetAttributeLocation(DebugMaterial->GetShader(), "aPosition");
		uint32 colorHandle = Shader::GetAttributeLocation(DebugMaterial->GetShader(), "aColor");

		uint32 projectionHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uProjectionMatrix");
		uint32 viewMatrixHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uViewMatrix");
		uint32 modelMatrixHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uModelMatrix");
		uint32 opacityHandle = Shader::GetUniformLocation(DebugMaterial->GetShader(), "uOpacity");

		Shader::SendUniform(Uniform("uProjectionMatrix", DataType::Matrix, &projectionMatrix.m[0]), projectionHandle);
		Shader::SendUniform(Uniform("uViewMatrix", DataType::Matrix, &viewMatrix.m[0]), viewMatrixHandle);
		Shader::SendUniform(Uniform("uModelMatrix", DataType::Matrix, &modelMatrix.m[0]), modelMatrixHandle);
		f32 opacity = 1.f;
		Shader::SendUniform(Uniform("uOpacity", DataType::Float, &opacity), opacityHandle);

		// Send Attributes
		GLCHECKER(glEnableVertexAttribArray(vertexHandle));
		GLCHECKER(glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertexLines[0]));

		GLCHECKER(glEnableVertexAttribArray(colorHandle));
		GLCHECKER(glVertexAttribPointer(colorHandle, 4, GL_FLOAT, GL_FALSE, 0, &colorLines[0]));

		// Draw Quad
		GLCHECKER(glDrawArrays(GL_LINES, 0, vertexLines.size()));

		// Disable Attributes
		GLCHECKER(glDisableVertexAttribArray(colorHandle));
		GLCHECKER(glDisableVertexAttribArray(vertexHandle));

		// Send Attributes
		GLCHECKER(glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertexTriangles[0]));
		GLCHECKER(glVertexAttribPointer(colorHandle, 4, GL_FLOAT, GL_FALSE, 0, &colorTriangles[0]));

		// Draw Quad
		GLCHECKER(glDrawArrays(GL_TRIANGLES, 0, vertexTriangles.size()));

		// Disable Attributes
		GLCHECKER(glDisableVertexAttribArray(colorHandle));
		GLCHECKER(glDisableVertexAttribArray(vertexHandle));

#if !defined(GLES2)
		// Send Attributes
		GLCHECKER(glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertexQuadStrip[0]));
		GLCHECKER(glVertexAttribPointer(colorHandle, 4, GL_FLOAT, GL_FALSE, 0, &colorQuadStrip[0]));

		// Draw Quad
		GLCHECKER(glDrawArrays(GL_QUAD_STRIP, 0, vertexQuadStrip.size()));

		// Disable Attributes
		GLCHECKER(glDisableVertexAttribArray(colorHandle));
		GLCHECKER(glDisableVertexAttribArray(vertexHandle));

#endif

		GLCHECKER(glUseProgram(0));

		// clean values
		vertexLines.clear();
		colorLines.clear();
		vertexTriangles.clear();
		colorTriangles.clear();
		vertexQuadStrip.clear();
		colorQuadStrip.clear();
	}

	void DebugRenderer::drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &fromColor, const Vec4 &toColor)
	{
		vertexLines.push_back(from);
		vertexLines.push_back(to);
		colorLines.push_back(fromColor);
		colorLines.push_back(toColor);
	}

	void DebugRenderer::drawLine(const Vec3 &from, const Vec3 &to, const Vec4 &color)
	{
		drawLine(from, to, color, color);
	}

	void DebugRenderer::drawSphere(const Vec3 &p, f32 radius, const Vec4 &color)
	{

#if !defined(GLES2)
		Vec3 pos = p;

		int lats = 5;
		int longs = 5;

		int i, j;
		for (i = 0; i <= lats; i++)
		{
			f32 lat0 = 3.14f * (-f32(0.5) + (f32)(i - 1) / lats);
			f32 z0 = radius*sin(lat0);
			f32 zr0 = radius*cos(lat0);

			f32 lat1 = 3.14f * (-f32(0.5) + (f32)i / lats);
			f32 z1 = radius*sin(lat1);
			f32 zr1 = radius*cos(lat1);

			GLCHECKER(glBegin(GL_QUAD_STRIP));
			for (j = 0; j <= longs; j++)
			{
				f32 lng = 2 * 3.14f * (f32)(j - 1) / longs;
				f32 x = cos(lng);
				f32 y = sin(lng);

				vertexQuadStrip.push_back(pos + Vec3(x * zr0, y * zr0, z0));
				vertexQuadStrip.push_back(pos + Vec3(x * zr1, y * zr1, z1));
				colorQuadStrip.push_back(color);
				colorQuadStrip.push_back(color);

				//                glNormal3f(x * zr0, y * zr0, z0);
				//                glNormal3f(x * zr1, y * zr1, z1);
			}
		}
#endif
	}

	void DebugRenderer::drawTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec4 &color)
	{
		//	if (m_debugMode > 0)
		{
			const Vec3 n = (b - a).cross(c - a).normalize();

			colorTriangles.push_back(color);
			colorTriangles.push_back(color);
			colorTriangles.push_back(color);

			//            glNormal3d(n.getX(),n.getY(),n.getZ());
			vertexTriangles.push_back(a);
			vertexTriangles.push_back(b);
			vertexTriangles.push_back(c);
		}
	}

};