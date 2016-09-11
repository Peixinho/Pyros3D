#include <Pyros3D/VR/VR_Distortion_Geometry.h>
#include <Pyros3D/VR/VR_Shaders.h>

namespace p3d {

	VR_Distortion_Geometry::VR_Distortion_Geometry(vr::IVRSystem* m_pHMD)
	{
		geometry = new Distortion_Geometry();
		shader = new Shader();
		shader->LoadShaderText(VR_SHADER_DISTORTION);
		shader->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n")));
		shader->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n")));
		shader->LinkProgram();
		material = new VR_Distortion_Material(shader);

		int m_iLensGridSegmentCountH = 43;
		int m_iLensGridSegmentCountV = 43;

		float w = (float)(1.0 / float(m_iLensGridSegmentCountH - 1));
		float h = (float)(1.0 / float(m_iLensGridSegmentCountV - 1));

		float u, v = 0;

		//left eye distortion verts
		float Xoffset = -1;
		for (int y = 0; y < m_iLensGridSegmentCountV; y++)
		{
			for (int x = 0; x < m_iLensGridSegmentCountH; x++)
			{
				u = x*w; v = 1 - y*h;
				geometry->tVertex.push_back(Vec2(Xoffset + u, -1 + 2 * y*h));

				vr::DistortionCoordinates_t dc0 = m_pHMD->ComputeDistortion(vr::Eye_Left, u, v);

				geometry->tTexcoordRed.push_back(Vec2(dc0.rfRed[0], 1 - dc0.rfRed[1]));
				geometry->tTexcoordGreen.push_back(Vec2(dc0.rfGreen[0], 1 - dc0.rfGreen[1]));
				geometry->tTexcoordBlue.push_back(Vec2(dc0.rfBlue[0], 1 - dc0.rfBlue[1]));
			}
		}

		//right eye distortion verts
		Xoffset = 0;
		for (int y = 0; y < m_iLensGridSegmentCountV; y++)
		{
			for (int x = 0; x < m_iLensGridSegmentCountH; x++)
			{
				u = x*w; v = 1 - y*h;
				geometry->tVertex.push_back(Vec2(Xoffset + u, -1 + 2 * y*h));

				vr::DistortionCoordinates_t dc0 = m_pHMD->ComputeDistortion(vr::Eye_Right, u, v);

				geometry->tTexcoordRed.push_back(Vec2(dc0.rfRed[0], 1 - dc0.rfRed[1]));
				geometry->tTexcoordGreen.push_back(Vec2(dc0.rfGreen[0], 1 - dc0.rfGreen[1]));
				geometry->tTexcoordBlue.push_back(Vec2(dc0.rfBlue[0], 1 - dc0.rfBlue[1]));
			}
		}

		std::vector<int> vIndices;
		int a, b, c, d;

		int offset = 0;
		for (int y = 0; y < m_iLensGridSegmentCountV - 1; y++)
		{
			for (int x = 0; x < m_iLensGridSegmentCountH - 1; x++)
			{
				a = m_iLensGridSegmentCountH*y + x + offset;
				b = m_iLensGridSegmentCountH*y + x + 1 + offset;
				c = (y + 1)*m_iLensGridSegmentCountH + x + 1 + offset;
				d = (y + 1)*m_iLensGridSegmentCountH + x + offset;
				geometry->index.push_back(a);
				geometry->index.push_back(b);
				geometry->index.push_back(c);

				geometry->index.push_back(a);
				geometry->index.push_back(c);
				geometry->index.push_back(d);
			}
		}

		offset = (m_iLensGridSegmentCountH)*(m_iLensGridSegmentCountV);
		for (int y = 0; y < m_iLensGridSegmentCountV - 1; y++)
		{
			for (int x = 0; x < m_iLensGridSegmentCountH - 1; x++)
			{
				a = m_iLensGridSegmentCountH*y + x + offset;
				b = m_iLensGridSegmentCountH*y + x + 1 + offset;
				c = (y + 1)*m_iLensGridSegmentCountH + x + 1 + offset;
				d = (y + 1)*m_iLensGridSegmentCountH + x + offset;
				geometry->index.push_back(a);
				geometry->index.push_back(b);
				geometry->index.push_back(c);

				geometry->index.push_back(a);
				geometry->index.push_back(c);
				geometry->index.push_back(d);
			}
		}
	}

	VR_Distortion_Geometry::~VR_Distortion_Geometry()
	{
		delete material;
		delete geometry;
		delete shader;
	}

};