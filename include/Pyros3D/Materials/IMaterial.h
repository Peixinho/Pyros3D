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
#include <map>
#include <vector>

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
		// Needs to set extraUniformsBinding/BlockName/Size/Offsets on its
		// own CustomShaderMaterial instances (secondpass*.glsl/lastPass.glsl)
		// after construction - same reasoning as IEffect's PostEffectsManager
		// friendship.
		friend class DeferredRenderer;

	public:

		// Constructor
		IMaterial();

		// Destructor
		virtual ~IMaterial();

		// Virtual Methods
		virtual void PreRender() {}
		virtual void Render() {}
		virtual void AfterRender() {}

		// True for materials whose shader is guaranteed to declare the
		// engine's fixed-binding UBOs for what would otherwise be "loose"
		// uniforms (VertexFrameUniforms/ObjectMatrixUniforms/BoneMatrices/
		// VelocityFrameUniforms/VelocityObjectUniforms/AmbientLightUniforms/
		// MaterialUniforms - see resources/shaders/PyrosShader.glsl).
		// IRenderer's uniform-sending loops check this once per material to
		// decide whether to accumulate matching Uniform values into those
		// UBOs (one UpdateUniformBuffer call per block) or fall back to
		// sending them individually via Shader::SendUniform, which is what
		// every material still does by default here. Only GenericShaderMaterial
		// overrides this - custom shaders (CustomShaderMaterial) declare
		// their own uniforms as loose GLSL uniforms and must keep receiving
		// them the old way.
		virtual bool SupportsUniformBlocks() const { return false; }

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
		void RemoveUniform(Uniform* handle);

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

		// Vulkan/SPIR-V rejects non-opaque uniforms outside a block outright
		// (see PyrosShader.glsl's header comment, and IEffect.h's identical
		// mechanism for post-effects). CustomShaderMaterial subclasses whose
		// shader wraps its own loose uniforms in a UBO block set one of
		// these (after the block's layout is decided - see
		// DeferredRenderer.cpp's constructor for the reference usage) so
		// IRenderer::SendExtraUniforms() can fill+upload it generically.
		// Two slots, not one: a shader whose loose uniforms are split
		// across both VERTEX and FRAGMENT stages needs two separate blocks/
		// bindings (secondpassPoint.glsl/secondpassSpot.glsl - one real GPU
		// uniform-buffer binding per stage; empirically, declaring a single
		// block referenced from both stages of one program triggered a real
		// GL_INVALID_OPERATION at draw time on this machine's Metal-backed
		// GL driver - not just a style preference). A material needing only
		// one stage's worth (every other CustomShaderMaterial so far) just
		// leaves slot [1] at its default (binding 0 = unused).
		struct ExtraUniformsBlock
		{
			// Binding 0 (the default) means "not in use" - every material
			// not opting into this slot is unaffected. Binding must be a
			// value no other UBO anywhere in the engine uses - see
			// VulkanRenderDevice::CreateUniformBuffer()'s comment (a
			// *global* registry, not per-program).
			uint32 binding;
			std::string blockName;
			uint32 size;
			uint32 bufferHandle;
			std::vector<uchar> scratch;
			std::map<std::string, uint32> offsets;
			ExtraUniformsBlock() : binding(0), size(0), bufferHandle(0) {}
		};
		ExtraUniformsBlock extraUniforms[2];

	private:

		// Internal ID
		static uint32 _InternalID;

	};

}

#endif /* IMATERIALl_H */