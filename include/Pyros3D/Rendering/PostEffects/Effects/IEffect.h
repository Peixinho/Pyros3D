//============================================================================
// Name        : IEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Effect Interface
//============================================================================

#ifndef IEFFECT_H
#define IEFFECT_H

#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Other/Export.h>
#include <list>
#include <map>
#include <vector>

//#include <iostream>

namespace p3d {

	namespace RTT {
		enum {
			Color = 1 << 0,
			Depth = 1 << 1,
			LastRTT = 1 << 2,
			CustomTexture = 1 << 3
		};
		struct Info {
			uint32 Type;
			Texture* texture;
			uint32 Unit;
			Info(const uint32 type, const uint32 unit = 0) { Type = type; Unit = unit; }
			Info(Texture *texture, const uint32 type, const uint32 unit = 0) { Type = type; Unit = unit; this->texture = texture; }
		};
	}

	namespace Uniforms {
		namespace PostEffects {
			enum {
				NearFarPlane,
				ScreenDimensions,
				ProjectionFromScene,
				Other,
				// Appended, never reordered: these are compared as values in
				// ProcessPostEffects' switch and stored in effects that were
				// written against the four above.
				//
				// The camera's view matrix and its inverse, for an effect that
				// works in view space - screen-space AO and reflections both
				// do. Only delivered once someone has called
				// PostEffectsManager::SetViewMatrix(); until then an effect
				// keeps whatever value it was given, which is what the Lua
				// chains that push it themselves rely on.
				ViewFromScene,
				InverseViewFromScene
			};
		}
	}

	struct __UniformPostProcess
	{
		__UniformPostProcess(const Uniform &Data)
		{
			uniform = Data;
			handle = -2;
		}
		Uniform uniform;
		int32 handle;
	};

	class PYROS3D_API IEffect {

		friend class PostEffectsManager;

	public:

		IEffect(const uint32 Width, const uint32 Height);

		virtual ~IEffect();

		// Compile Shader
		void CompileShaders();

		// Custom Dimensions
		void Resize(const uint32 width, const uint32 height);
		const uint32 GetWidth() const;
		const uint32 GetHeight() const;

		Texture* GetTexture() { return attachment; }

		// This effect's size as a fraction of the chain's target, kept so
		// PostEffectsManager::Resize() can follow a viewport change without
		// flattening a deliberately smaller stage. Depth of field blurs a
		// quarter-resolution copy of the frame and reads it back alongside
		// the full-resolution one; resizing every effect to the target
		// would silently turn that into a second full-res blur - the same
		// image twice, and no visible near-field bokeh. 1.0 for everything
		// else, which is almost everything.
		void SetResizeScale(const f32 scale) { resizeScale = scale > 0.f ? scale : 1.f; }

		// What RTT::Color means for this effect. Normally the captured
		// scene, which is right for an effect that wants the untouched
		// frame - but wrong for one of those sitting halfway down a chain,
		// because "the untouched frame" then silently discards every effect
		// before it. [Bloom, DepthOfField] rendered exactly as
		// [DepthOfField] did, with no error and no clue why.
		// A multi-pass built-in points this at the output of whatever ran
		// before its group, so the group composes; NULL restores the
		// default. See PostEffectChain::AppendBuiltIn().
		void SetColorOverride(Texture* texture) { colorOverride = texture; }
		Texture* GetColorOverride() const { return colorOverride; }
		f32 GetResizeScale() const { return resizeScale; }

	protected:

		Uniform* AddUniform(const Uniform &Data);

		int32 positionHandle, texcoordHandle;

		// RTT to Use
		void UseCustomTexture(Texture *texture);
		void UseRTT(const uint32 RTT);

		// Shaders Strings
		std::string FragmentShaderString;

		// Shaders
		Shader* shader;

		// Pipeline for this effect's full-screen-triangle draw - built
		// lazily by PostEffectsManager::ProcessPostEffects() on first use
		// (needs a real render pass/render target to exist first, which
		// isn't true yet at construction time), cached here since it's
		// otherwise identical every subsequent frame for a non-last effect
		// (this effect's own FBO is texture-backed and format-stable
		// across a resize - only its VkFramebuffer needs rebuilding, which
		// VulkanRenderDevice::InvalidateFramebuffersForTexture() already
		// handles). 0 = not yet built. Same underlying type as
		// IRenderDevice::DeviceHandle - not spelled that way here to avoid
		// pulling in IRenderDevice.h for a type this header is otherwise
		// device-agnostic about.
		//
		// The *last* effect in the chain is a real exception to "identical
		// every subsequent frame": it draws straight to the swapchain, and
		// a resize on Vulkan destroys and recreates the swapchain's own
		// VkRenderPass (VulkanRenderDevice::RecreateSwapchain()) - a
		// pipeline built against the old one and never rebuilt reads back
		// as solid garbage on MoltenVK, confirmed via a real reproduction
		// (a tiling WM auto-resizing the window right after launch) and
		// debug logging catching the exact moment the render pass got
		// replaced underneath the cached pipeline. See
		// pipelineBuiltForSwapchainGeneration below - PostEffectsManager
		// rebuilds this pipeline whenever that's stale, not just when it's
		// 0.
		uint32 pipelineHandle;
		// IRenderDevice::GetSwapchainGeneration() at the moment
		// pipelineHandle was built - only meaningful for the last effect
		// in the chain (see pipelineHandle's comment); ignored for every
		// other effect, whose own FBO's render pass never needs this.
		uint32 pipelineBuiltForSwapchainGeneration;

		// Vulkan/SPIR-V rejects non-opaque (non-sampler) uniforms outside
		// a block outright (see PyrosShader.glsl's header comment on the
		// same rule for the main shader) - unlike that shader, individual
		// IEffect subclasses are hand-written per-effect GLSL, so there's
		// no single shared UBO layout to reuse. A subclass with any
		// non-sampler uniform (SSAOEffect's uStrength/uRadius/matProj/
		// etc.) wraps them in its own UBO block and sets these three (in
		// its constructor, after the block's layout is decided) so
		// PostEffectsManager can create+fill it generically: binding must
		// be a value no *differently shaped* block anywhere in the engine
		// uses, so this can't reuse PyrosShader.glsl's 0-23 range. Two
		// live PostEffectsManagers each holding their own instance of the
		// same effect at this binding is fine, though - PostEffectsManager
		// points each program's descriptor at extraUniformsBufferHandle
		// below rather than at the device's global binding-point registry
		// (see IRenderDevice::BindUniformBlockIfPresent()).
		// size is the block's total std140 byte size.
		// extraUniformOffsets maps each member's GLSL name to its std140
		// byte offset within that block, matched by hand against the
		// shader's own declared member order/types - same "author both
		// sides, keep them in sync by hand" discipline IRenderer.cpp's
		// MaterialUniformsData/ObjectLightCountsData already use for the
		// main shader's UBOs. Binding 0 (the default) means "no extra
		// uniforms block" - PostEffectsManager skips all of this then.
		uint32 extraUniformsBinding;
		// The block's own GLSL name (e.g. "SSAOParams"), exactly as
		// declared in FragmentShaderString above. Vulkan's
		// BindUniformBlockIfPresent() ignores this (binding points are
		// already static in the SPIR-V), but GL's looks the block up by
		// name via glGetUniformBlockIndex() to rebind it from its default
		// (0) to extraUniformsBinding - passing "" there never matches
		// any block, so the rebind silently never happens and the shader
		// keeps reading whatever's actually bound at binding 0 instead.
		std::string extraUniformsBlockName;
		uint32 extraUniformsSize;
		uint32 extraUniformsBufferHandle;
		std::vector<uchar> extraUniformsScratch;
		std::map<std::string, uint32> extraUniformOffsets;

		// Texture Units
		int32 TextureUnits;

		// RTT Order
		std::vector<RTT::Info> RTTOrder;

		uint32 Width, Height;
		// See SetResizeScale().
		f32 resizeScale = 1.f;
		// See SetColorOverride().
		Texture* colorOverride = NULL;
		FrameBuffer* fbo;
		Texture* attachment;

		std::string VertexShaderString;

	private:
		std::list<__UniformPostProcess> Uniforms;

		void UseColor();
		void UseDepth();
		void UseLastRTT();

	};

};

#endif	/* IEFFECT_H */
