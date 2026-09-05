//============================================================================
// Name        : GLRenderDevice.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IRenderDevice implementation backed by OpenGL
//============================================================================

#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <cstdlib>

namespace p3d {

	// GL executes every call immediately against whatever's currently
	// bound - it has no real command buffer - so these are no-ops; the
	// returned handle is never meaningfully compared against anything.
	CommandBufferHandle GLRenderDevice::BeginCommandBuffer()
	{
		return 0;
	}

	void GLRenderDevice::EndCommandBuffer(const CommandBufferHandle cmd)
	{
	}

	// No GL equivalent - see the comment on IRenderDevice::BeginFrame()/
	// EndFrame(). SDL2Context::Draw()'s SDL_GL_SwapWindow() is GL's actual
	// frame-boundary/present step, untouched by this.
	void GLRenderDevice::BeginFrame()
	{
	}

	void GLRenderDevice::EndFrame()
	{
	}

	// Genuine no-op, not a stub - GL's context model has no equivalent
	// async-in-flight-work hazard for a caller to defend against here
	// (see IRenderDevice.h's comment on why this exists).
	void GLRenderDevice::WaitIdle()
	{
	}

	uint32 GLRenderDevice::TranslateBufferBit(const uint32 bufferBits)
	{
		uint32 nativeBits = 0;
		if (bufferBits & Buffer_Bit::Color) nativeBits |= GL_COLOR_BUFFER_BIT;
		if (bufferBits & Buffer_Bit::Depth) nativeBits |= GL_DEPTH_BUFFER_BIT;
		if (bufferBits & Buffer_Bit::Stencil) nativeBits |= GL_STENCIL_BUFFER_BIT;
		return nativeBits;
	}

	void GLRenderDevice::Clear(const uint32 nativeBufferBits)
	{
		GLCHECKER(glClear((GLuint)nativeBufferBits));
	}

	void GLRenderDevice::SetClearColor(const Vec4 &color)
	{
		lastClearColor = color;
		GLCHECKER(glClearColor(color.x, color.y, color.z, color.w));
	}

	void GLRenderDevice::SetDepthTest(const bool enabled, const uint32 mode)
	{
		if (enabled)
		{
			GLCHECKER(glEnable(GL_DEPTH_TEST));

			// GL_DITHER defaults to enabled per the GL spec and nothing in
			// this engine ever touched it - invisible on most content, but
			// on a large flat-shaded surface under a smooth diffuse
			// gradient (e.g. a plain-colored floor) it produces a visible
			// fine cross-hatch/noise pattern, since dithering exists
			// specifically to fake extra color depth on subtle gradients.
			// Vulkan has no equivalent fixed-function state to disable.
			GLCHECKER(glDisable(GL_DITHER));

			switch (mode)
			{
			case DepthTest::Always:
				GLCHECKER(glDepthFunc(GL_ALWAYS));
				break;
			case DepthTest::Equal:
				GLCHECKER(glDepthFunc(GL_EQUAL));
				break;
			case DepthTest::GEqual:
				GLCHECKER(glDepthFunc(GL_GEQUAL));
				break;
			case DepthTest::Greater:
				GLCHECKER(glDepthFunc(GL_GREATER));
				break;
			case DepthTest::LEqual:
				GLCHECKER(glDepthFunc(GL_LEQUAL));
				break;
			case DepthTest::Never:
				GLCHECKER(glDepthFunc(GL_NEVER));
				break;
			case DepthTest::NotEqual:
				GLCHECKER(glDepthFunc(GL_NOTEQUAL));
				break;
			case DepthTest::Less:
			default:
				GLCHECKER(glDepthFunc(GL_LESS));
				break;
			}
		}
		else {
			GLCHECKER(glDisable(GL_DEPTH_TEST));
		}
	}

	void GLRenderDevice::SetDepthMask(const bool enabled)
	{
		GLCHECKER(glDepthMask(enabled ? GL_TRUE : GL_FALSE));
	}

	void GLRenderDevice::PrepareDepthClear()
	{
#if !defined(GLES3)
		GLCHECKER(glDepthMask(GL_TRUE));
		GLCHECKER(glClearDepth(1.f));
#endif
	}

	void GLRenderDevice::SetStencilTestEnabled(const bool enabled)
	{
		if (enabled)
		{
			GLCHECKER(glEnable(GL_STENCIL_TEST));
		}
		else {
			GLCHECKER(glDisable(GL_STENCIL_TEST));
		}
	}

	void GLRenderDevice::SetClearStencilValue()
	{
		GLCHECKER(glClearStencil(0));
	}

	void GLRenderDevice::SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask)
	{
		uint32 Func = GL_ALWAYS;
		switch (func)
		{
		case StencilFunc::Never:
			Func = GL_NEVER;
			break;
		case StencilFunc::Less:
			Func = GL_LESS;
			break;
		case StencilFunc::LEqual:
			Func = GL_LEQUAL;
			break;
		case StencilFunc::Greater:
			Func = GL_GREATER;
			break;
		case StencilFunc::GEqual:
			Func = GL_GEQUAL;
			break;
		case StencilFunc::Equal:
			Func = GL_EQUAL;
			break;
		case StencilFunc::Notequal:
			Func = GL_NOTEQUAL;
			break;
		default:
		case StencilFunc::Always:
			Func = GL_ALWAYS;
			break;
		}
		GLCHECKER(glStencilFunc(Func, ref, mask));
	}

	static uint32 TranslateStencilOp(const uint32 op)
	{
		switch (op)
		{
		case StencilOp::Zero:
			return GL_KEEP;
		case StencilOp::Replace:
			return GL_REPLACE;
		case StencilOp::Incr:
			return GL_INCR;
		case StencilOp::Incr_Wrap:
			return GL_INCR_WRAP;
		case StencilOp::Decr:
			return GL_DECR;
		case StencilOp::Decr_Wrap:
			return GL_DECR_WRAP;
		case StencilOp::Invert:
			return GL_INVERT;
		default:
		case StencilOp::Keep:
			return GL_KEEP;
		}
	}

	void GLRenderDevice::SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass)
	{
		GLCHECKER(glStencilOp(TranslateStencilOp(sfail), TranslateStencilOp(dpfail), TranslateStencilOp(dppass)));
	}

	void GLRenderDevice::SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height)
	{
		// Top-left in, bottom-left out - see IRenderDevice's declaration.
		const f32 flipped = (f32)(lastViewport[1] + lastViewport[3]) - (y + height);
		GLCHECKER(glScissor((GLint)x, (GLint)flipped, (GLsizei)width, (GLsizei)height));
	}

	void GLRenderDevice::SetScissorTestEnabled(const bool enabled)
	{
		if (enabled)
		{
			GLCHECKER(glEnable(GL_SCISSOR_TEST));
		}
		else {
			GLCHECKER(glDisable(GL_SCISSOR_TEST));
		}
	}

	void GLRenderDevice::SetWireFrame(const bool enabled)
	{
#if !defined(GLES3)
		GLCHECKER(glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL));
#endif
	}

	void GLRenderDevice::SetColorMask(const bool r, const bool g, const bool b, const bool a)
	{
		GLCHECKER(glColorMask((GLboolean)r, (GLboolean)g, (GLboolean)b, (GLboolean)a));
	}

	void GLRenderDevice::SetPolygonOffsetEnabled(const bool enabled)
	{
		if (enabled)
		{
			GLCHECKER(glEnable(GL_POLYGON_OFFSET_FILL));
		}
		else {
			GLCHECKER(glDisable(GL_POLYGON_OFFSET_FILL));
		}
	}

	void GLRenderDevice::SetPolygonOffset(const f32 factor, const f32 units)
	{
		GLCHECKER(glPolygonOffset(factor, units));
	}

	void GLRenderDevice::SetBlendingEnabled(const bool enabled)
	{
		if (enabled)
		{
			GLCHECKER(glEnable(GL_BLEND));
		}
		else {
			GLCHECKER(glDisable(GL_BLEND));
		}
	}

	static uint32 TranslateBlendFunc(const uint32 factor)
	{
		switch (factor)
		{
		case BlendFunc::Zero:
			return GL_ZERO;
		case BlendFunc::Src_Color:
			return GL_SRC_COLOR;
		case BlendFunc::One_Minus_Src_Color:
			return GL_ONE_MINUS_SRC_COLOR;
		case BlendFunc::Dst_Color:
			return GL_DST_COLOR;
		case BlendFunc::One_Minus_Dst_Color:
			return GL_ONE_MINUS_DST_COLOR;
		case BlendFunc::Src_Alpha:
			return GL_SRC_ALPHA;
		case BlendFunc::One_Minus_Src_Alpha:
			return GL_ONE_MINUS_SRC_ALPHA;
		case BlendFunc::Dst_Alpha:
			return GL_DST_ALPHA;
		case BlendFunc::One_Minus_Dst_Alpha:
			return GL_ONE_MINUS_DST_ALPHA;
		case BlendFunc::Constant_Color:
			return GL_CONSTANT_COLOR;
		case BlendFunc::One_Minus_Constant_Color:
			return GL_ONE_MINUS_CONSTANT_COLOR;
		case BlendFunc::Constant_Alpha:
			return GL_CONSTANT_ALPHA;
		case BlendFunc::One_Minus_Constant_Alpha:
			return GL_ONE_MINUS_CONSTANT_ALPHA;
		case BlendFunc::Src_Alpha_Saturate:
			return GL_SRC_ALPHA_SATURATE;
#if !defined(GLES3)
		case BlendFunc::Src1_Color:
			return GL_SRC1_COLOR;
		case BlendFunc::One_Minus_Src1_Color:
			return GL_ONE_MINUS_SRC1_COLOR;
		case BlendFunc::Src1_Alpha:
			return GL_SRC1_ALPHA;
		case BlendFunc::One_Minus_Src1_Alpha:
			return GL_ONE_MINUS_SRC1_ALPHA;
#endif
		default:
		case BlendFunc::One:
			return GL_ONE;
		}
	}

	void GLRenderDevice::SetBlendFunction(const uint32 sfactor, const uint32 dfactor)
	{
		GLCHECKER(glBlendFunc(TranslateBlendFunc(sfactor), TranslateBlendFunc(dfactor)));
	}

	void GLRenderDevice::SetBlendEquation(const uint32 mode)
	{
		uint32 Mode = GL_FUNC_ADD;
		switch (mode)
		{
		case BlendEq::Subtract:
			Mode = GL_FUNC_SUBTRACT;
			break;
		case BlendEq::Reverse_Subtract:
			Mode = GL_FUNC_REVERSE_SUBTRACT;
			break;
		default:
		case BlendEq::Add:
			Mode = GL_FUNC_ADD;
			break;
		}
		GLCHECKER(glBlendEquation(Mode));
	}

	void GLRenderDevice::SetCullFaceMode(const uint32 cullFace)
	{
		switch (cullFace)
		{
		case CullFace::FrontFace:
			GLCHECKER(glEnable(GL_CULL_FACE));
			GLCHECKER(glCullFace(GL_FRONT));
			break;
		case CullFace::DoubleSided:
			GLCHECKER(glDisable(GL_CULL_FACE));
			break;
		case CullFace::BackFace:
		default:
			GLCHECKER(glEnable(GL_CULL_FACE));
			GLCHECKER(glCullFace(GL_BACK));
			break;
		}
	}

	void GLRenderDevice::DisableCullFace()
	{
		GLCHECKER(glDisable(GL_CULL_FACE));
	}

	DeviceHandle GLRenderDevice::CreatePipeline(const PipelineDesc &desc)
	{
		DeviceHandle handle = nextPipelineHandle++;
		pipelines[handle] = desc;
		return handle;
	}

	void GLRenderDevice::DestroyPipeline(const DeviceHandle pipeline)
	{
		pipelines.erase(pipeline);
	}

	void GLRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		std::map<DeviceHandle, PipelineDesc>::const_iterator it = pipelines.find(pipeline);
		if (it == pipelines.end())
			return;
		const PipelineDesc &desc = it->second;

		UseProgram(desc.shaderProgram);
		SetDepthTest(desc.depthTest, desc.depthTestMode);
		SetDepthMask(desc.depthWrite);
		SetBlendingEnabled(desc.blendingEnabled);
		if (desc.blendingEnabled)
		{
			SetBlendFunction(desc.blendSrcFactor, desc.blendDstFactor);
			SetBlendEquation(desc.blendEquation);
		}
		SetCullFaceMode(desc.cullFace);
		SetWireFrame(desc.wireframe);
	}

	void GLRenderDevice::EnableClipDistance(const uint32 index)
	{
		// Intentionally a no-op (same as VulkanRenderDevice). Island water
		// and any ShaderUsage::ClipPlane material clip via PyrosShader's
		// CLIPSPACE discard (uClipPlane0 in VertexFrameUniforms), not
		// gl_ClipDistance. Enabling GL_CLIP_DISTANCEi without the vertex
		// shader writing gl_ClipDistance[i] is undefined - on some GL
		// drivers (seen as DemoLauncher Island multipass going fully
		// black while the same C++ IslandDemo looked fine) that discards
		// every triangle into the reflection/refraction FBOs. Keep clip
		// in the shader discard path only.
		(void)index;
	}

	void GLRenderDevice::DisableClipDistance(const uint32 index)
	{
		(void)index;
	}

	void GLRenderDevice::SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height)
	{
		lastViewport[0] = x; lastViewport[1] = y; lastViewport[2] = width; lastViewport[3] = height;
		GLCHECKER(glViewport(x, y, width, height));
	}

	void GLRenderDevice::UseProgram(const uint32 program)
	{
		GLCHECKER(glUseProgram(program));
	}

	DeviceHandle GLRenderDevice::CreateVertexArray()
	{
		GLuint vao = 0;
		GLCHECKER(glGenVertexArrays(1, &vao));
		return vao;
	}

	void GLRenderDevice::DeleteVertexArray(const DeviceHandle vao)
	{
		GLuint id = vao;
		GLCHECKER(glDeleteVertexArrays(1, &id));
	}

	void GLRenderDevice::BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao)
	{
		GLCHECKER(glBindVertexArray(vao));
	}

	void GLRenderDevice::BindArrayBuffer(const uint32 buffer)
	{
		GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, buffer));
	}

	void GLRenderDevice::BindElementBuffer(const uint32 buffer)
	{
		GLCHECKER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer));
	}

	void GLRenderDevice::SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset)
	{
		GLCHECKER(glEnableVertexAttribArray(location));
		GLCHECKER(glVertexAttribPointer(location, typeCount, nativeType, GL_FALSE, stride, BUFFER_OFFSET(offset)));
	}

	void GLRenderDevice::SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset)
	{
		GLCHECKER(glEnableVertexAttribArray(location));
		GLCHECKER(glVertexAttribPointer(location, componentCount, GL_FLOAT, GL_FALSE, stride, BUFFER_OFFSET(offset)));
	}

	void GLRenderDevice::DisableVertexAttribute(const int32 location)
	{
		GLCHECKER(glDisableVertexAttribArray(location));
	}

	void GLRenderDevice::SetVertexAttributeDivisor(const int32 location, const uint32 divisor)
	{
		// GLES3 / WebGL2 have glVertexAttribDivisor as core. Skipping it
		// here made ParticleSystem (and any divisor=1 AttributeBuffer) feed
		// a different instance slot to each quad vertex — folded billboards
		// and only a couple of visible fragments. Plain GLES2 is the only
		// profile that needs an extension path we don't use on web.
#if defined(GLES2) && !defined(GLES3)
		(void)location;
		(void)divisor;
#else
		GLCHECKER(glVertexAttribDivisor(location, divisor));
#endif
	}

	void GLRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint, const DeviceHandle bufferHandle)
	{
		// bufferHandle is Vulkan-only bookkeeping (see IRenderDevice.h).
		// GL's binding-point -> buffer table is glBindBufferBase's, and
		// ReplaceUniformBuffer() re-issues it for the buffer it just wrote
		// on every call - which, for the per-material extraUniforms blocks,
		// is immediately before that material's own draw. So GL already
		// reads the right buffer even when two live materials share one
		// binding point; only the program-side block -> binding point
		// mapping is this function's job here.
		(void)bufferHandle;
		GLuint blockIndex = glGetUniformBlockIndex(program, blockName.c_str());
		if (blockIndex != GL_INVALID_INDEX)
		{
			GLCHECKER(glUniformBlockBinding(program, blockIndex, TranslateUniformBindingPoint(bindingPoint)));
		}
	}

	Matrix GLRenderDevice::TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip)
	{
		// No-op - Matrix::PerspectiveMatrix()/OrthoMatrix() already build
		// GL's own NDC convention. See the comment on this method in
		// IRenderDevice.h. skipYFlip is Vulkan-only (there's no flip here
		// to skip).
		(void)skipYFlip;
		return projectionMatrix;
	}

	Matrix GLRenderDevice::TranslateShadowBiasMatrix()
	{
		// GL's own clip-space Z is still [-1,1] (TranslateProjectionMatrix()
		// above is a no-op) - Matrix::BIAS's full X/Y/Z remap is exactly
		// what's needed, unchanged. See the comment on this method in
		// IRenderDevice.h.
		return Matrix::BIAS;
	}

	uint32 GLRenderDevice::TranslateDrawType(const uint32 engineDrawType)
	{
		switch (engineDrawType)
		{
		case DrawingType::Lines:
			return GL_LINES;
		case DrawingType::Points:
			return GL_POINTS;
		case DrawingType::Line_Loop:
			return GL_LINE_LOOP;
		case DrawingType::Line_Strip:
			return GL_LINE_STRIP;
		case DrawingType::Triangle_Fan:
			return GL_TRIANGLE_FAN;
		case DrawingType::Triangle_Strip:
			return GL_TRIANGLE_STRIP;
		case DrawingType::Triangles:
		default:
			return GL_TRIANGLES;
		}
	}

	void GLRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count)
	{
		GLCHECKER(glDrawArrays(nativeDrawType, first, count));
	}

	void GLRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount)
	{
		GLCHECKER(glDrawElements(nativeDrawType, indexCount, __INDEX_TYPE__, BUFFER_OFFSET(0)));
	}

	void GLRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount)
	{
		GLCHECKER(glDrawElementsInstanced(nativeDrawType, indexCount, __INDEX_TYPE__, BUFFER_OFFSET(0), instanceCount));
	}

	std::map<uint32, uint32> GLRenderDevice::uniformBindingRemap;

	// See GLRenderDevice.h. Anything the driver accepts passes through
	// unchanged; anything past its limit is given a slot that nothing else has
	// claimed. Every translation is recorded, pass-through included, so "free"
	// means free according to what this device has actually handed out rather
	// than according to a hardcoded guess - the previous version reserved a
	// fixed 5-15 window because that is the gap
	// IRenderer::RetainSharedUniformBuffers() happens to leave today, which was
	// exactly as many slots as there were out-of-range blocks and would have
	// broken silently the moment either side changed.
	//
	// Slot 0 is never handed out. It is GlobalMatrices' own, and it is also
	// where GLSL leaves any block the engine forgot to bind: keeping the
	// smallest block there means such a block fails loudly on WebGL2 (the
	// buffer is too small for it) instead of quietly reading someone else's
	// data. That is how the deferred directional pass' block was found.
	//
	// A driver at the GLES3 minimum of 24 cannot be satisfied by any allocation
	// scheme here: the engine declares more distinct blocks than that. The
	// error below says so with the numbers, which is the signal to reduce the
	// block count rather than to shuffle slots.
	uint32 GLRenderDevice::TranslateUniformBindingPoint(const uint32 engineBinding)
	{
		static int32 maxBindings = 0;
		if (maxBindings == 0)
		{
			GLint v = 0;
			glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &v);
			while (glGetError() != GL_NO_ERROR) {}
			// A driver that will not answer gets the benefit of the doubt:
			// pass everything through, exactly as before this existed.
			maxBindings = (v > 0) ? (int32)v : 0x7fffffff;
		}

		std::map<uint32, uint32>::const_iterator it = uniformBindingRemap.find(engineBinding);
		if (it != uniformBindingRemap.end())
			return it->second;

		if ((int32)engineBinding < maxBindings)
		{
			uniformBindingRemap[engineBinding] = engineBinding;
			return engineBinding;
		}

		for (uint32 slot = 1; (int32)slot < maxBindings; slot++)
		{
			bool taken = false;
			for (std::map<uint32, uint32>::const_iterator i = uniformBindingRemap.begin(); i != uniformBindingRemap.end(); i++)
				if (i->second == slot) { taken = true; break; }
			if (taken)
				continue;
			uniformBindingRemap[engineBinding] = slot;
			echo("UBO binding " + std::to_string(engineBinding) + " is past this driver's limit of "
				+ std::to_string(maxBindings) + "; using " + std::to_string(slot) + " instead");
			return slot;
		}
		echo("ERROR: no GL uniform binding point left for engine binding " + std::to_string(engineBinding)
			+ " - this driver has " + std::to_string(maxBindings) + " and all of them are taken."
			+ " That block will not be bound, and every draw using it will be dropped."
			+ " The engine needs fewer distinct uniform blocks to run here.");
		return engineBinding;
	}

	DeviceHandle GLRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint)
	{
		GLuint ubo = 0;
		GLCHECKER(glGenBuffers(1, &ubo));
		GLCHECKER(glBindBuffer(GL_UNIFORM_BUFFER, ubo));
		GLCHECKER(glBufferData(GL_UNIFORM_BUFFER, sizeBytes, NULL, GL_DYNAMIC_DRAW));
		// Store the translated point, not the engine one: ReplaceUniformBuffer
		// re-issues this bind straight from the map.
		const uint32 glBinding = TranslateUniformBindingPoint(bindingPoint);
		GLCHECKER(glBindBufferBase(GL_UNIFORM_BUFFER, glBinding, ubo));
		GLCHECKER(glBindBuffer(GL_UNIFORM_BUFFER, 0));
		uniformBufferBindings[ubo] = glBinding;
		uniformBufferSizes[ubo] = sizeBytes;
		return ubo;
	}

	void GLRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data)
	{
		GLCHECKER(glBindBuffer(GL_UNIFORM_BUFFER, buffer));
		GLCHECKER(glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeBytes, data));
		GLCHECKER(glBindBuffer(GL_UNIFORM_BUFFER, 0));
	}

	void GLRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data)
	{
		GLCHECKER(glBindBuffer(GL_UNIFORM_BUFFER, buffer));
		// Orphan storage (see IRenderDevice.h). Never shrink below the
		// capacity established at CreateUniformBuffer — WebGL2 rejects draws
		// when the bound UBO is smaller than the shader's uniform block.
		uint32 capacity = sizeBytes;
		std::map<DeviceHandle, uint32>::iterator sizeIt = uniformBufferSizes.find(buffer);
		if (sizeIt != uniformBufferSizes.end())
		{
			if (sizeBytes < sizeIt->second)
				capacity = sizeIt->second;
			else
				sizeIt->second = sizeBytes;
		}
		else
		{
			uniformBufferSizes[buffer] = sizeBytes;
		}

		if (capacity == sizeBytes)
		{
			GLCHECKER(glBufferData(GL_UNIFORM_BUFFER, capacity, data, GL_DYNAMIC_DRAW));
		}
		else
		{
			GLCHECKER(glBufferData(GL_UNIFORM_BUFFER, capacity, NULL, GL_DYNAMIC_DRAW));
			if (data && sizeBytes > 0)
				GLCHECKER(glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeBytes, data));
		}
		// Re-bind the indexed slot after orphaning - required on some macOS
		// GL drivers that drop the BindBufferBase association when storage
		// is replaced (Vulkan path is unaffected; this is GL-only).
		std::map<DeviceHandle, uint32>::iterator it = uniformBufferBindings.find(buffer);
		if (it != uniformBufferBindings.end())
			GLCHECKER(glBindBufferBase(GL_UNIFORM_BUFFER, it->second, buffer));
		GLCHECKER(glBindBuffer(GL_UNIFORM_BUFFER, 0));
	}

	void GLRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer)
	{
		uniformBufferBindings.erase(buffer);
		uniformBufferSizes.erase(buffer);
		GLuint id = buffer;
		GLCHECKER(glDeleteBuffers(1, &id));
	}

	static uint32 TranslateBufferType(const uint32 bufferType)
	{
		switch (bufferType)
		{
		case Buffer::Type::Index:
			return GL_ELEMENT_ARRAY_BUFFER;
		case Buffer::Type::Vertex:
		case Buffer::Type::Attribute:
		default:
			return GL_ARRAY_BUFFER;
		}
	}

	static uint32 TranslateBufferDraw(const uint32 bufferDraw)
	{
		switch (bufferDraw)
		{
		case Buffer::Draw::Static:
			return GL_STATIC_DRAW;
		case Buffer::Draw::Dynamic:
			return GL_DYNAMIC_DRAW;
		case Buffer::Draw::Stream:
		default:
			return GL_STREAM_DRAW;
		}
	}

	DeviceHandle GLRenderDevice::CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length)
	{
		uint32 nativeType = TranslateBufferType(bufferType);
		GLuint id = 0;
		GLCHECKER(glGenBuffers(1, &id));
		GLCHECKER(glBindBuffer(nativeType, id));
		GLCHECKER(glBufferData(nativeType, length, data, TranslateBufferDraw(bufferDraw)));
		GLCHECKER(glBindBuffer(nativeType, 0));
		return id;
	}

	void GLRenderDevice::ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length)
	{
		uint32 nativeType = TranslateBufferType(bufferType);
		GLCHECKER(glBindBuffer(nativeType, buffer));
#if !defined(GLES3) && !defined(GLLEGACY) && !defined(GL42) && !defined(GL41)
		GLCHECKER(glInvalidateBufferData(buffer));
#endif
		GLCHECKER(glBufferData(nativeType, length, data, TranslateBufferDraw(bufferDraw)));
		GLCHECKER(glBindBuffer(nativeType, 0));
	}

	void GLRenderDevice::UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length)
	{
		uint32 nativeType = TranslateBufferType(bufferType);
		GLCHECKER(glBindBuffer(nativeType, buffer));
#if !defined(GLES3) && !defined(GLLEGACY) && !defined(GL42) && !defined(GL41)
		GLCHECKER(glInvalidateBufferData(buffer));
#endif
		GLCHECKER(glBufferSubData(nativeType, 0, length, data));
		GLCHECKER(glBindBuffer(nativeType, 0));
	}

	void GLRenderDevice::DestroyBuffer(const DeviceHandle buffer)
	{
		GLuint id = buffer;
		GLCHECKER(glDeleteBuffers(1, &id));
	}

	void *GLRenderDevice::MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType)
	{
#if !defined(GLES3)
		uint32 nativeType = TranslateBufferType(bufferType);
		GLCHECKER(glBindBuffer(nativeType, buffer));
		uint32 MP;
		switch (mappingType)
		{
		case Buffer::Mapping::Read:
			MP = GL_READ_ONLY;
			break;
		case Buffer::Mapping::Write:
			MP = GL_WRITE_ONLY;
			break;
		case Buffer::Mapping::ReadAndWrite:
			MP = GL_READ_WRITE;
			break;
		}

		void* vboData = glMapBuffer(nativeType, MP);
		if (vboData)
		{
			GLCHECKER(glBindBuffer(nativeType, 0));
			return vboData;
		}
		else if (!vboData)
		{
			GLCHECKER(glGetBufferPointerv(nativeType, GL_BUFFER_MAP_POINTER, &vboData));
			GLCHECKER(glBindBuffer(nativeType, 0));
			if (vboData) return vboData;
		}
#endif
		return NULL;
	}

	void GLRenderDevice::UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType)
	{
		uint32 nativeType = TranslateBufferType(bufferType);
		GLCHECKER(glBindBuffer(nativeType, buffer));
		GLCHECKER(glUnmapBuffer(nativeType));
		GLCHECKER(glBindBuffer(nativeType, 0));
	}

	uint32 GLRenderDevice::TranslateAttributeType(const uint32 engineType)
	{
		switch (engineType)
		{
		case Buffer::Attribute::Type::Int:
			return GL_INT;
		case Buffer::Attribute::Type::Short:
			return GL_SHORT;
		case Buffer::Attribute::Type::Float:
		case Buffer::Attribute::Type::Vec2:
		case Buffer::Attribute::Type::Vec3:
		case Buffer::Attribute::Type::Vec4:
		case Buffer::Attribute::Type::Matrix:
		default:
			return GL_FLOAT;
		}
	}

	std::string GLRenderDevice::BuildShaderSource(const std::string &definitions, const std::string &shaderBody)
	{
#if defined(GLES3)
		// GLSL ES 3.00 has no default precision in the FRAGMENT stage for
		// float, int, or any of the shadow/3D/array/integer sampler types -
		// only sampler2D and samplerCube get one. A shader that uses
		// sampler2DShadow without declaring its precision does not warn, it
		// fails to compile ("sampler2DShadow: no precision defined"), and the
		// program then links to nothing: WebGL reports "useProgram: program
		// not valid" from somewhere else entirely and the scene silently
		// draws no geometry while the UI, which has its own shader, carries on.
		//
		// Declared for BOTH stages rather than only the fragment one. A
		// precision statement is legal in a vertex shader too (where these
		// types default to highp anyway), so emitting one preamble avoids
		// threading the shader stage through BuildShaderSource for no gain.
		//
		// These are DEFAULTS, and that word is load-bearing: a `precision`
		// statement in the shader body silently overrides whatever is set
		// here, because redeclaring the default precision is legal and does
		// not warn. Every shader under resources/shaders/ used to open with
		// `#if defined(GLES3) precision mediump float; #endif`, which landed
		// AFTER this preamble and quietly demoted the whole program - see the
		// note on the float line below before adding another one.
		static const char* kGles3Precision =
			// highp, and it has to stay highp all the way through the
			// shader body. On Apple GPUs (iPhone, and Apple Silicon under
			// a browser) mediump really is fp16: +/-65504 with about three
			// decimal digits. World coordinates through the MVP, plus the
			// per-light shadow matrices, leave that range - gl_Position
			// comes out garbage or NaN and the triangles are dropped
			// before rasterization. No GL error, no fragments, and the
			// object does not even occlude what is behind it. Desktop GL
			// never saw this because GLES3 is undefined there.
			"precision highp float;\n"
			"precision highp int;\n"
			"precision lowp sampler2D;\n"
			"precision lowp samplerCube;\n"
			"precision highp sampler3D;\n"
			// highp for the SHADOW samplers, not lowp. A shadow sampler does
			// a depth comparison, and the precision qualifier governs the
			// coordinate and the comparison as well as the result - at lowp
			// the compare is meaningless, every lit surface reads as
			// shadowed, and a lit object comes out black. Which against a
			// dark viewport is indistinguishable from not drawing at all.
			//
			// The plain colour samplers keep the spec's own default (lowp);
			// they only carry an 8-bit texel.
			"precision highp sampler2DShadow;\n"
			"precision highp samplerCubeShadow;\n"
			"precision lowp sampler2DArray;\n"
			"precision highp sampler2DArrayShadow;\n";
		// EMSCRIPTEN belongs here, not in each material type:
		// CustomShaderMaterial defined it and GenericShaderMaterial did not,
		// so the two paths compiled different source from the same file.
		// Defining it once, in the device that knows what platform it is, is
		// what stops them disagreeing again.
		//
		// It gates PyrosShader.glsl's _highpMat4/_highpVec4 macros, which are
		// now belt-and-braces: they only ever qualified the two _transpose
		// helpers, so while the file still defaulted to mediump the actual
		// MVP transform, the UBO matrices and the shadow coordinates stayed
		// low-precision regardless. That is why defining EMSCRIPTEN alone
		// changed nothing on screen - the default precision was the bug.
		std::string platformDefines("#define GLES3\n");
#if defined(EMSCRIPTEN)
		platformDefines += "#define EMSCRIPTEN\n";
#endif
		return std::string("#version 300 es\n") + platformDefines + kGles3Precision
			+ definitions + std::string(" ") + shaderBody;
#elif defined(GL42)
		return std::string("#version 420\n") + definitions + std::string(" ") + shaderBody;
#elif defined(GL41)
		return std::string("#version 410\n") + definitions + std::string(" ") + shaderBody;
#else
		return std::string("#version 450\n") + definitions + std::string(" ") + shaderBody;
#endif
	}

	DeviceHandle GLRenderDevice::CreateShaderStage(const uint32 engineShaderType)
	{
		switch (engineShaderType)
		{
		case ShaderType::VertexShader:
			return glCreateShader(GL_VERTEX_SHADER);
		case ShaderType::FragmentShader:
			return glCreateShader(GL_FRAGMENT_SHADER);
		case ShaderType::GeometryShader:
		default:
			// Geometry shader creation was already commented out upstream
			// (Shaders.cpp's old CompileShader never actually called
			// glCreateShader(GL_GEOMETRY_SHADER)) - preserved as-is.
			return 0;
		}
	}

	bool GLRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog)
	{
		uint32 len = source.length();
		const char *src = source.c_str();

		GLCHECKER(glShaderSource(shader, 1, (const GLchar**)&src, (const GLint *)&len));
		GLCHECKER(glCompileShader(shader));

		GLint result, length = 0;

		GLCHECKER(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));
		if (length > 1)
		{
			char* log = (char*)malloc(length);
			GLCHECKER(glGetShaderInfoLog(shader, length, &result, log));
			GLCHECKER(glGetShaderiv(shader, GL_COMPILE_STATUS, &result));
			errorLog = std::string(log);
			free(log);

			if (result == GL_FALSE)
				return false;
		}
		return true;
	}

	DeviceHandle GLRenderDevice::CreateProgram()
	{
		return (uint32)glCreateProgram();
	}

	void GLRenderDevice::AttachShaderStage(const DeviceHandle program, const DeviceHandle shader)
	{
		GLCHECKER(glAttachShader(program, shader));
	}

	bool GLRenderDevice::LinkProgram(const DeviceHandle program, std::string &errorLog)
	{
		GLCHECKER(glLinkProgram(program));

		GLint result, length = 0;

		GLCHECKER(glGetProgramiv(program, GL_LINK_STATUS, &result));
		if (result == GL_FALSE)
		{
			GLCHECKER(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length));
			char* log = (char*)malloc(length);
			GLCHECKER(glGetProgramInfoLog(program, length, &result, log));
			errorLog = std::string(log);
			free(log);
			return false;
		}
		return true;
	}

	bool GLRenderDevice::IsProgram(const DeviceHandle id)
	{
		return glIsProgram(id);
	}

	bool GLRenderDevice::IsShaderStage(const DeviceHandle id)
	{
		return glIsShader(id);
	}

	void GLRenderDevice::DetachShaderStage(const DeviceHandle program, const DeviceHandle shader)
	{
		GLCHECKER(glDetachShader(program, shader));
	}

	void GLRenderDevice::DeleteShaderStage(const DeviceHandle shader)
	{
		GLCHECKER(glDeleteShader(shader));
	}

	void GLRenderDevice::DeleteProgram(const DeviceHandle program)
	{
		GLCHECKER(glDeleteProgram(program));
	}

	int32 GLRenderDevice::GetUniformLocation(const uint32 program, const std::string &name)
	{
		return glGetUniformLocation(program, name.c_str());
	}

	int32 GLRenderDevice::GetAttributeLocation(const uint32 program, const std::string &name)
	{
		return glGetAttribLocation(program, name.c_str());
	}

	void GLRenderDevice::SendUniformInt(const int32 handle, const int32 *data, const uint32 count)
	{
		GLCHECKER(glUniform1iv(handle, count, (const GLint*)data));
	}

	void GLRenderDevice::SendUniformFloat(const int32 handle, const f32 *data, const uint32 count)
	{
		GLCHECKER(glUniform1fv(handle, count, data));
	}

	void GLRenderDevice::SendUniformVec2(const int32 handle, const f32 *data, const uint32 count)
	{
		GLCHECKER(glUniform2fv(handle, count, data));
	}

	void GLRenderDevice::SendUniformVec3(const int32 handle, const f32 *data, const uint32 count)
	{
		GLCHECKER(glUniform3fv(handle, count, data));
	}

	void GLRenderDevice::SendUniformVec4(const int32 handle, const f32 *data, const uint32 count)
	{
		GLCHECKER(glUniform4fv(handle, count, data));
	}

	void GLRenderDevice::SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count)
	{
		GLCHECKER(glUniformMatrix4fv(handle, count, false, data));
	}

	void GLRenderDevice::TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type)
	{
		switch (engineDataType)
		{
		case TextureDataType::DepthComponent:
		case TextureDataType::DepthComponent24:
			internalFormat = GL_DEPTH_COMPONENT24;
			format = GL_DEPTH_COMPONENT;
#if defined(GLES3)
			type = GL_UNSIGNED_INT;
#else
			type = GL_FLOAT;
#endif
			break;
		case TextureDataType::DepthComponent16:
			internalFormat = GL_DEPTH_COMPONENT16;
			format = GL_DEPTH_COMPONENT;
#if defined(GLES3)
			type = GL_UNSIGNED_INT;
#else
			type = GL_FLOAT;
#endif
			break;
		case TextureDataType::DepthComponent32:
			internalFormat = GL_DEPTH_COMPONENT32F;
			format = GL_DEPTH_COMPONENT;
			type = GL_FLOAT;
			break;
		case TextureDataType::R16F:
			internalFormat = GL_R16F;
			format = GL_RED;
			type = GL_FLOAT;
			break;
		case TextureDataType::R32F:
			internalFormat = GL_R32F;
			format = GL_RED;
			type = GL_FLOAT;
			break;
		case TextureDataType::R16I:
			internalFormat = GL_R16I;
			format = GL_R8;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::R32I:
			internalFormat = GL_R32I;
			format = GL_R8;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RG8:
			internalFormat = GL_RG8;
			format = GL_RG;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RG16F:
			internalFormat = GL_RG16F;
			format = GL_RG;
			type = GL_FLOAT;
			break;
		case TextureDataType::RG32F:
			internalFormat = GL_RG32F;
			format = GL_RG;
			type = GL_FLOAT;
			break;
		case TextureDataType::RG16I:
			internalFormat = GL_RG16I;
			format = GL_RG;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RG32I:
			internalFormat = GL_RG32I;
			format = GL_RG;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGB8:
			internalFormat = GL_RGB8;
			format = GL_RGB;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGB16F:
			internalFormat = GL_RGB16F;
			format = GL_RGB;
			type = GL_FLOAT;
			break;
		case TextureDataType::RGB32F:
			internalFormat = GL_RGB32F;
			format = GL_RGB;
			type = GL_FLOAT;
			break;
		case TextureDataType::RGB16I:
			internalFormat = GL_RGB16I;
			format = GL_RGB;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGB32I:
			internalFormat = GL_RGB32I;
			format = GL_RGB;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGBA16F:
			internalFormat = GL_RGBA16F;
			format = GL_RGBA;
			type = GL_FLOAT;
			break;
		case TextureDataType::RGBA32F:
			internalFormat = GL_RGBA32F;
			format = GL_RGBA;
			type = GL_FLOAT;
			break;
		case TextureDataType::RGBA16I:
			internalFormat = GL_RGBA16I;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGBA32I:
			internalFormat = GL_RGBA32I;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::R8:
			internalFormat = GL_R8;
			format = GL_RED;
			type = GL_UNSIGNED_BYTE;
			break;
#if !defined(GLES3)
		case TextureDataType::BGR:
			internalFormat = GL_RGB8;
			format = GL_BGR;
			type = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::BGRA:
			internalFormat = GL_RGBA8;
			format = GL_BGRA;
			type = GL_UNSIGNED_BYTE;
			break;
#endif
		case TextureDataType::RGBA:
		default:
			internalFormat = GL_RGBA;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			break;
		}
	}

	void GLRenderDevice::TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode)
	{
		switch (engineTextureType) {
		case TextureType::CubemapNegative_X:
			mode = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
			subMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapNegative_Y:
			mode = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			subMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapNegative_Z:
			mode = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			subMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapPositive_X:
			mode = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
			subMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapPositive_Y:
			mode = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
			subMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapPositive_Z:
			mode = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
			subMode = GL_TEXTURE_CUBE_MAP;
			break;
#if !defined(GLES3)
		case TextureType::Texture_Multisample:
			mode = GL_TEXTURE_2D_MULTISAMPLE;
			subMode = GL_TEXTURE_2D_MULTISAMPLE;
			break;
#endif
		case TextureType::Texture:
		default:
			mode = GL_TEXTURE_2D;
			subMode = GL_TEXTURE_2D;
			break;
		}
	}

	void *GLRenderDevice::GetImGuiTextureID(const DeviceHandle texture, const uint32 engineTextureType)
	{
		// GL's ImGui backend takes the texture name straight through as the
		// ImTextureID, so the engine handle is already what is wanted. Only
		// plain 2D colour targets: ImGui samples with a 2D sampler, so a
		// cube face or a multisample target would draw as garbage.
		if (texture == 0 || engineTextureType != TextureType::Texture)
			return NULL;
		return (void *)(intptr_t)texture;
	}

	DeviceHandle GLRenderDevice::CreateTextureObject()
	{
		GLuint id = 0;
		GLCHECKER(glGenTextures(1, &id));
		return id;
	}

	void GLRenderDevice::DestroyTextureObject(const DeviceHandle texture)
	{
		GLuint id = texture;
		GLCHECKER(glDeleteTextures(1, &id));
	}

	void GLRenderDevice::BindTextureToTarget(const uint32 target, const DeviceHandle texture)
	{
		GLCHECKER(glBindTexture(target, texture));
	}

	void GLRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap)
	{
		(void)willMipmap; // see IRenderDevice.h's comment - Vulkan-only
		GLCHECKER(glTexImage2D(target, level, internalFormat, width, height, 0, format, type, data));
	}

	void GLRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height)
	{
#if !defined(GLES3)
		GLCHECKER(glTexImage2DMultisample(target, samples, internalFormat, width, height, true));
#endif
	}

	void GLRenderDevice::GenerateMipmap(const uint32 target)
	{
		GLCHECKER(glGenerateMipmap(target));
	}

	void GLRenderDevice::SetTextureWrapS(const uint32 target, const uint32 engineRepeat)
	{
		switch (engineRepeat)
		{
		case TextureRepeat::ClampToEdge:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			break;
#if !defined(GLES3)
		case TextureRepeat::ClampToBorder:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
			break;
#endif
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT));
			break;
		}
	}

	void GLRenderDevice::SetTextureWrapT(const uint32 target, const uint32 engineRepeat)
	{
		switch (engineRepeat)
		{
		case TextureRepeat::ClampToEdge:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			break;
#if !defined(GLES3)
		case TextureRepeat::ClampToBorder:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
			break;
#endif
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT));
			break;
		}
	}

	void GLRenderDevice::SetTextureWrapR(const uint32 target, const uint32 engineRepeat)
	{
#if !defined(GLES3)
		switch (engineRepeat)
		{
		case TextureRepeat::ClampToEdge:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
			break;
		case TextureRepeat::ClampToBorder:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER));
			break;
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_REPEAT));
			break;
		}
#endif
	}

	void GLRenderDevice::SetTextureMagFilter(const uint32 target, const uint32 engineFilter)
	{
		switch (engineFilter)
		{
		case TextureFilter::Nearest:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			break;
		default:
			GLCHECKER(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			break;
		}
	}

	void GLRenderDevice::SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap)
	{
		switch (engineFilter)
		{
		case TextureFilter::Nearest:
		case TextureFilter::NearestMipmapNearest:
			if (hasMipmap) {
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST));
			}
			else {
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			}
			break;
		case TextureFilter::NearestMipmapLinear:
			if (hasMipmap) {
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR));
			}
			else {
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			}
			break;
		case TextureFilter::LinearMipmapNearest:
			if (hasMipmap) {
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST));
			}
			else {
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			}
			break;
		case TextureFilter::Linear:
		case TextureFilter::LinearMipmapLinear:
		default:
			if (hasMipmap)
			{
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
			}
			else {
				GLCHECKER(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			}
			break;
		}
	}

	void GLRenderDevice::SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel)
	{
#if !defined(GLES3)
		GLCHECKER(glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, baseLevel));
		GLCHECKER(glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, maxLevel));
#endif
	}

	void GLRenderDevice::SetTextureBorderColor(const uint32 target, const Vec4 &color)
	{
#if !defined(GLES3)
		GLCHECKER(glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, (const GLfloat*)&color));
#endif
	}

	void GLRenderDevice::SetTextureCompareMode(const uint32 target)
	{
		// NOT guarded for GLES3, though it used to be. GL_TEXTURE_COMPARE_MODE
		// is core in OpenGL ES 3.0 and in WebGL2 - the old #if was inherited
		// from GLES2, where it genuinely did not exist.
		//
		// Skipping it on the web does not merely lose the comparison: a
		// sampler2DShadow whose texture has COMPARE_MODE = NONE is INCOMPLETE
		// for that sampler type, and WebGL2 drops the whole draw call rather
		// than sampling it. Warning in the console, nothing from glGetError,
		// and every lit+shadowed mesh silently stops rendering while the grid,
		// the unlit icons and the selection overlay carry on drawing - so the
		// object still has a bounding box, is still selectable, and still
		// highlights, which makes it look like a material bug rather than a
		// dropped draw. Desktop GL never showed it because drivers there
		// sample an incomplete shadow texture instead of refusing.
		GLCHECKER(glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
		GLCHECKER(glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));
	}

	void GLRenderDevice::SetPixelUnpackAlignment(const uint32 value)
	{
		GLCHECKER(glPixelStorei(GL_UNPACK_ALIGNMENT, value));
	}

	void GLRenderDevice::ActivateTextureUnit(const uint32 unit)
	{
		GLCHECKER(glActiveTexture(GL_TEXTURE0 + unit));
	}

	void GLRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer)
	{
#if !defined(GLES3)
		GLCHECKER(glGetTexImage(target, level, format, type, outBuffer));
#endif
	}

	uint32 GLRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height)
	{
#if !defined(GLES3)
		switch (nativeInternalFormat)
		{
		// All three depth formats size by the *read* type, not the storage
		// format: TranslateTextureFormat() pairs every GL_DEPTH_COMPONENT*
		// internal format with GL_FLOAT as the read type on desktop, so
		// glGetTexImage() writes 4 bytes per texel regardless of whether
		// the texture stores 16, 24 or 32 bits. Sizing 16/24 by their
		// storage width (2 and 3 bytes) under-allocated the caller's
		// buffer by width*height bytes and glGetTexImage() wrote past the
		// end of it - a real heap overflow on any depth readback, not a
		// cosmetic rounding quirk. Found via the point-shadow cubemap
		// viewer, whose faces are DepthComponent (-> GL_DEPTH_COMPONENT24)
		// and which read back as all-1.0 because the short buffer was
		// rejected before it could be overflowed.
		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32F:
			return sizeof(f32) * width * height;
		case GL_R16F:
			return sizeof(uchar) * 2 * width * height;
		case GL_R32F:
			return sizeof(f32) * width * height;
		case GL_RG8:
			return sizeof(uchar) * width * height * 2;
		case GL_R16I:
			return sizeof(uchar) * 2 * width * height;
		case GL_R32I:
			return sizeof(int32) * width * height;
		case GL_RG16F:
			return sizeof(uchar) * 2 * width * height * 2;
		case GL_RG32F:
			return sizeof(f32) * width * height * 2;
		case GL_RG16I:
			return sizeof(uchar) * 2 * width * height;
		case GL_RG32I:
			return sizeof(int32) * width * height * 2;
		case GL_RGB8:
			return sizeof(uchar) * width * height * 2;
		case GL_RGB16F:
			return sizeof(uchar) * 2 * width * height * 3;
		case GL_RGB32F:
			return sizeof(f32) * width * height * 3;
		case GL_RGB16I:
			return sizeof(uchar) * 2 * width * height * 3;
		case GL_RGB32I:
			return sizeof(int32) * width * height * 3;
		case GL_RGBA16F:
			return sizeof(uchar) * 2 * width * height * 4;
		case GL_RGBA32F:
			return sizeof(f32) * width * height * 4;
		case GL_RGBA16I:
			return sizeof(uchar) * 2 * width * height * 4;
		case GL_RGBA32I:
			return sizeof(int32) * width * height * 4;
		case GL_R8:
			return sizeof(uchar) * width * height * 4;
		default:
			return sizeof(uchar) * width * height * 4;
		}
#else
		return 0;
#endif
	}

	// Track the draw-bound FBO so ForwardRenderer's swapchain vs offscreen
	// gate matches Vulkan (Island multipass Bind/UnBind).
	DeviceHandle GLRenderDevice::GetCurrentRenderTarget() { return currentDrawFBO; }

	DeviceHandle GLRenderDevice::CreateFramebuffer()
	{
		GLuint id = 0;
		GLCHECKER(glGenFramebuffers(1, &id));
		return id;
	}

	void GLRenderDevice::DestroyFramebuffer(const DeviceHandle fbo)
	{
		GLuint id = fbo;
		GLCHECKER(glDeleteFramebuffers(1, &id));
		if (currentDrawFBO == fbo)
			currentDrawFBO = 0;
	}

	uint32 GLRenderDevice::TranslateFramebufferAccess(const uint32 engineAccess)
	{
		switch (engineAccess)
		{
		case FBOAccess::Read:
			return GL_READ_FRAMEBUFFER;
		case FBOAccess::Write:
			return GL_DRAW_FRAMEBUFFER;
		default:
			return GL_FRAMEBUFFER;
		}
	}

	void GLRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending)
	{
		(void)finalizePending; // no render-pass/attachment-shape concept to defer - see IRenderDevice.h's comment
		GLCHECKER(glBindFramebuffer(nativeAccess, fbo));
		// Read-only binds must not change the draw target (matches Vulkan).
		if (nativeAccess != GL_READ_FRAMEBUFFER)
			currentDrawFBO = fbo;
	}

	uint32 GLRenderDevice::TranslateFramebufferAttachment(const uint32 engineAttachmentFormat)
	{
		switch (engineAttachmentFormat)
		{
		case FrameBufferAttachmentFormat::Color_Attachment0:
			return GL_COLOR_ATTACHMENT0;
		case FrameBufferAttachmentFormat::Color_Attachment1:
			return GL_COLOR_ATTACHMENT1;
		case FrameBufferAttachmentFormat::Color_Attachment2:
			return GL_COLOR_ATTACHMENT2;
		case FrameBufferAttachmentFormat::Color_Attachment3:
			return GL_COLOR_ATTACHMENT3;
		case FrameBufferAttachmentFormat::Color_Attachment4:
			return GL_COLOR_ATTACHMENT4;
		case FrameBufferAttachmentFormat::Color_Attachment5:
			return GL_COLOR_ATTACHMENT5;
		case FrameBufferAttachmentFormat::Color_Attachment6:
			return GL_COLOR_ATTACHMENT6;
		case FrameBufferAttachmentFormat::Color_Attachment7:
			return GL_COLOR_ATTACHMENT7;
		case FrameBufferAttachmentFormat::Color_Attachment8:
			return GL_COLOR_ATTACHMENT8;
		case FrameBufferAttachmentFormat::Color_Attachment9:
			return GL_COLOR_ATTACHMENT9;
		case FrameBufferAttachmentFormat::Color_Attachment10:
			return GL_COLOR_ATTACHMENT10;
		case FrameBufferAttachmentFormat::Color_Attachment11:
			return GL_COLOR_ATTACHMENT11;
		case FrameBufferAttachmentFormat::Color_Attachment12:
			return GL_COLOR_ATTACHMENT12;
		case FrameBufferAttachmentFormat::Color_Attachment13:
			return GL_COLOR_ATTACHMENT13;
		case FrameBufferAttachmentFormat::Color_Attachment14:
			return GL_COLOR_ATTACHMENT14;
		case FrameBufferAttachmentFormat::Color_Attachment15:
			return GL_COLOR_ATTACHMENT15;
		case FrameBufferAttachmentFormat::Depth_Attachment:
			return GL_DEPTH_ATTACHMENT;
		case FrameBufferAttachmentFormat::Stencil_Attachment:
			return GL_STENCIL_ATTACHMENT;
		default:
			return 0;
		}
	}

	void GLRenderDevice::AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound)
	{
		(void)wasAlreadyBound; // no render-pass/attachment-shape concept to defer - see IRenderDevice.h's comment
		GLCHECKER(glFramebufferTexture2D(GL_FRAMEBUFFER, nativeAttachmentFormat, nativeTextureTarget, textureId, 0));
	}

	void GLRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer)
	{
		GLCHECKER(glFramebufferRenderbuffer(GL_FRAMEBUFFER, nativeAttachmentFormat, GL_RENDERBUFFER, renderbuffer));
	}

	void GLRenderDevice::SetDrawBufferNone()
	{
#if !defined(GLES3)
		GLCHECKER(glDrawBuffer(GL_NONE));
#endif
	}

	void GLRenderDevice::SetReadBufferNone()
	{
#if !defined(GLES3)
		GLCHECKER(glReadBuffer(GL_NONE));
#endif
	}

	void GLRenderDevice::SetDrawBufferBack()
	{
#if !defined(GLES3)
		GLCHECKER(glDrawBuffer(GL_BACK));
#endif
	}

	void GLRenderDevice::SetReadBufferBack()
	{
#if !defined(GLES3)
		GLCHECKER(glReadBuffer(GL_BACK));
#endif
	}

	void GLRenderDevice::SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices)
	{
		// NOT guarded for GLES3, unlike its glDrawBuffer (singular) siblings
		// above: glDrawBuffers is core in OpenGL ES 3.0 and WebGL2. While it
		// was a no-op there, an MRT framebuffer kept its default state of
		// draw buffer 0 = COLOR_ATTACHMENT0 and the rest NONE, so the G-buffer
		// was neither written nor CLEARED past its first attachment and the
		// deferred path composited every frame over the last one.
		if (colorAttachmentIndices.empty())
			return;
		std::vector<GLenum> BufferIDs;
		for (std::vector<uint32>::const_iterator i = colorAttachmentIndices.begin(); i != colorAttachmentIndices.end(); i++)
			BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + *i);
		GLCHECKER(glDrawBuffers((GLsizei)BufferIDs.size(), &BufferIDs[0]));
	}

	uint32 GLRenderDevice::CheckFramebufferStatus()
	{
		return glCheckFramebufferStatus(GL_FRAMEBUFFER);
	}

	uint32 GLRenderDevice::TranslateFramebufferStatus(const uint32 nativeStatus)
	{
		switch (nativeStatus)
		{
		case GL_FRAMEBUFFER_COMPLETE:
			return FBOStatus::Complete;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			return FBOStatus::IncompleteAttachment;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			return FBOStatus::IncompleteMissingAttachment;
#if !defined(GLES3)
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			return FBOStatus::IncompleteDrawBuffer;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			return FBOStatus::IncompleteReadBuffer;
#endif
		case GL_FRAMEBUFFER_UNSUPPORTED:
			return FBOStatus::Unsupported;
		default:
			return FBOStatus::Unknown;
		}
	}

	DeviceHandle GLRenderDevice::CreateRenderbuffer()
	{
		GLuint id = 0;
		GLCHECKER(glGenRenderbuffers(1, &id));
		return id;
	}

	void GLRenderDevice::DestroyRenderbuffer(const DeviceHandle rbo)
	{
		GLuint id = rbo;
		GLCHECKER(glDeleteRenderbuffers(1, &id));
	}

	void GLRenderDevice::BindRenderbuffer(const DeviceHandle rbo)
	{
		GLCHECKER(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
	}

	uint32 GLRenderDevice::TranslateRenderbufferFormat(const uint32 engineDataType)
	{
		switch (engineDataType)
		{
		case RenderBufferDataType::Depth:
		case RenderBufferDataType::Depth_Multisample:
#if defined(GLES3)
			return GL_DEPTH_COMPONENT16;
#else
			return GL_DEPTH_COMPONENT;
#endif
		case RenderBufferDataType::RGBA:
		case RenderBufferDataType::RGBA_Multisample:
		default:
			return GL_RGBA;
		}
	}

	void GLRenderDevice::RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height)
	{
		GLCHECKER(glRenderbufferStorage(GL_RENDERBUFFER, nativeFormat, width, height));
	}

	void GLRenderDevice::RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height)
	{
		GLCHECKER(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, nativeFormat, width, height));
	}

	void GLRenderDevice::SetMultisampleEnabled(const bool enabled)
	{
#if !defined(GLES3)
		if (enabled) {
			GLCHECKER(glEnable(GL_MULTISAMPLE));
		}
		else {
			GLCHECKER(glDisable(GL_MULTISAMPLE));
		}
#endif
	}

	void GLRenderDevice::BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter)
	{
		GLuint Mask;
		switch (engineMask)
		{
		case FBOBufferBit::Depth:
			Mask = GL_DEPTH_BUFFER_BIT;
			break;
		case FBOBufferBit::Stencil:
			Mask = GL_STENCIL_BUFFER_BIT;
			break;
		case FBOBufferBit::Color:
		default:
			Mask = GL_COLOR_BUFFER_BIT;
			break;
		};
		GLuint Filter;
		switch (engineFilter)
		{
		case FBOFilter::Nearest:
			Filter = GL_NEAREST;
			break;
		case FBOFilter::Linear:
		default:
			Filter = GL_LINEAR;
			break;
		};
		GLCHECKER(glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, Mask, Filter));
	}

	void GLRenderDevice::CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height)
	{
		// See IRenderDevice.h's comment on CopyDepthTexture - GL doesn't
		// actually need this (aliasing one depth texture across two FBOs
		// is fine here), but the interface is shared, so this still has
		// to do a real copy for whichever caller (DeferredRenderer)
		// expects `dstTexture` to hold `srcTexture`'s current contents
		// afterward. A throwaway read/draw FBO pair + glBlitFramebuffer,
		// torn down immediately - correctness over performance, matches
		// GLRenderDevice's existing bar for infrequent (once-per-frame,
		// not once-per-object) operations.
		GLuint fbos[2] = { 0, 0 };
		GLCHECKER(glGenFramebuffers(2, fbos));
		GLint prevReadFBO = 0, prevDrawFBO = 0;
		GLCHECKER(glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO));
		GLCHECKER(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO));

		GLCHECKER(glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]));
		GLCHECKER(glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (GLuint)srcTexture, 0));
		GLCHECKER(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]));
		GLCHECKER(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (GLuint)dstTexture, 0));

		GLCHECKER(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST));

		GLCHECKER(glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevReadFBO));
		GLCHECKER(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDrawFBO));
		GLCHECKER(glDeleteFramebuffers(2, fbos));
	}

};
