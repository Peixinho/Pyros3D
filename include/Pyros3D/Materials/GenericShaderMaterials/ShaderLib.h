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
			VelocityRendering = 0x200000,
			PBR = 0x400000,
			PBRMap = 0x800000,
			// Alpha-tested (cutout) rendering: fragments below the
			// material's alpha cutoff are discarded outright rather than
			// blended. The only way to draw foliage, fences, grates or
			// anything else with holes through a deferred renderer, which
			// cannot blend into a G-buffer at all.
			AlphaTest = 0x1000000,
			// Per-instance tint, read from the aInstancedColor vertex
			// attribute RenderingInstancedComponent::EnableInstanceColors()
			// creates. Opt-in rather than always-on because it costs a
			// vec4 per instance, and because Vulkan requires every
			// attribute a compiled shader declares to have a matching
			// vertex buffer attribute - a shader declaring
			// aInstancedColor for a component that never allocated the
			// buffer would fail pipeline creation outright.
			InstancedColor = 0x2000000,
			// Vertex-stage wind sway (GenericShaderMaterial::SetWind()).
			// Displaces vertices above the mesh's local origin along a
			// travelling wave, phase-offset per instance, so a field of
			// instanced cards doesn't move in lockstep.
			VertexWind = 0x4000000,
			// Text whose atlas holds a signed distance field rather than
			// coverage (Font's sdf flag). Thresholded in the shader against
			// the pixel's own rate of change, so one bake stays sharp at any
			// size - which is the entire point of an SDF atlas. Used
			// alongside TextRendering, never instead of it.
			TextSDF = 0x8000000
		};
	};
}

#endif /* SHADERLIB_H */
