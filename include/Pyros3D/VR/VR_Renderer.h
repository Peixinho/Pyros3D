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
#include <Pyros3D/VR/VR_Distortion_Geometry.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <openvr.h>

namespace p3d {
	class VR_Renderer {

	public:

		VR_Renderer();
		bool Init();
		void Renderer(SceneGraph* Scene);
		virtual ~VR_Renderer();

	protected:

		// Lenses
		VR_Distortion_Geometry* DistortionLens;

		// OpenVR
		vr::IVRSystem *m_pHMD;
		vr::IVRRenderModels *m_pRenderModels;
		std::string m_strDriver;
		std::string m_strDisplay;

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
		void SetupRenderModelForTrackedDevice(vr::TrackedDeviceIndex_t unTrackedDeviceIndex);
		void UpdateHMDMatrixPose();
		Matrix ConvertSteamVRMatrixToMatrix(const vr::HmdMatrix34_t &matPose);

		// Shader
		int32 distortionPositionHandle, distortionRedInHandle, distortionGreenInHandle, distortionBlueInHandle;

		// Regular Rendering
		ForwardRenderer* fwdRenderer;
		GameObject go;
	};

};

#endif /*VR_RENDERER_H*/