//============================================================================
// Name        : IMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IMaterial Interface
//============================================================================

#ifndef IMATERIAL_H
#define IMATERIAL_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Other/Export.h>
#include <list>

namespace p3d {

	// Culling
	namespace CullFace {
		enum {
			BackFace,
			FrontFace,
			DoubleSided
		};
	}

	class PYROS3D_API IMaterial {

		friend class IRenderer;
		friend class ForwardRenderer;

	public:

		// Constructor
		IMaterial();

		// Destructor
		virtual ~IMaterial();

		// Virtual Methods
		virtual void PreRender() {}
		virtual void Render() {}
		virtual void AfterRender() {}

		// Properties
		void SetCullFace(const uint32 &face);
		uint32 GetCullFace() const;
		bool isWireFrame;
		bool IsWireFrame() const;
		const f32 &GetOpacity() const;
		bool IsTransparent() const;
		void SetOpacity(const f32 &opacity);
		void SetTransparencyFlag(bool transparency);

		// Polygon Offset
		void EnableDethBias(f32 factor, f32 units);
		void DisableDethBias();

		// Blending
		void EnableBlending() { blending = true; }
		void DisableBlending() { blending = false; }
		void BlendingFunction(const uint32 sFactor, const uint32 dFactor) { sfactor = sFactor; dfactor = dFactor; }
		void BlendingEquation(const uint32 Mode) { mode = Mode; }

		// Uniforms        
		std::list<Uniform> GlobalUniforms;
		std::list<Uniform> ModelUniforms;
		std::list<Uniform> UserUniforms;

		// Add Uniform
		Uniform* AddUniform(const Uniform Data);

		// Render WireFrame
		void StartRenderWireFrame();
		void StopRenderWireFrame();

		// Shadows Casting
		void EnableCastingShadows();
		void DisableCastingShadows();
		bool IsCastingShadows();

		// Destroy
		void Destroy();

		// Get Shader Program
		const uint32 &GetShader() const;

		// Get Internal ID
		uint32 GetInternalID();

		// Depth Test and Write
		void EnableDepthTest(const uint32 test = 0) { forceDepthWrite = depthTest = true; depthTestMode = test; }
		void DisableDepthTest() { forceDepthWrite = depthTest = false; }
		void EnableDepthWrite() { depthWrite = true; }
		void DisableDepthWrite() { depthWrite = false; }
		bool IsDepthWritting()
		{
			if (forceDepthWrite) return true;
			else {
				if (IsTransparent()) return false;
				else return depthWrite;
			}
		}
		bool IsDepthTesting()
		{
			return depthTest;
		}

	protected:

		// Depth Test and Write
		bool forceDepthWrite, depthTest, depthWrite;
		uint32 depthTestMode;

		//Casting Shadows
		bool isCastingShadows;

		// Cull Face
		uint32 cullFace;

		// Transparency
		bool isTransparent;

		// Opacity
		f32 opacity;
		Uniform* opacityHandle;

		// Shader program        
		uint32 shaderProgram;

		// Material Options
		uint32 materialID;

		// Depth Bias
		f32 depthFactor, depthUnits;
		bool depthBias;

		// Blending
		bool blending;
		uint32 sfactor, dfactor, mode;

	private:

		// Internal ID
		static uint32 _InternalID;

	};

}

#endif /* IMATERIALl_H */