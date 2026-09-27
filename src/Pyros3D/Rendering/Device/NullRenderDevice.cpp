//============================================================================
// Name        : NullRenderDevice.cpp
// Author      : Duarte Peixinho
// Description : See NullRenderDevice.h.
//============================================================================

#include <Pyros3D/Rendering/Device/NullRenderDevice.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

namespace p3d {

	CommandBufferHandle NullRenderDevice::BeginCommandBuffer() { return 1; }
	void NullRenderDevice::EndCommandBuffer(const CommandBufferHandle cmd) {  }
	void NullRenderDevice::BeginFrame() {  }
	void NullRenderDevice::EndFrame() {  }
	void NullRenderDevice::WaitIdle() {  }
	bool NullRenderDevice::IsVulkan() const { return false; }
	uint32 NullRenderDevice::TranslateBufferBit(const uint32 bufferBits) { return 0; }
	void NullRenderDevice::Clear(const uint32 nativeBufferBits) {  }
	void NullRenderDevice::SetClearColor(const Vec4 &color) {  }
	void NullRenderDevice::SetDepthTest(const bool enabled, const uint32 mode) {  }
	void NullRenderDevice::SetDepthMask(const bool enabled) {  }
	void NullRenderDevice::PrepareDepthClear() {  }
	void NullRenderDevice::SetStencilTestEnabled(const bool enabled) {  }
	void NullRenderDevice::SetClearStencilValue() {  }
	void NullRenderDevice::SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask) {  }
	void NullRenderDevice::SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass) {  }
	void NullRenderDevice::SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height) {  }
	void NullRenderDevice::SetScissorTestEnabled(const bool enabled) {  }
	void NullRenderDevice::SetWireFrame(const bool enabled) {  }
	void NullRenderDevice::SetColorMask(const bool r, const bool g, const bool b, const bool a) {  }
	void NullRenderDevice::SetPolygonOffsetEnabled(const bool enabled) {  }
	void NullRenderDevice::SetPolygonOffset(const f32 factor, const f32 units) {  }
	void NullRenderDevice::SetBlendingEnabled(const bool enabled) {  }
	void NullRenderDevice::SetBlendFunction(const uint32 sfactor, const uint32 dfactor) {  }
	void NullRenderDevice::SetBlendEquation(const uint32 mode) {  }
	void NullRenderDevice::SetCullFaceMode(const uint32 cullFace) {  }
	void NullRenderDevice::DisableCullFace() {  }
	DeviceHandle NullRenderDevice::CreatePipeline(const PipelineDesc &desc) { return NextHandle(); }
	void NullRenderDevice::DestroyPipeline(const DeviceHandle pipeline) {  }
	void NullRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline) {  }
	void NullRenderDevice::EnableClipDistance(const uint32 index) {  }
	void NullRenderDevice::DisableClipDistance(const uint32 index) {  }
	void NullRenderDevice::SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height) {  }
	void NullRenderDevice::UseProgram(const uint32 program) {  }
	DeviceHandle NullRenderDevice::CreateVertexArray() { return NextHandle(); }
	void NullRenderDevice::DeleteVertexArray(const DeviceHandle vao) {  }
	void NullRenderDevice::BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao) {  }
	void NullRenderDevice::BindArrayBuffer(const uint32 buffer) {  }
	void NullRenderDevice::BindElementBuffer(const uint32 buffer) {  }
	void NullRenderDevice::SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) {  }
	void NullRenderDevice::SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) {  }
	void NullRenderDevice::DisableVertexAttribute(const int32 location) {  }
	void NullRenderDevice::SetVertexAttributeDivisor(const int32 location, const uint32 divisor) {  }
	void NullRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint, const DeviceHandle bufferHandle) {  }
	Matrix NullRenderDevice::TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip) { return projectionMatrix; }
	void NullRenderDevice::SetPointShadowCubeFacePass(const bool enabled) {  }
	Matrix NullRenderDevice::TranslateShadowBiasMatrix() { Matrix m; m.identity(); return m; }
	uint32 NullRenderDevice::TranslateDrawType(const uint32 engineDrawType) { return 0; }
	void NullRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count) {  }
	void NullRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount) {  }
	void NullRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount) {  }
	DeviceHandle NullRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint) { return NextHandle(); }
	void NullRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data) {  }
	void NullRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data) {  }
	void NullRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer) {  }
	DeviceHandle NullRenderDevice::CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) { const DeviceHandle h = NextHandle(); CreateScratch(h, length); return h; }
	void NullRenderDevice::ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) {  }
	void NullRenderDevice::UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length) {  }
	void NullRenderDevice::DestroyBuffer(const DeviceHandle buffer) { scratch.erase(buffer); }
	void* NullRenderDevice::MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType) { return MapScratch(buffer); }
	void NullRenderDevice::UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) {  }
	uint32 NullRenderDevice::TranslateAttributeType(const uint32 engineType) { return 0; }
	std::string NullRenderDevice::BuildShaderSource(const std::string &definitions, const std::string &shaderBody) { return definitions + shaderBody; }
	DeviceHandle NullRenderDevice::CreateShaderStage(const uint32 engineShaderType) { return NextHandle(); }
	bool NullRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog) { return true; }
	DeviceHandle NullRenderDevice::CreateProgram() { return NextHandle(); }
	void NullRenderDevice::AttachShaderStage(const DeviceHandle program, const DeviceHandle shader) {  }
	bool NullRenderDevice::LinkProgram(const DeviceHandle program, std::string &errorLog) { return true; }
	bool NullRenderDevice::IsProgram(const DeviceHandle id) { return true; }
	bool NullRenderDevice::IsShaderStage(const DeviceHandle id) { return true; }
	void NullRenderDevice::DetachShaderStage(const DeviceHandle program, const DeviceHandle shader) {  }
	void NullRenderDevice::DeleteShaderStage(const DeviceHandle shader) {  }
	void NullRenderDevice::DeleteProgram(const DeviceHandle program) {  }
	int32 NullRenderDevice::GetUniformLocation(const uint32 program, const std::string &name) { return 0; }
	int32 NullRenderDevice::GetAttributeLocation(const uint32 program, const std::string &name) { return 0; }
	void NullRenderDevice::SendUniformInt(const int32 handle, const int32 *data, const uint32 count) {  }
	void NullRenderDevice::SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) {  }
	void NullRenderDevice::SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) {  }
	void NullRenderDevice::SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) {  }
	void NullRenderDevice::SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) {  }
	void NullRenderDevice::SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) {  }
	void NullRenderDevice::TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type) {  }
	void NullRenderDevice::TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode) {  }
	DeviceHandle NullRenderDevice::CreateTextureObject() { return NextHandle(); }
	void NullRenderDevice::DestroyTextureObject(const DeviceHandle texture) {  }
	void NullRenderDevice::BindTextureToTarget(const uint32 target, const DeviceHandle texture) {  }
	void NullRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap) {  }
	void NullRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) {  }
	void NullRenderDevice::GenerateMipmap(const uint32 target) {  }
	void NullRenderDevice::SetTextureWrapS(const uint32 target, const uint32 engineRepeat) {  }
	void NullRenderDevice::SetTextureWrapT(const uint32 target, const uint32 engineRepeat) {  }
	void NullRenderDevice::SetTextureWrapR(const uint32 target, const uint32 engineRepeat) {  }
	void NullRenderDevice::SetTextureMagFilter(const uint32 target, const uint32 engineFilter) {  }
	void NullRenderDevice::SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap) {  }
	void NullRenderDevice::SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) {  }
	void NullRenderDevice::SetTextureBorderColor(const uint32 target, const Vec4 &color) {  }
	void NullRenderDevice::SetTextureCompareMode(const uint32 target) {  }
	void NullRenderDevice::SetPixelUnpackAlignment(const uint32 value) {  }
	void NullRenderDevice::ActivateTextureUnit(const uint32 unit) {  }
	void NullRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) {  }
	uint32 NullRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) { return 0; }
	DeviceHandle NullRenderDevice::GetCurrentRenderTarget() { return 0; }
	DeviceHandle NullRenderDevice::CreateFramebuffer() { return NextHandle(); }
	void NullRenderDevice::DestroyFramebuffer(const DeviceHandle fbo) {  }
	void NullRenderDevice::SetFramebufferPreserveDepth(const DeviceHandle fbo, const bool preserve) {  }
	uint32 NullRenderDevice::TranslateFramebufferAccess(const uint32 engineAccess) { return 0; }
	void NullRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending) {  }
	uint32 NullRenderDevice::TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) { return 0; }
	void NullRenderDevice::AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound) {  }
	void NullRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) {  }
	void NullRenderDevice::SetDrawBufferNone() {  }
	void NullRenderDevice::SetReadBufferNone() {  }
	void NullRenderDevice::SetDrawBufferBack() {  }
	void NullRenderDevice::SetReadBufferBack() {  }
	void NullRenderDevice::SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) {  }
	uint32 NullRenderDevice::CheckFramebufferStatus() { return 0; }
	uint32 NullRenderDevice::TranslateFramebufferStatus(const uint32 nativeStatus) { return 0; }
	DeviceHandle NullRenderDevice::CreateRenderbuffer() { return NextHandle(); }
	void NullRenderDevice::DestroyRenderbuffer(const DeviceHandle rbo) {  }
	void NullRenderDevice::BindRenderbuffer(const DeviceHandle rbo) {  }
	uint32 NullRenderDevice::TranslateRenderbufferFormat(const uint32 engineDataType) { return 0; }
	void NullRenderDevice::RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height) {  }
	void NullRenderDevice::RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height) {  }
	void NullRenderDevice::SetMultisampleEnabled(const bool enabled) {  }
	void NullRenderDevice::BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter) {  }
	void NullRenderDevice::CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height) {  }

}
