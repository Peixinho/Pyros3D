//============================================================================
// Name        : VR_Shaders.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Shaders
//============================================================================

#ifndef VR_SHADERS_H
#define VR_SHADERS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include "VR_Model.h"
#include <openvr.h>

namespace p3d {

	extern const char VR_SHADER_CONTROLLER[];
	extern const char VR_SHADER_DISTORTION[];

};

#endif /*VR_SHADERS_H*/