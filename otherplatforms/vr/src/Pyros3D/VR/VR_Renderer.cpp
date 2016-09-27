//============================================================================
// Name        : VR_Renderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Renderer
//============================================================================

#include "VR_Renderer.h"
#include <Pyros3D/Other/PyrosGL.h>
namespace p3d {

	VR_Renderer::VR_Renderer(const f32 near, const f32 far, bool renderToScreen, const uint32 screenWidth, const uint32 screenHeight)
	{
		scene = NULL;
		m_screenWidth = screenWidth;
		m_screenHeight = screenHeight;
		this->renderToScreen = renderToScreen;
		m_nearClip = near;
		m_farClip = far;
		m_strDriver = "No Driver";
		m_strDisplay = "No Display";

		showControllers = showTrackers = false;
	}

	void VR_Renderer::Resize(const uint32 screenWidth, const uint32 screenHeight)
	{
		m_screenWidth = screenWidth;
		m_screenHeight = screenHeight;

		m_projection.Perspective(70, (f32)m_screenWidth / (f32)m_screenHeight, m_nearClip, m_farClip);
	}

	void VR_Renderer::RenderControllers(bool show)
	{
		for (std::vector<VR_GameObject*>::iterator i = Controllers.begin(); i != Controllers.end(); i++)
		{
			if ((*i)->showingModel != show)
			{
				if (show) (*i)->Add((*i)->rcomp);
				else (*i)->Remove((*i)->rcomp);
			}
			(*i)->showingModel = show;
		}
		showControllers = show;
	}

	void VR_Renderer::RenderTrackers(bool show)
	{
		for (std::vector<VR_GameObject*>::iterator i = TrackingDevices.begin(); i != TrackingDevices.end(); i++)
		{
			if ((*i)->showingModel != show)
			{
				if (show) (*i)->Add((*i)->rcomp);
				else (*i)->Remove((*i)->rcomp);
			}
			(*i)->showingModel = show;
		}
		showTrackers = show;
	}

	VR_Renderer::~VR_Renderer()
	{
		for (std::vector<VR_GameObject*>::iterator i = Controllers.begin(); i != Controllers.end(); i++)
		{
			if ((*i)->isOnScene)
				scene->Remove((*i));
			delete (*i);
		}
		for (std::vector<VR_GameObject*>::iterator i = TrackingDevices.begin(); i != TrackingDevices.end(); i++)
		{
			if ((*i)->isOnScene)
				scene->Remove((*i));
			delete (*i);
		}

		Controllers.clear();
		TrackingDevices.clear();

		delete fwdRenderer;
		delete cameraL;
		delete cameraR;
		delete leftEye;
		delete leftEyeFBO;
		delete leftEyeFBOResolve;
		delete leftEyeResolve;
		delete rightEye;
		delete rightEyeFBO;
		delete rightEyeFBOResolve;
		delete rightEyeResolve;

		if (m_pHMD)
		{
			vr::VR_Shutdown();
			m_pHMD = NULL;
		}
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

		m_strDriver = GetTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
		m_strDisplay = GetTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

		m_mat4ProjectionLeft = GetHMDMatrixProjectionEye(vr::Eye_Left);
		m_mat4ProjectionRight = GetHMDMatrixProjectionEye(vr::Eye_Right);
		m_mat4eyePosLeft = GetHMDMatrixPoseEye(vr::Eye_Left);
		m_mat4eyePosRight = GetHMDMatrixPoseEye(vr::Eye_Right);

		m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);

		// Set Textures and Framebuffers
		leftEye = new Texture();
		leftEye->CreateEmptyTexture(TextureType::Texture_Multisample, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, false, 0, 4);
		leftEyeResolve = new Texture();
		leftEyeResolve->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, true);
		leftEyeResolve->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
		leftEyeResolve->SetMinMagFilter(TextureFilter::Linear, TextureFilter::LinearMipmapLinear);

		leftEyeFBO = new FrameBuffer();
		leftEyeFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture_Multisample, leftEye);
		leftEyeFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth_Multisample, m_nRenderWidth, m_nRenderHeight, 4);

		leftEyeFBOResolve = new FrameBuffer();
		leftEyeFBOResolve->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, leftEyeResolve);

		rightEye = new Texture();
		rightEye->CreateEmptyTexture(TextureType::Texture_Multisample, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, false, 0, 4);
		rightEyeResolve = new Texture();
		rightEyeResolve->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, m_nRenderWidth, m_nRenderHeight, true);
		rightEyeResolve->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
		rightEyeResolve->SetMinMagFilter(TextureFilter::Linear, TextureFilter::LinearMipmapLinear);

		rightEyeFBO = new FrameBuffer();
		rightEyeFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture_Multisample, rightEye);
		rightEyeFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth_Multisample, m_nRenderWidth, m_nRenderHeight, 4);

		rightEyeFBOResolve = new FrameBuffer();
		rightEyeFBOResolve->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, rightEyeResolve);

		vr::EVRInitError peError = vr::VRInitError_None;

		if (!vr::VRCompositor())
		{
			echo("Compositor initialization failed. See log file for details\n");
			return false;
		}

		// Set Renderer
		fwdRenderer = new ForwardRenderer(m_nRenderWidth, m_nRenderHeight);

		// Set Eye Cameras
		cameraL = new VR_Camera();
		cameraR = new VR_Camera();

		// Get Projections
		projectionL.m = GetCurrentProjectionMatrix(vr::Eye_Left);
		projectionR.m = GetCurrentProjectionMatrix(vr::Eye_Right);
		projectionL.Far = projectionR.Far = m_farClip;
		projectionL.Near = projectionR.Near = m_nearClip;
		m_projection.Perspective(70, m_screenWidth / m_screenHeight, m_nearClip, m_farClip);

		// Check for VR Devices
		for (uint32_t unTrackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1; unTrackedDevice < vr::k_unMaxTrackedDeviceCount; unTrackedDevice++)
		{
			if (!m_pHMD->IsTrackedDeviceConnected(unTrackedDevice))
				continue;

			SetupDevice(unTrackedDevice);
		}

		return true;
	}

	int VR_Renderer::Alloc(std::vector<VR_GameObject*> array)
	{
		for (int i = 0; i < array.size(); ++i)
			if (!array[i])
				return i;
		array.resize(array.size() + 1);
		return array.size() - 1;
	}

	void VR_Renderer::Free(std::vector<VR_GameObject*> array, int index)
	{
		for (int i = index + 1; i < array.size(); ++i)
			if (array[i])
			{
				array[index] = NULL;
				return;
			}
		array.resize(index);
	}

	void VR_Renderer::SetupDevice(uint32 modelIndex)
	{
		VR_GameObject* go = new VR_GameObject();
		switch (m_pHMD->GetTrackedDeviceClass(modelIndex))
		{
		case vr::TrackedDeviceClass_Controller:
			go->type = vr::TrackedDeviceClass_Controller;

			Controllers[Alloc(Controllers)] = go;
			LoadModel(modelIndex, go);
			if (showControllers)
			{
				go->showingModel = true;
				go->Add(go->rcomp);
			}
			break;
		case vr::TrackedDeviceClass_TrackingReference:
			go->type = vr::TrackedDeviceClass_TrackingReference;
			TrackingDevices[Alloc(TrackingDevices)] = go;
			LoadModel(modelIndex, go);
			if (showTrackers)
			{
				go->showingModel = true;
				go->Add(go->rcomp);
			}
			break;
		default:

			break;
		}
	}

	void VR_Renderer::LoadModel(uint32 modelIndex, VR_GameObject* go)
	{
		VR_Model* model = SetupRenderModelForTrackedDevice(modelIndex);
		if (model != NULL)
		{
			RenderingComponent* rcomp = new RenderingComponent(model);
			go->rcomp = rcomp;
			go->model = model;
			go->texture = model->texture;
			go->index = modelIndex;

			echo("Device attached: " + modelIndex);
		}
	}

	void VR_Renderer::UnloadModel(uint32 modelIndex)
	{
		for (std::vector<VR_GameObject*>::iterator i = Controllers.begin(); i != Controllers.end(); i++)
		{
			if ((*i) != NULL && modelIndex == (*i)->index)
			{
				if (scene && (*i)->isOnScene)
					scene->Remove((*i));
				delete (*i);
				(*i) = NULL;
				return;
			}
		}

		for (std::vector<VR_GameObject*>::iterator i = TrackingDevices.begin(); i != TrackingDevices.end(); i++)
		{
			if ((*i) != NULL && modelIndex == (*i)->index)
			{
				if (scene && (*i)->isOnScene)
					scene->Remove((*i));
				delete (*i);
				(*i) = NULL;
				return;
			}
		}

		echo("Device detached: " + modelIndex);
	}

	void VR_Renderer::ProcessVREvent(const vr::VREvent_t & event)
	{
		switch (event.eventType)
		{
		case vr::VREvent_TrackedDeviceActivated:
		{
			SetupDevice(event.trackedDeviceIndex);
		}
		break;
		case vr::VREvent_TrackedDeviceDeactivated:
		{
			UnloadModel(event.trackedDeviceIndex);
		}
		break;
		case vr::VREvent_TrackedDeviceUpdated:
		{
			echo("Device updated: " + event.trackedDeviceIndex);
		}
		break;
		}
	}

	void VR_Renderer::HandleVRInputs() {
		// Process SteamVR events
		vr::VREvent_t event;
		if (m_pHMD != NULL)
			while (m_pHMD->PollNextEvent(&event, sizeof(event)))
			{
				ProcessVREvent(event);
			}
	}

	vr::VRControllerState_t VR_Renderer::GetControllerEvents(uint32 modelIndex)
	{
		vr::VRControllerState_t state;
		if (m_pHMD != NULL)
			m_pHMD->GetControllerState(modelIndex, &state);
		return state;
	}

	VR_Model* VR_Renderer::SetupRenderModelForTrackedDevice(vr::TrackedDeviceIndex_t unTrackedDeviceIndex)
	{
		if (unTrackedDeviceIndex >= vr::k_unMaxTrackedDeviceCount)
			return NULL;

		std::string sRenderModelName = GetTrackedDeviceString(m_pHMD, unTrackedDeviceIndex, vr::Prop_RenderModelName_String);

		vr::RenderModel_t *pModel;
		vr::EVRRenderModelError error;
		while (1)
		{
			error = vr::VRRenderModels()->LoadRenderModel_Async(sRenderModelName.c_str(), &pModel);
			if (error != vr::VRRenderModelError_Loading)
				break;
		}
		if (error != vr::VRRenderModelError_None)
		{
			echo("Unable to load render model " + sRenderModelName);
			return NULL;
		}
		vr::RenderModel_TextureMap_t *pTexture;
		while (1)
		{
			error = vr::VRRenderModels()->LoadTexture_Async(pModel->diffuseTextureId, &pTexture);
			if (error != vr::VRRenderModelError_Loading)
				break;
		}

		if (error != vr::VRRenderModelError_None)
		{
			echo("Unable to load render texture id: " + pModel->diffuseTextureId);
			vr::VRRenderModels()->FreeRenderModel(pModel);
			return NULL; // move on to the next tracked device
		}

		Texture *tex = new Texture();
		tex->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, pTexture->unWidth, pTexture->unHeight, false);
		tex->UpdateData((void*)pTexture->rubTextureMapData);
		VR_Model *model = new VR_Model(pModel, tex);

		vr::VRRenderModels()->FreeRenderModel(pModel);
		vr::VRRenderModels()->FreeTexture(pTexture);

		return model;
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

	void VR_Renderer::Renderer(SceneGraph* Scene, Projection *projection, GameObject* camera, SceneGraph* SceneToScreen)
	{

		scene = Scene;

		if (m_pHMD)
		{
			{
				if (m_pHMD->IsInputFocusCapturedByAnotherProcess())
					return;

				int m_uiControllerVertcount = 0;
				int m_iTrackedControllerCount = 0;

				for (vr::TrackedDeviceIndex_t unTrackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1; unTrackedDevice < vr::k_unMaxTrackedDeviceCount; ++unTrackedDevice)
				{
					if (!m_pHMD->IsTrackedDeviceConnected(unTrackedDevice))
						continue;

					//if (m_pHMD->GetTrackedDeviceClass(unTrackedDevice) != vr::TrackedDeviceClass_Controller)
						//continue;

					m_iTrackedControllerCount += 1;

					//if (!m_rTrackedDevicePose[unTrackedDevice].bPoseIsValid)
						//continue;

					const Matrix & mat = m_rmat4DevicePose[unTrackedDevice];

					Vec4 center = mat * Vec4(0, 0, 0, 1);

					if (m_pHMD->GetTrackedDeviceClass(unTrackedDevice) == vr::TrackedDeviceClass_Controller)
					{
						// Store HMD Transform for later usage
						Controllers[unTrackedDevice]->SetTransformationMatrix(mat);
						if (scene && !Controllers[unTrackedDevice]->isOnScene)
						{
							scene->Add(Controllers[unTrackedDevice]);
							Controllers[unTrackedDevice]->isOnScene = true;
						}
					}
					// Update only once
					else if (TrackingDevices[unTrackedDevice] != NULL && scene && !TrackingDevices[unTrackedDevice]->isOnScene)
					{
						TrackingDevices[unTrackedDevice]->SetTransformationMatrix(mat);
						scene->Add(TrackingDevices[unTrackedDevice]);
						TrackingDevices[unTrackedDevice]->isOnScene = true;
					}
				}
			}

			// RenderStereoTargets();
			{
				fwdRenderer->SetViewPort(0, 0, m_nRenderWidth, m_nRenderHeight);

				FrameBuffer::EnableMultisample();

				// Left Eye
				leftEyeFBO->Bind();

				Matrix viewL = GetCurrentViewMatrix(vr::Eye_Left).Inverse();

				cameraL->SetTransformationMatrix(viewL);
				cameraL->Update();

				// Render Shadows once each frame
				fwdRenderer->PreRender(cameraR, Scene);
				fwdRenderer->RenderScene(projectionL, cameraL, Scene);

				leftEyeFBO->UnBind();

				FrameBuffer::DisableMultisample();

				leftEyeFBO->Bind(FBOAccess::Read);
				leftEyeFBOResolve->Bind(FBOAccess::Write);
				FrameBuffer::BlitFrameBuffer(0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight, FBOBufferBit::Color, FBOFilter::Linear);
				leftEyeFBO->UnBind();
				leftEyeFBOResolve->UnBind();

				FrameBuffer::EnableMultisample();

				// Right Eye
				rightEyeFBO->Bind();

				Matrix viewR = GetCurrentViewMatrix(vr::Eye_Right).Inverse();

				cameraR->SetTransformationMatrix(viewR);
				cameraR->Update();

				fwdRenderer->RenderScene(projectionR, cameraR, Scene);

				rightEyeFBO->UnBind();

				FrameBuffer::DisableMultisample();

				rightEyeFBO->Bind(FBOAccess::Read);
				rightEyeFBOResolve->Bind(FBOAccess::Write);
				FrameBuffer::BlitFrameBuffer(0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight, Buffer_Bit::Color, TextureFilter::Linear);
				rightEyeFBO->UnBind();
				rightEyeFBOResolve->UnBind();
			}

			// Render To Screen
			if (renderToScreen)
			{
				fwdRenderer->SetViewPort(0, 0, m_screenWidth, m_screenHeight);
				fwdRenderer->RenderScene((projection != NULL ? *projection : m_projection), (camera != NULL ? camera : cameraL), (SceneToScreen != NULL ? SceneToScreen : Scene));
			}

			vr::Texture_t leftEyeTexture = { (void*)leftEyeResolve->GetBindID(), vr::API_OpenGL, vr::ColorSpace_Gamma };
			vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
			vr::Texture_t rightEyeTexture = { (void*)rightEyeResolve->GetBindID(), vr::API_OpenGL, vr::ColorSpace_Gamma };
			vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);

			UpdateHMDMatrixPose();
		}
	}
}