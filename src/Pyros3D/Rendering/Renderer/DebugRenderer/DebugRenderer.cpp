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
	}

	void DebugRenderer::Render(const Matrix &camera, const Matrix &projection)
	{
		GLCHECKER(glEnable(GL_DEPTH_TEST));
		GLCHECKER(glDepthFunc(GL_LEQUAL));

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

		GLCHECKER(glUseProgram(0));

		// clean values
		vertexLines.clear();
		colorLines.clear();
		vertexTriangles.clear();
		colorTriangles.clear();

		GLCHECKER(glDisable(GL_DEPTH_TEST));
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
		Vec3 pos = p;

		uint32 latitudeBands = 8;
		uint32 longitudeBands = 8;
		std::vector<Vec3> v;
		for (uint32 latNumber = 0; latNumber <= latitudeBands; latNumber++) {
			f32 theta = latNumber * PI / latitudeBands;
			f32 sinTheta = sinf(theta);
			f32 cosTheta = cosf(theta);
			for (uint32 longNumber = 0; longNumber <= longitudeBands; longNumber++) {
				f32 phi = longNumber * 2 * PI / longitudeBands;
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
				drawLine(v[first+1], v[second], color);
				drawLine(v[first+1], v[second+1], color);
			}
		}
	}

	void DebugRenderer::drawTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec4 &color)
	{
		const Vec3 n = (b - a).cross(c - a).normalize();

		colorTriangles.push_back(color);
		colorTriangles.push_back(color);
		colorTriangles.push_back(color);

		vertexTriangles.push_back(a);
		vertexTriangles.push_back(b);
		vertexTriangles.push_back(c);
	}

};