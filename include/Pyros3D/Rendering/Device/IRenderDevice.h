//============================================================================
// Name        : IRenderDevice.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Backend-agnostic seam for the state/bind/draw calls
//               IRenderer (and friends) used to issue directly against
//               OpenGL. GLRenderDevice is the only implementation today;
//               see VULKAN_ROADMAP.md for the motivation and a future
//               VulkanRenderDevice.
//============================================================================

#ifndef IRENDERDEVICE_H
#define IRENDERDEVICE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <vector>

namespace p3d {

	namespace Buffer_Bit
	{
		enum {
			None = 0,
			Color = 0x10,
			Depth = 0x20,
			Stencil = 0x40
		};
	}

	namespace StencilOp
	{
		enum {
			Keep = 0,
			Zero,
			Replace,
			Incr,
			Incr_Wrap,
			Decr,
			Decr_Wrap,
			Invert
		};
	}

	namespace StencilFunc
	{
		enum {
			Always = 0,
			Never,
			Less,
			LEqual,
			Greater,
			GEqual,
			Equal,
			Notequal
		};
	}

	namespace BlendFunc
	{
		enum {
			Zero = 0,
			One,
			Src_Color,
			One_Minus_Src_Color,
			Dst_Color,
			One_Minus_Dst_Color,
			Src_Alpha,
			One_Minus_Src_Alpha,
			Dst_Alpha,
			One_Minus_Dst_Alpha,
			Constant_Color,
			One_Minus_Constant_Color,
			Constant_Alpha,
			One_Minus_Constant_Alpha,
			Src_Alpha_Saturate,
			Src1_Color,
			One_Minus_Src1_Color,
			Src1_Alpha,
			One_Minus_Src1_Alpha
		};
	}

	namespace BlendEq
	{
		enum {
			Add = 0,
			Subtract,
			Reverse_Subtract
		};
	}

	namespace DepthTest
	{
		enum {
			Less = 0,
			Never,
			Greater,
			Equal,
			Always,
			LEqual,
			GEqual,
			NotEqual
		};
	}

	// Opaque handle returned by resource-creation calls below. Same
	// underlying type as the GL handles this replaces (uint32) - resource
	// creation itself (Texture/FrameBuffer/GeometryBuffer/Shader) is not
	// part of this seam yet, only the state/bind/draw calls IRenderer used
	// to issue directly. See VULKAN_ROADMAP.md Phase 3.
	typedef uint32 DeviceHandle;

	// Backend-agnostic seam for state changes, resource binding, and draw
	// calls. One instance per backend choice (today: GLRenderDevice only),
	// constructed by IRenderer/DebugRenderer/PostEffectsManager. Every
	// method here is a mechanical 1:1 stand-in for a GL call sequence that
	// used to live directly in those classes - behavior is meant to be
	// byte-for-byte identical to before this seam existed.
	class PYROS3D_API IRenderDevice {

	public:

		virtual ~IRenderDevice() {}

		// Clearing - TranslateBufferBit() has no side effects (pure
		// translation, cached by the caller); Clear() issues the actual
		// clear using a previously-translated mask.
		virtual uint32 TranslateBufferBit(const uint32 bufferBits) = 0;
		virtual void Clear(const uint32 nativeBufferBits) = 0;
		virtual void SetClearColor(const Vec4 &color) = 0;

		// Depth
		virtual void SetDepthTest(const bool enabled, const uint32 mode) = 0;
		virtual void SetDepthMask(const bool enabled) = 0;
		// glDepthMask(GL_TRUE) + glClearDepth(1.f) - a no-op on GLES3,
		// matching ClearDepthBuffer()'s existing #if !defined(GLES3) guard.
		virtual void PrepareDepthClear() = 0;

		// Stencil
		virtual void SetStencilTestEnabled(const bool enabled) = 0;
		virtual void SetClearStencilValue() = 0;
		virtual void SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask) = 0;
		virtual void SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass) = 0;

		// Scissor
		virtual void SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height) = 0;
		virtual void SetScissorTestEnabled(const bool enabled) = 0;

		// Wireframe (no-op on GLES3, matching Enable/DisableWireFrame()'s
		// existing #if !defined(GLES3) guard)
		virtual void SetWireFrame(const bool enabled) = 0;

		// Color
		virtual void SetColorMask(const bool r, const bool g, const bool b, const bool a) = 0;

		// Depth bias / polygon offset
		virtual void SetPolygonOffsetEnabled(const bool enabled) = 0;
		virtual void SetPolygonOffset(const f32 factor, const f32 units) = 0;

		// Blending
		virtual void SetBlendingEnabled(const bool enabled) = 0;
		virtual void SetBlendFunction(const uint32 sfactor, const uint32 dfactor) = 0;
		virtual void SetBlendEquation(const uint32 mode) = 0;

		// Cull face - cullFace is one of IMaterial.h's CullFace::BackFace/
		// FrontFace/DoubleSided values.
		virtual void SetCullFaceMode(const uint32 cullFace) = 0;
		virtual void DisableCullFace() = 0;

		// Clip distances (no-op on GLES3, matching StartClippingPlanes()/
		// EndClippingPlanes()'s existing #if !defined(GLES3) guard)
		virtual void EnableClipDistance(const uint32 index) = 0;
		virtual void DisableClipDistance(const uint32 index) = 0;

		// Viewport
		virtual void SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height) = 0;

		// Program / vertex state
		virtual void UseProgram(const uint32 program) = 0;
		virtual DeviceHandle CreateVertexArray() = 0;
		virtual void DeleteVertexArray(const DeviceHandle vao) = 0;
		virtual void BindVertexArray(const DeviceHandle vao) = 0;
		virtual void BindArrayBuffer(const uint32 buffer) = 0;
		virtual void BindElementBuffer(const uint32 buffer) = 0;
		// typeCount/nativeType come from the engine's existing
		// Buffer::Attribute::GetTypeCount()/GetType() helpers
		// (GeometryBuffer.h) - nativeType is already a backend-native
		// token by the time it reaches here (a GL_* enum today), passed
		// through unmodified since GeometryBuffer's internals aren't part
		// of this seam yet (see VULKAN_ROADMAP.md Phase 3).
		virtual void SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) = 0;
		// Same as SetVertexAttribute(), but for the common case (DebugRenderer,
		// PostEffectsManager) of a plain float/vecN attribute with no
		// engine-side type token available - the device fills in its own
		// native "float" type.
		virtual void SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) = 0;
		virtual void DisableVertexAttribute(const int32 location) = 0;
		virtual void SetVertexAttributeDivisor(const int32 location, const uint32 divisor) = 0;
		// glGetUniformBlockIndex + glUniformBlockBinding, no-op if the
		// shader doesn't declare a block with this name.
		virtual void BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint) = 0;

		// Draw - engineDrawType is one of RenderingComponent.h's
		// DrawingType::Triangles/Lines/... values.
		virtual uint32 TranslateDrawType(const uint32 engineDrawType) = 0;
		virtual void DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count) = 0;
		virtual void DrawElements(const uint32 nativeDrawType, const uint32 indexCount) = 0;
		virtual void DrawElementsInstanced(const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount) = 0;

		// Uniform buffers - CreateUniformBuffer mirrors what IRenderer's
		// constructor used to do inline for each shared UBO (gen, bind,
		// allocate, bind to binding point, unbind).
		virtual DeviceHandle CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint) = 0;
		virtual void UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data) = 0;
		virtual void DestroyUniformBuffer(const DeviceHandle buffer) = 0;

		// Geometry/vertex/index buffers - bufferType/bufferDraw/mappingType
		// are GeometryBuffer.h's Buffer::Type::Index/Vertex/Attribute,
		// Buffer::Draw::Static/Dynamic/Stream, and
		// Buffer::Mapping::Read/Write/ReadAndWrite values respectively.
		virtual DeviceHandle CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) = 0;
		// Full reallocation (glBufferData) - used when new data is a
		// different size than what's currently allocated.
		virtual void ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) = 0;
		// In-place update (glBufferSubData) - same size as before.
		virtual void UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length) = 0;
		virtual void DestroyBuffer(const DeviceHandle buffer) = 0;
		virtual void *MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType) = 0;
		virtual void UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) = 0;

		// Vertex attribute component type translation - engineType is one
		// of GeometryBuffer.h's Buffer::Attribute::Type::Int/Short/Float/
		// Vec2/Vec3/Vec4/Matrix values (matches SetVertexAttribute's
		// nativeType param above).
		virtual uint32 TranslateAttributeType(const uint32 engineType) = 0;

		// Shader compilation/linking - engineShaderType is one of
		// Shaders.h's ShaderType::VertexShader/FragmentShader/
		// GeometryShader values. BuildShaderSource assembles the
		// backend/profile-specific prefix (a `#version` line for GL) ahead
		// of definitions + the shader body - this is the seam Phase 2's
		// GLSL->SPIR-V cross-compilation will replace/extend.
		virtual std::string BuildShaderSource(const std::string &definitions, const std::string &shaderBody) = 0;
		virtual DeviceHandle CreateShaderStage(const uint32 engineShaderType) = 0;
		// Returns false and fills errorLog on failure (errorLog may be
		// left untouched if the log has fewer than 2 bytes, matching the
		// original "length > 1" check).
		virtual bool CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog) = 0;
		virtual DeviceHandle CreateProgram() = 0;
		virtual void AttachShaderStage(const DeviceHandle program, const DeviceHandle shader) = 0;
		virtual bool LinkProgram(const DeviceHandle program, std::string &errorLog) = 0;
		virtual bool IsProgram(const DeviceHandle id) = 0;
		virtual bool IsShaderStage(const DeviceHandle id) = 0;
		virtual void DetachShaderStage(const DeviceHandle program, const DeviceHandle shader) = 0;
		virtual void DeleteShaderStage(const DeviceHandle shader) = 0;
		virtual void DeleteProgram(const DeviceHandle program) = 0;

		virtual int32 GetUniformLocation(const uint32 program, const std::string &name) = 0;
		virtual int32 GetAttributeLocation(const uint32 program, const std::string &name) = 0;

		// One method per Uniforms::DataType (Uniforms.h) - matches the
		// glUniform*v overload set Shader::SendUniform() used to dispatch
		// to directly.
		virtual void SendUniformInt(const int32 handle, const int32 *data, const uint32 count) = 0;
		virtual void SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) = 0;

		// Texture translation - engineDataType/engineTextureType are
		// Texture.h's TextureDataType::*/TextureType::* values.
		// TranslateTextureFormat mirrors what Texture::GetInternalFormat()
		// used to compute inline; TranslateTextureTarget mirrors
		// Texture::GetGLModes().
		virtual void TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type) = 0;
		virtual void TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode) = 0;

		// Texture object lifecycle
		virtual DeviceHandle CreateTextureObject() = 0;
		virtual void DestroyTextureObject(const DeviceHandle texture) = 0;
		virtual void BindTextureToTarget(const uint32 target, const DeviceHandle texture) = 0;

		// Texture upload - internalFormat/format/type are the native
		// tokens TranslateTextureFormat() produced.
		virtual void UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data) = 0;
		virtual void UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) = 0;
		virtual void GenerateMipmap(const uint32 target) = 0;

		// Texture parameters - engineRepeat/engineFilter are Texture.h's
		// TextureRepeat::*/TextureFilter::* values. SetTextureWrapR and
		// SetTextureCompareMode are no-ops on GLES3, matching the
		// original #if !defined(GLES3) guards around those call sites.
		virtual void SetTextureWrapS(const uint32 target, const uint32 engineRepeat) = 0;
		virtual void SetTextureWrapT(const uint32 target, const uint32 engineRepeat) = 0;
		virtual void SetTextureWrapR(const uint32 target, const uint32 engineRepeat) = 0;
		virtual void SetTextureMagFilter(const uint32 target, const uint32 engineFilter) = 0;
		virtual void SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap) = 0;
		virtual void SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) = 0;
		virtual void SetTextureBorderColor(const uint32 target, const Vec4 &color) = 0;
		virtual void SetTextureCompareMode(const uint32 target) = 0;
		virtual void SetPixelUnpackAlignment(const uint32 value) = 0;

		// Texture units
		virtual void ActivateTextureUnit(const uint32 unit) = 0;

		// Readback - returns the byte size the caller should have already
		// sized `outBuffer` to (see GetTextureDataSize below); a no-op on
		// GLES3, matching Texture::GetTextureData()'s original
		// #if !defined(GLES3) guard.
		virtual void ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) = 0;
		// Mirrors the byte-size-per-format switch that used to live
		// directly in Texture::GetTextureData() (including its existing
		// quirks/rounding, e.g. RGB8 sizing as if 2 bytes/pixel not 3 -
		// preserved as-is rather than "fixed" during this mechanical move).
		virtual uint32 GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) = 0;

		// Framebuffers/renderbuffers - engine enum params are
		// FrameBuffer.h's FBOAccess::*/FrameBufferAttachmentFormat::*/
		// RenderBufferDataType::*/FBOBufferBit::*/FBOFilter::* values.
		// Attachment texture targets reuse TranslateTextureTarget()'s
		// `mode` output (same GL_TEXTURE_CUBE_MAP_*/GL_TEXTURE_2D/
		// GL_TEXTURE_2D_MULTISAMPLE translation table as Texture.cpp).
		virtual DeviceHandle CreateFramebuffer() = 0;
		virtual void DestroyFramebuffer(const DeviceHandle fbo) = 0;
		virtual uint32 TranslateFramebufferAccess(const uint32 engineAccess) = 0;
		virtual void BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo) = 0;
		virtual uint32 TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) = 0;
		virtual void AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId) = 0;
		virtual void AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) = 0;
		// No-ops on GLES3, matching the original #if !defined(GLES3) guards.
		virtual void SetDrawBufferNone() = 0;
		virtual void SetReadBufferNone() = 0;
		virtual void SetDrawBufferBack() = 0;
		virtual void SetReadBufferBack() = 0;
		// colorAttachmentIndices are 0-based indices into the
		// Color_Attachment0.. sequence (the device adds the
		// GL_COLOR_ATTACHMENT0 base internally).
		virtual void SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) = 0;
		virtual uint32 CheckFramebufferStatus() = 0;
		// Translates a raw status (as returned by CheckFramebufferStatus())
		// into FrameBuffer.h's FBOStatus::* values.
		virtual uint32 TranslateFramebufferStatus(const uint32 nativeStatus) = 0;

		virtual DeviceHandle CreateRenderbuffer() = 0;
		virtual void DestroyRenderbuffer(const DeviceHandle rbo) = 0;
		virtual void BindRenderbuffer(const DeviceHandle rbo) = 0;
		// engineDataType is RenderBufferDataType::Depth/Depth_Multisample/
		// RGBA/RGBA_Multisample/etc (the *_Multisample suffix doesn't
		// affect the translated format, only which of
		// RenderbufferStorage/RenderbufferStorageMultisample below the
		// caller goes on to use). FBOAttachment::DataType is overwritten
		// with this native token afterward (see FrameBuffer::Resize(),
		// which reuses it directly).
		virtual uint32 TranslateRenderbufferFormat(const uint32 engineDataType) = 0;
		// Both take an already-native format (from
		// TranslateRenderbufferFormat() or a previously-stored
		// FBOAttachment::DataType).
		virtual void RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height) = 0;
		virtual void RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height) = 0;

		virtual void SetMultisampleEnabled(const bool enabled) = 0;
		virtual void BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter) = 0;

	};

};

#endif /* IRENDERDEVICE_H */
