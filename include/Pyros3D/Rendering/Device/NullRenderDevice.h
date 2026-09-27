//============================================================================
// Name        : NullRenderDevice.h
// Author      : Duarte Peixinho
// Description : A render device that renders nothing - for a headless
//               dedicated server, which loads the same scenes a client does
//               (meshes, materials, textures and all) but never draws them.
//
//               Every call is a no-op except where the engine reads back an
//               answer: handles are unique and non-zero, shaders "compile"
//               and "link", and a mapped buffer is real memory. Written
//               against IRenderDevice's full list, so a new pure virtual
//               there fails the build here rather than silently.
//============================================================================

#ifndef NULLRENDERDEVICE_H
#define NULLRENDERDEVICE_H

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <map>
#include <vector>

namespace p3d {

	class PYROS3D_API NullRenderDevice : public IRenderDevice
	{
	public:
		NullRenderDevice() {}
		virtual ~NullRenderDevice() {}

		virtual CommandBufferHandle BeginCommandBuffer() override;
		virtual void EndCommandBuffer(const CommandBufferHandle cmd) override;
		virtual void BeginFrame() override;
		virtual void EndFrame() override;
		virtual void WaitIdle() override;
		virtual bool IsVulkan() const override;
		virtual uint32 TranslateBufferBit(const uint32 bufferBits) override;
		virtual void Clear(const uint32 nativeBufferBits) override;
		virtual void SetClearColor(const Vec4 &color) override;
		virtual void SetDepthTest(const bool enabled, const uint32 mode) override;
		virtual void SetDepthMask(const bool enabled) override;
		virtual void PrepareDepthClear() override;
		virtual void SetStencilTestEnabled(const bool enabled) override;
		virtual void SetClearStencilValue() override;
		virtual void SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask) override;
		virtual void SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass) override;
		virtual void SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height) override;
		virtual void SetScissorTestEnabled(const bool enabled) override;
		virtual void SetWireFrame(const bool enabled) override;
		virtual void SetColorMask(const bool r, const bool g, const bool b, const bool a) override;
		virtual void SetPolygonOffsetEnabled(const bool enabled) override;
		virtual void SetPolygonOffset(const f32 factor, const f32 units) override;
		virtual void SetBlendingEnabled(const bool enabled) override;
		virtual void SetBlendFunction(const uint32 sfactor, const uint32 dfactor) override;
		virtual void SetBlendEquation(const uint32 mode) override;
		virtual void SetCullFaceMode(const uint32 cullFace) override;
		virtual void DisableCullFace() override;
		virtual DeviceHandle CreatePipeline(const PipelineDesc &desc) override;
		virtual void DestroyPipeline(const DeviceHandle pipeline) override;
		virtual void BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline) override;
		virtual void EnableClipDistance(const uint32 index) override;
		virtual void DisableClipDistance(const uint32 index) override;
		virtual void SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height) override;
		virtual void UseProgram(const uint32 program) override;
		virtual DeviceHandle CreateVertexArray() override;
		virtual void DeleteVertexArray(const DeviceHandle vao) override;
		virtual void BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao) override;
		virtual void BindArrayBuffer(const uint32 buffer) override;
		virtual void BindElementBuffer(const uint32 buffer) override;
		virtual void SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) override;
		virtual void SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) override;
		virtual void DisableVertexAttribute(const int32 location) override;
		virtual void SetVertexAttributeDivisor(const int32 location, const uint32 divisor) override;
		virtual void BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint, const DeviceHandle bufferHandle = 0) override;
		virtual Matrix TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip = false) override;
		virtual void SetPointShadowCubeFacePass(const bool enabled) override;
		virtual Matrix TranslateShadowBiasMatrix() override;
		virtual uint32 TranslateDrawType(const uint32 engineDrawType) override;
		virtual void DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count) override;
		virtual void DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount) override;
		virtual void DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount) override;
		virtual DeviceHandle CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint) override;
		virtual void UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data) override;
		virtual void ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data) override;
		virtual void DestroyUniformBuffer(const DeviceHandle buffer) override;
		virtual DeviceHandle CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) override;
		virtual void ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) override;
		virtual void UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length) override;
		virtual void DestroyBuffer(const DeviceHandle buffer) override;
		virtual void* MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType) override;
		virtual void UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) override;
		virtual uint32 TranslateAttributeType(const uint32 engineType) override;
		virtual std::string BuildShaderSource(const std::string &definitions, const std::string &shaderBody) override;
		virtual DeviceHandle CreateShaderStage(const uint32 engineShaderType) override;
		virtual bool CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog) override;
		virtual DeviceHandle CreateProgram() override;
		virtual void AttachShaderStage(const DeviceHandle program, const DeviceHandle shader) override;
		virtual bool LinkProgram(const DeviceHandle program, std::string &errorLog) override;
		virtual bool IsProgram(const DeviceHandle id) override;
		virtual bool IsShaderStage(const DeviceHandle id) override;
		virtual void DetachShaderStage(const DeviceHandle program, const DeviceHandle shader) override;
		virtual void DeleteShaderStage(const DeviceHandle shader) override;
		virtual void DeleteProgram(const DeviceHandle program) override;
		virtual int32 GetUniformLocation(const uint32 program, const std::string &name) override;
		virtual int32 GetAttributeLocation(const uint32 program, const std::string &name) override;
		virtual void SendUniformInt(const int32 handle, const int32 *data, const uint32 count) override;
		virtual void SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) override;
		virtual void SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) override;
		virtual void SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) override;
		virtual void SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) override;
		virtual void SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) override;
		virtual void TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type) override;
		virtual void TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode) override;
		virtual DeviceHandle CreateTextureObject() override;
		virtual void DestroyTextureObject(const DeviceHandle texture) override;
		virtual void BindTextureToTarget(const uint32 target, const DeviceHandle texture) override;
		virtual void UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap) override;
		virtual void UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) override;
		virtual void GenerateMipmap(const uint32 target) override;
		virtual void SetTextureWrapS(const uint32 target, const uint32 engineRepeat) override;
		virtual void SetTextureWrapT(const uint32 target, const uint32 engineRepeat) override;
		virtual void SetTextureWrapR(const uint32 target, const uint32 engineRepeat) override;
		virtual void SetTextureMagFilter(const uint32 target, const uint32 engineFilter) override;
		virtual void SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap) override;
		virtual void SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) override;
		virtual void SetTextureBorderColor(const uint32 target, const Vec4 &color) override;
		virtual void SetTextureCompareMode(const uint32 target) override;
		virtual void SetPixelUnpackAlignment(const uint32 value) override;
		virtual void ActivateTextureUnit(const uint32 unit) override;
		virtual void ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) override;
		virtual uint32 GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) override;
		virtual DeviceHandle GetCurrentRenderTarget() override;
		virtual DeviceHandle CreateFramebuffer() override;
		virtual void DestroyFramebuffer(const DeviceHandle fbo) override;
		virtual void SetFramebufferPreserveDepth(const DeviceHandle fbo, const bool preserve) override;
		virtual uint32 TranslateFramebufferAccess(const uint32 engineAccess) override;
		virtual void BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending) override;
		virtual uint32 TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) override;
		virtual void AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound) override;
		virtual void AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) override;
		virtual void SetDrawBufferNone() override;
		virtual void SetReadBufferNone() override;
		virtual void SetDrawBufferBack() override;
		virtual void SetReadBufferBack() override;
		virtual void SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) override;
		virtual uint32 CheckFramebufferStatus() override;
		virtual uint32 TranslateFramebufferStatus(const uint32 nativeStatus) override;
		virtual DeviceHandle CreateRenderbuffer() override;
		virtual void DestroyRenderbuffer(const DeviceHandle rbo) override;
		virtual void BindRenderbuffer(const DeviceHandle rbo) override;
		virtual uint32 TranslateRenderbufferFormat(const uint32 engineDataType) override;
		virtual void RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height) override;
		virtual void RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height) override;
		virtual void SetMultisampleEnabled(const bool enabled) override;
		virtual void BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter) override;
		virtual void CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height) override;

	private:
		DeviceHandle NextHandle() { return ++lastHandle; }
		void CreateScratch(const DeviceHandle h, const uint32 bytes) { scratch[h].assign(bytes, 0); }
		void* MapScratch(const DeviceHandle h)
		{
			std::vector<uchar> &b = scratch[h];
			if (b.empty()) b.resize(64 * 1024);
			return &b[0];
		}
		DeviceHandle lastHandle = 0;
		std::map<DeviceHandle, std::vector<uchar> > scratch;
	};

}

#endif /* NULLRENDERDEVICE_H */
