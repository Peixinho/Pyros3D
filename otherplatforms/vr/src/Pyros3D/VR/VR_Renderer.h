//============================================================================
// Name        : VR_Renderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Renderer
//============================================================================

#ifndef VR_RENDERER_H
#define VR_RENDERER_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include "VR_Model.h"
#include <openvr.h>

namespace p3d {

	class VR_Camera : public GameObject {
	public:
		VR_Camera() : GameObject() {}
		virtual void Update() { UpdateTransformation(); }
	};

	class VR_GameObject : public GameObject {
		public:
			VR_GameObject() : GameObject()
			{
				isOnScene = false;
				showingModel = false;
				model = NULL;
				rcomp = NULL;
				texture = NULL;
			}
			virtual ~VR_GameObject() 
			{
				if (model != NULL) delete model;
				if (rcomp != NULL) delete rcomp;
				if (texture != NULL) delete texture;
			}
			uint32 index;
			bool isOnScene;
			bool showingModel;
			uint32 type;
			Renderable* model;
			RenderingComponent* rcomp;
			Texture* texture;
	};

	class VR_Renderer {

	public:

		VR_Renderer(const f32 near = 0.001f, const f32 far = 1000.f, bool renderToScreen = false, const uint32 screenWidth = 0, const uint32 screenHeight = 0);
		void Resize(const uint32 screenWidth, const uint32 screenHeight);
		bool Init();
		void Renderer(SceneGraph* Scene, Projection *projection = NULL, GameObject* camera = NULL, SceneGraph* SceneToScreen = NULL);
		virtual ~VR_Renderer();

		void RenderControllers(bool show);
		void RenderTrackers(bool show);

		VR_GameObject* GetController(const uint32 number) { return (Controllers.size() >= number ? Controllers[(number - 1)] : NULL); }

		void RumbleController(VR_GameObject* controller,uint32 duration) { if (m_pHMD!=NULL && controller!=NULL) m_pHMD->TriggerHapticPulse(controller->index, 0, duration); }

		uint32 GetWidth() { return m_nRenderWidth; }
		uint32 GetHeight() { return m_nRenderHeight; }

		void HandleVRInputs();
		vr::VRControllerState_t GetControllerEvents(uint32 modelIndex);

	protected:
		
		// OpenVR
		vr::IVRSystem *m_pHMD;
		vr::IVRRenderModels *m_pRenderModels;
		std::string m_strDriver;
		std::string m_strDisplay;

		// Screen
		bool renderToScreen;
		uint32 m_screenWidth, m_screenHeight;

		// Transforms
		Matrix m_mat4HMDPose;
		Matrix m_mat4eyePosLeft;
		Matrix m_mat4eyePosRight;
		Matrix m_mat4ProjectionCenter;
		Matrix m_mat4ProjectionLeft;
		Matrix m_mat4ProjectionRight;

		Matrix m_rmat4DevicePose[vr::k_unMaxTrackedDeviceCount];
		vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
		char m_rDevClassChar[vr::k_unMaxTrackedDeviceCount];   // for each device, a character representing its class

		// Dimensions
		uint32 m_nWindowWidth, m_nWindowHeight;
		uint32 m_nRenderWidth, m_nRenderHeight;
		f32 m_nearClip, m_farClip;

		// Eye Specifics
		FrameBuffer *leftEyeFBO, *leftEyeFBOResolve, *rightEyeFBO, *rightEyeFBOResolve;
		Texture *leftEye, *rightEye, *leftEyeResolve, *rightEyeResolve;

		// Methods
		std::string GetTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL);
		Matrix GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye);
		Matrix GetHMDMatrixPoseEye(vr::Hmd_Eye nEye);
		Matrix GetCurrentViewProjectionMatrix(vr::Hmd_Eye nEye);
		Matrix GetCurrentViewMatrix(vr::Hmd_Eye nEye);
		Matrix GetCurrentProjectionMatrix(vr::Hmd_Eye nEye);
		VR_Model* SetupRenderModelForTrackedDevice(vr::TrackedDeviceIndex_t unTrackedDeviceIndex);
		void UpdateHMDMatrixPose();
		void ProcessVREvent(const vr::VREvent_t & event);
		Matrix ConvertSteamVRMatrixToMatrix(const vr::HmdMatrix34_t &matPose);
		void SetupDevice(uint32 modelIndex);
		void LoadModel(uint32 modelIndex, VR_GameObject* go);
		void UnloadModel(uint32 modelIndex);

		// Regular Rendering
		ForwardRenderer* fwdRenderer;
		VR_Camera *cameraL, *cameraR;
		Projection projectionL, projectionR, m_projection;

		bool m_rbShowTrackedDevice[vr::k_unMaxTrackedDeviceCount];
		
		SceneGraph* scene;

		std::vector<VR_GameObject*> Controllers;
		std::vector<VR_GameObject*> TrackingDevices;

		int Alloc(std::vector<VR_GameObject*> array);
		void Free(std::vector<VR_GameObject*> array, int index);
		bool showControllers, showTrackers;
		
	};

};

#endif /*VR_RENDERER_H*/