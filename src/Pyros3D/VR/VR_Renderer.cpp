//============================================================================
// Name        : VR_Renderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Renderer
//============================================================================

#include <Pyros3D/VR/VR_Renderer.h>
#include <Pyros3D/Other/PyrosGL.h>
namespace p3d {

	VR_Renderer::VR_Renderer()
	{

	}

	VR_Renderer::~VR_Renderer()
	{

	}

	bool VR_Renderer::Init()
	{
		// Loading the SteamVR Runtime
		vr::EVRInitError eError = vr::VRInitError_None;
		m_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);

		if (eError != vr::VRInitError_None)
		{
			m_pHMD = NULL;
			char buf[1024];
			sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
			echo("VR_Init Failed: " + std::string(buf));

			return false;
		}

		// Render Models
		m_pRenderModels = (vr::IVRRenderModels *)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
		if (!m_pRenderModels)
		{
			m_pHMD = NULL;
			vr::VR_Shutdown();

			char buf[1024];
			sprintf_s(buf, sizeof(buf), "Unable to get render model interface: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
			echo("VR_Init Failed: " + std::string(buf));

			return false;
		}

		m_strDriver = "No Driver";
		m_strDisplay = "No Display";

		m_strDriver = GetTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
		m_strDisplay = GetTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

		m_mat4ProjectionLeft = GetHMDMatrixProjectionEye(vr::Eye_Left);
		m_mat4ProjectionRight = GetHMDMatrixProjectionEye(vr::Eye_Right);
		m_mat4eyePosLeft = GetHMDMatrixPoseEye(vr::Eye_Left);
		m_mat4eyePosRight = GetHMDMatrixPoseEye(vr::Eye_Right);

		m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);

		// Set Textures and Framebuffers
		leftEye = new Texture();
		leftEye->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, false);
		leftEyeResolve = new Texture();
		leftEyeResolve->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, false);

		leftEyeFBO = new FrameBuffer();
		leftEyeFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, m_nRenderWidth, m_nRenderHeight);
		leftEyeFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, leftEye);
		leftEyeFBOResolve = new FrameBuffer();
		leftEyeFBOResolve->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, leftEyeResolve);

		rightEye = new Texture();
		rightEye->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, false);
		rightEyeResolve = new Texture();
		rightEyeResolve->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, false);

		rightEyeFBO= new FrameBuffer();
		rightEyeFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, m_nRenderWidth, m_nRenderHeight);
		rightEyeFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, rightEye);
		rightEyeFBOResolve = new FrameBuffer();
		rightEyeFBOResolve->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, rightEyeResolve);

		// Distortion
		//DistortionLens = new VR_Distortion_Geometry(m_pHMD);

		distortionPositionHandle= -2;
		distortionRedInHandle = -2;
		distortionGreenInHandle = -2;
		distortionBlueInHandle = -2;

		// Render Models
		/*memset(m_rTrackedDeviceToRenderModel, 0, sizeof(m_rTrackedDeviceToRenderModel));

		for (uint32_t unTrackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1; unTrackedDevice < vr::k_unMaxTrackedDeviceCount; unTrackedDevice++)
		{
		if (!m_pHMD->IsTrackedDeviceConnected(unTrackedDevice))
		continue;

		SetupRenderModelForTrackedDevice(unTrackedDevice);
		}*/

		vr::EVRInitError peError = vr::VRInitError_None;

		if (!vr::VRCompositor())
		{
			echo("Compositor initialization failed. See log file for details\n");
			//return false;
		}

		fwdRenderer = new ForwardRenderer(m_nRenderWidth, m_nRenderHeight);

		return true;
	}

	void VR_Renderer::SetupRenderModelForTrackedDevice(vr::TrackedDeviceIndex_t unTrackedDeviceIndex)
	{
		/*if (unTrackedDeviceIndex >= vr::k_unMaxTrackedDeviceCount)
			return;

		// try to find a model we've already set up
		std::string sRenderModelName = GetTrackedDeviceString(m_pHMD, unTrackedDeviceIndex, vr::Prop_RenderModelName_String);
		CGLRenderModel *pRenderModel = FindOrLoadRenderModel(sRenderModelName.c_str());
		if (!pRenderModel)
		{
			std::string sTrackingSystemName = GetTrackedDeviceString(m_pHMD, unTrackedDeviceIndex, vr::Prop_TrackingSystemName_String);
			echo(std::string(std::string("Unable to load render model for tracked device ") + " " + sTrackingSystemName + " " + sRenderModelName));
		}
		else
		{
			m_rTrackedDeviceToRenderModel[unTrackedDeviceIndex] = pRenderModel;
			m_rbShowTrackedDevice[unTrackedDeviceIndex] = true;
		}*/
	}

	Matrix VR_Renderer::GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
	{
		if (!m_pHMD)
			return Matrix();

		vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(nEye, m_nearClip, m_farClip, vr::API_OpenGL);

		return Matrix(
			mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
			mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
			mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
			mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
			);
	}

	Matrix VR_Renderer::GetHMDMatrixPoseEye(vr::Hmd_Eye nEye)
	{
		if (!m_pHMD)
			return Matrix();

		vr::HmdMatrix34_t matEyeRight = m_pHMD->GetEyeToHeadTransform(nEye);
		Matrix matrixObj(
			matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0,
			matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
			matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
			matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f
			);

		return matrixObj.Inverse();
	}

	Matrix VR_Renderer::ConvertSteamVRMatrixToMatrix(const vr::HmdMatrix34_t &matPose)
	{
		Matrix matrixObj(
			matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
			matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
			matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
			matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
			);
		return matrixObj;
	}

	std::string VR_Renderer::GetTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError)
	{
		uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
		if (unRequiredBufferLen == 0)
			return "";

		char *pchBuffer = new char[unRequiredBufferLen];
		unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
		std::string sResult = pchBuffer;
		delete[] pchBuffer;
		return sResult;
	}

	Matrix VR_Renderer::GetCurrentViewProjectionMatrix(vr::Hmd_Eye nEye)
	{
		Matrix matMVP;
		if (nEye == vr::Eye_Left)
		{
			matMVP = m_mat4ProjectionLeft * m_mat4eyePosLeft * m_mat4HMDPose;
		}
		else if (nEye == vr::Eye_Right)
		{
			matMVP = m_mat4ProjectionRight * m_mat4eyePosRight *  m_mat4HMDPose;
		}

		return matMVP;
	}

	Matrix VR_Renderer::GetCurrentViewMatrix(vr::Hmd_Eye nEye)
	{
		Matrix matMVP;
		if (nEye == vr::Eye_Left)
		{
			matMVP = m_mat4eyePosLeft * m_mat4HMDPose;
		}
		else if (nEye == vr::Eye_Right)
		{
			matMVP = m_mat4eyePosRight *  m_mat4HMDPose;
		}

		return matMVP;
	}

	Matrix VR_Renderer::GetCurrentProjectionMatrix(vr::Hmd_Eye nEye)
	{
		Matrix matMVP;
		if (nEye == vr::Eye_Left)
		{
			matMVP = m_mat4ProjectionLeft;
		}
		else if (nEye == vr::Eye_Right)
		{
			matMVP = m_mat4ProjectionRight;
		}

		return matMVP;
	}

	void VR_Renderer::UpdateHMDMatrixPose()
	{
		vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

		int m_iValidPoseCount = 0;
		std::string m_strPoseClasses = "";
		for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
		{
			if (m_rTrackedDevicePose[nDevice].bPoseIsValid)
			{
				m_iValidPoseCount++;
				m_rmat4DevicePose[nDevice] = ConvertSteamVRMatrixToMatrix(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
				if (m_rDevClassChar[nDevice] == 0)
				{
					switch (m_pHMD->GetTrackedDeviceClass(nDevice))
					{
					case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
					case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
					case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
					case vr::TrackedDeviceClass_Other:             m_rDevClassChar[nDevice] = 'O'; break;
					case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
					default:                                       m_rDevClassChar[nDevice] = '?'; break;
					}
				}
				m_strPoseClasses += m_rDevClassChar[nDevice];
			}
		}

		if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
		{
			m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd].Inverse();
		}
	}

	void VR_Renderer::Renderer(SceneGraph* Scene)
	{

		Scene->Add(&go);

		if (m_pHMD)
		{
			//DrawControllers();
			{
				if (m_pHMD->IsInputFocusCapturedByAnotherProcess())
					return;

				std::vector<float> vertdataarray;

				int m_uiControllerVertcount = 0;
				int m_iTrackedControllerCount = 0;

				for (vr::TrackedDeviceIndex_t unTrackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1; unTrackedDevice < vr::k_unMaxTrackedDeviceCount; ++unTrackedDevice)
				{
					if (!m_pHMD->IsTrackedDeviceConnected(unTrackedDevice))
						continue;

					if (m_pHMD->GetTrackedDeviceClass(unTrackedDevice) != vr::TrackedDeviceClass_Controller)
						continue;

					m_iTrackedControllerCount += 1;

					if (!m_rTrackedDevicePose[unTrackedDevice].bPoseIsValid)
						continue;

					const Matrix & mat = m_rmat4DevicePose[unTrackedDevice];

					Vec4 center = mat * Vec4(0, 0, 0, 1);

					for (int i = 0; i < 3; ++i)
					{
						Vec3 color(0, 0, 0);
						Vec4 point(0, 0, 0, 1);
						point[i] += 0.05f;  // offset in X, Y, Z
						color[i] = 1.0;  // R, G, B
						point = mat * point;
						vertdataarray.push_back(center.x);
						vertdataarray.push_back(center.y);
						vertdataarray.push_back(center.z);

						vertdataarray.push_back(color.x);
						vertdataarray.push_back(color.y);
						vertdataarray.push_back(color.z);

						vertdataarray.push_back(point.x);
						vertdataarray.push_back(point.y);
						vertdataarray.push_back(point.z);

						vertdataarray.push_back(color.x);
						vertdataarray.push_back(color.y);
						vertdataarray.push_back(color.z);

						m_uiControllerVertcount += 2;
					}

					Vec4 start = mat * Vec4(0, 0, -0.02f, 1);
					Vec4 end = mat * Vec4(0, 0, -39.f, 1);
					Vec3 color(.92f, .92f, .71f);

					vertdataarray.push_back(start.x); vertdataarray.push_back(start.y); vertdataarray.push_back(start.z);
					vertdataarray.push_back(color.x); vertdataarray.push_back(color.y); vertdataarray.push_back(color.z);

					vertdataarray.push_back(end.x); vertdataarray.push_back(end.y); vertdataarray.push_back(end.z);
					vertdataarray.push_back(color.x); vertdataarray.push_back(color.y); vertdataarray.push_back(color.z);
					m_uiControllerVertcount += 2;
				}
			}

			// Setup the VAO the first time through.
			/*f (m_unControllerVAO == 0)
			{
				glGenVertexArrays(1, &m_unControllerVAO);
				glBindVertexArray(m_unControllerVAO);

				glGenBuffers(1, &m_glControllerVertBuffer);
				glBindBuffer(GL_ARRAY_BUFFER, m_glControllerVertBuffer);

				GLuint stride = 2 * 3 * sizeof(float);
				GLuint offset = 0;

				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset);

				offset += sizeof(Vec3);
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset);

				glBindVertexArray(0);
			}

			glBindBuffer(GL_ARRAY_BUFFER, m_glControllerVertBuffer);

			// set vertex data if we have some
			if (vertdataarray.size() > 0)
			{
				//$ TODO: Use glBufferSubData for this...
				glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertdataarray.size(), &vertdataarray[0], GL_STREAM_DRAW);
			}*/

			// RenderStereoTargets();
			{
				//glClearColor(0.15f, 0.15f, 0.18f, 1.0f); // nice background color, but not black
				glEnable(GL_MULTISAMPLE);

				// Left Eye
				leftEyeFBO->Bind();
				//glViewport(0, 0, m_nRenderWidth, m_nRenderHeight);
				
				Matrix viewL = GetCurrentViewMatrix(vr::Eye_Left).Inverse();
				go.SetTransformationMatrix(viewL);
				Projection projectionL;
				projectionL.m = GetCurrentProjectionMatrix(vr::Eye_Left);

				fwdRenderer->ResetViewPort();
				fwdRenderer->SetViewPort(0, 0, m_nRenderWidth, m_nRenderHeight);
				fwdRenderer->RenderScene(projectionL, &go, Scene);

				leftEyeFBO->UnBind();

				glDisable(GL_MULTISAMPLE);

				leftEyeFBO->Bind(FBOAccess::Read);
				leftEyeFBOResolve->Bind(FBOAccess::Write);

				glBlitFramebuffer(0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

				leftEyeFBO->UnBind();
				leftEyeFBOResolve->UnBind();

				glEnable(GL_MULTISAMPLE);

				// Right Eye
				rightEyeFBO->Bind();
				//glViewport(0, 0, m_nRenderWidth, m_nRenderHeight);
				
				Matrix viewR = GetCurrentViewMatrix(vr::Eye_Right).Inverse();
				go.SetTransformationMatrix(viewR);

				Projection projectionR;
				projectionR.m = GetCurrentProjectionMatrix(vr::Eye_Right);
				
				fwdRenderer->ResetViewPort();
				fwdRenderer->SetViewPort(0, 0, m_nRenderWidth, m_nRenderHeight);
				fwdRenderer->RenderScene(projectionR, &go, Scene);

				rightEyeFBO->UnBind();

				glDisable(GL_MULTISAMPLE);

				rightEyeFBO->Bind(FBOAccess::Read);
				rightEyeFBOResolve->Bind(FBOAccess::Write);
				glBlitFramebuffer(0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

				rightEyeFBO->UnBind();
				rightEyeFBOResolve->UnBind();
			}
			
			// RenderDistortion();
			{
				/*glDisable(GL_DEPTH_TEST);
				//glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);

				glUseProgram(DistortionLens->material->GetShader());

				if (distortionPositionHandle == -2)
				{
					distortionPositionHandle = Shader::GetAttributeLocation(DistortionLens->material->GetShader(), "position");
				}
				// Texcoord
				if (distortionRedInHandle == -2)
				{
					distortionRedInHandle = Shader::GetAttributeLocation(DistortionLens->material->GetShader(), "v2UVredIn");
				}
				if (distortionGreenInHandle == -2)
				{
					distortionGreenInHandle = Shader::GetAttributeLocation(DistortionLens->material->GetShader(), "v2UVgreenIn");
				}
				if (distortionBlueInHandle == -2)
				{
					distortionBlueInHandle = Shader::GetAttributeLocation(DistortionLens->material->GetShader(), "v2UVblueIn");
				}
				// Send Attributes
				if (distortionPositionHandle>-1)
				{
					glEnableVertexAttribArray(distortionPositionHandle);
					glVertexAttribPointer(distortionPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, &DistortionLens->geometry->tVertex[0]);
				}
				if (distortionRedInHandle>-1)
				{
					glEnableVertexAttribArray(distortionRedInHandle);
					glVertexAttribPointer(distortionRedInHandle, 2, GL_FLOAT, GL_FALSE, 0, &DistortionLens->geometry->tTexcoordRed[0]);
				}
				if (distortionGreenInHandle>-1)
				{
					glEnableVertexAttribArray(distortionGreenInHandle);
					glVertexAttribPointer(distortionGreenInHandle, 2, GL_FLOAT, GL_FALSE, 0, &DistortionLens->geometry->tTexcoordGreen[0]);
				}
				if (distortionRedInHandle>-1)
				{
					glEnableVertexAttribArray(distortionBlueInHandle);
					glVertexAttribPointer(distortionBlueInHandle, 2, GL_FLOAT, GL_FALSE, 0, &DistortionLens->geometry->tTexcoordBlue[0]);
				}

				int pos = Shader::GetUniformLocation(DistortionLens->material->GetShader(), "mytexture");
				GLint v = 1;
				glUniform1iv(pos, 1, &v);

				//render left lens (first half of index array )
				leftEyeResolve->Bind();
				glDrawElements(GL_TRIANGLES, DistortionLens->geometry->index.size() / 2, __INDEX_TYPE__, &DistortionLens->geometry->index[0]);
				
				//render right lens (second half of index array )
				rightEyeResolve->Bind();
				glDrawElements(GL_TRIANGLES, DistortionLens->geometry->index.size() / 2, __INDEX_TYPE__, &DistortionLens->geometry->index[DistortionLens->geometry->index.size() / 2]);

				// Disable Attributes
				if (distortionBlueInHandle>-1)
				{
					glDisableVertexAttribArray(distortionBlueInHandle);
				}
				if (distortionGreenInHandle>-1)
				{
					glDisableVertexAttribArray(distortionGreenInHandle);
				}
				if (distortionRedInHandle>-1)
				{
					glDisableVertexAttribArray(distortionRedInHandle);
				}
				if (distortionPositionHandle)
				{
					glDisableVertexAttribArray(distortionPositionHandle);
				}

				glUseProgram(0);*/

				Matrix viewL = GetCurrentViewMatrix(vr::Eye_Left).Inverse();
				go.SetTransformationMatrix(viewL);
				Projection projectionL;
				projectionL.m = GetCurrentProjectionMatrix(vr::Eye_Left);

				fwdRenderer->ResetViewPort();
				fwdRenderer->SetViewPort(0, 0, m_nRenderWidth, m_nRenderHeight);
				fwdRenderer->RenderScene(projectionL, &go, Scene);
			}
			
			vr::Texture_t leftEyeTexture = { (void*)leftEye->GetBindID(), vr::API_OpenGL, vr::ColorSpace_Gamma };
			vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
			vr::Texture_t rightEyeTexture = { (void*)rightEye->GetBindID(), vr::API_OpenGL, vr::ColorSpace_Gamma };
			vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);


			// Spew out the controller and pose count whenever they change.
			/*if (m_iTrackedControllerCount != m_iTrackedControllerCount_Last || m_iValidPoseCount != m_iValidPoseCount_Last)
			{
				m_iValidPoseCount_Last = m_iValidPoseCount;
				m_iTrackedControllerCount_Last = m_iTrackedControllerCount;

				echo("PoseCount:%d(%s) Controllers:%d\n", m_iValidPoseCount, m_strPoseClasses.c_str(), m_iTrackedControllerCount);
			}*/

			UpdateHMDMatrixPose();
		}
	}
}