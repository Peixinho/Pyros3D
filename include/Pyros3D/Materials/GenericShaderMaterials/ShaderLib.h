//============================================================================
// Name        : ShaderLib.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ShaderLib
//============================================================================

#ifndef SHADERLIB_H
#define SHADERLIB_H
#include <iostream>
#include <map>

#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>

namespace p3d
{
	namespace ShaderUsage
	{
		enum {
			Color = 0x1,
			Texture = 0x2,
			EnvMap = 0x4,
			Skybox = 0x8,
			Refraction = 0x10,
			Skinning = 0x20,
			CellShading = 0x40,
			BumpMapping = 0x80,
			SpecularMap = 0x100,
			SpecularColor = 0x200,
			DirectionalShadow = 0x400,
			PointShadow = 0x800,
			SpotShadow = 0x1000,
			CastShadows = 0x2000,
			Diffuse = 0x4000,
			TextRendering = 0x8000,
			DebugRendering = 0x10000,
			ClipPlane = 0x20000,
			DeferredRenderer_Gbuffer = 0x40000,
			ParallaxMapping = 0x80000,
			InstancedRendering = 0x100000,
			VelocityRendering = 0x200000
		};
	};
}

#endif /* SHADERLIB_H */
