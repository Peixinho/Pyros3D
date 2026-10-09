// Written by tools/gen_threaded_device.py from IRenderDevice.h - not by hand.
	void ThreadedRenderDevice::EndCommandBuffer(const CommandBufferHandle cmd)
	{
		Push([dev_ = real, cmd]() { dev_->EndCommandBuffer(cmd); });
	}

	void ThreadedRenderDevice::FlushOffscreenWork()
	{
		Push([dev_ = real]() { dev_->FlushOffscreenWork(); });
	}

	std::string ThreadedRenderDevice::MemoryReport()
	{
		Drain("MemoryReport");
		return real->MemoryReport();
	}

	void ThreadedRenderDevice::WaitOffscreenWork()
	{
		Drain("WaitOffscreenWork");
		real->WaitOffscreenWork();
	}

	bool ThreadedRenderDevice::IsVulkan() const
	{
		return real->IsVulkan();
	}

	bool ThreadedRenderDevice::NeedsManualDisplayGamma() const
	{
		return real->NeedsManualDisplayGamma();
	}

	bool ThreadedRenderDevice::RenderTargetOriginIsTopLeft() const
	{
		return real->RenderTargetOriginIsTopLeft();
	}

	uint32 ThreadedRenderDevice::TranslateBufferBit(const uint32 bufferBits)
	{
		return real->TranslateBufferBit(bufferBits);
	}

	void ThreadedRenderDevice::Clear(const uint32 nativeBufferBits)
	{
		Push([dev_ = real, nativeBufferBits]() { dev_->Clear(nativeBufferBits); });
	}

	void ThreadedRenderDevice::SetDepthTest(const bool enabled, const uint32 mode)
	{
		Push([dev_ = real, enabled, mode]() { dev_->SetDepthTest(enabled, mode); });
	}

	void ThreadedRenderDevice::SetDepthMask(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetDepthMask(enabled); });
	}

	void ThreadedRenderDevice::PrepareDepthClear()
	{
		Push([dev_ = real]() { dev_->PrepareDepthClear(); });
	}

	void ThreadedRenderDevice::SetStencilTestEnabled(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetStencilTestEnabled(enabled); });
	}

	void ThreadedRenderDevice::SetClearStencilValue()
	{
		Push([dev_ = real]() { dev_->SetClearStencilValue(); });
	}

	void ThreadedRenderDevice::SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask)
	{
		Push([dev_ = real, func, ref, mask]() { dev_->SetStencilFunction(func, ref, mask); });
	}

	void ThreadedRenderDevice::SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass)
	{
		Push([dev_ = real, sfail, dpfail, dppass]() { dev_->SetStencilOperation(sfail, dpfail, dppass); });
	}

	void ThreadedRenderDevice::SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height)
	{
		Push([dev_ = real, x, y, width, height]() { dev_->SetScissorRect(x, y, width, height); });
	}

	void ThreadedRenderDevice::SetScissorTestEnabled(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetScissorTestEnabled(enabled); });
	}

	void ThreadedRenderDevice::SetWireFrame(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetWireFrame(enabled); });
	}

	void ThreadedRenderDevice::SetColorMask(const bool r, const bool g, const bool b, const bool a)
	{
		Push([dev_ = real, r, g, b, a]() { dev_->SetColorMask(r, g, b, a); });
	}

	void ThreadedRenderDevice::SetPolygonOffsetEnabled(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetPolygonOffsetEnabled(enabled); });
	}

	void ThreadedRenderDevice::SetPolygonOffset(const f32 factor, const f32 units)
	{
		Push([dev_ = real, factor, units]() { dev_->SetPolygonOffset(factor, units); });
	}

	void ThreadedRenderDevice::SetBlendingEnabled(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetBlendingEnabled(enabled); });
	}

	void ThreadedRenderDevice::SetBlendFunction(const uint32 sfactor, const uint32 dfactor)
	{
		Push([dev_ = real, sfactor, dfactor]() { dev_->SetBlendFunction(sfactor, dfactor); });
	}

	void ThreadedRenderDevice::SetBlendEquation(const uint32 mode)
	{
		Push([dev_ = real, mode]() { dev_->SetBlendEquation(mode); });
	}

	void ThreadedRenderDevice::SetCullFaceMode(const uint32 cullFace)
	{
		Push([dev_ = real, cullFace]() { dev_->SetCullFaceMode(cullFace); });
	}

	void ThreadedRenderDevice::DisableCullFace()
	{
		Push([dev_ = real]() { dev_->DisableCullFace(); });
	}

	void ThreadedRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		Push([dev_ = real, cmd, pipeline, self = this]() { dev_->BindPipeline(cmd, self->RealOf(pipeline)); });
	}

	uint32 ThreadedRenderDevice::GetSwapchainGeneration() const
	{
		return real->GetSwapchainGeneration();
	}

	void ThreadedRenderDevice::NotifySurfaceResized(const uint32 width, const uint32 height)
	{
		Drain("NotifySurfaceResized");
		real->NotifySurfaceResized(width, height);
	}

	void ThreadedRenderDevice::EnableClipDistance(const uint32 index)
	{
		Push([dev_ = real, index]() { dev_->EnableClipDistance(index); });
	}

	void ThreadedRenderDevice::DisableClipDistance(const uint32 index)
	{
		Push([dev_ = real, index]() { dev_->DisableClipDistance(index); });
	}

	void ThreadedRenderDevice::SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height)
	{
		Push([dev_ = real, x, y, width, height]() { dev_->SetViewport(x, y, width, height); });
	}

	void ThreadedRenderDevice::UseProgram(const uint32 program)
	{
		Push([dev_ = real, program]() { dev_->UseProgram(program); });
	}

	void ThreadedRenderDevice::BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao)
	{
		Push([dev_ = real, cmd, vao, self = this]() { dev_->BindVertexArray(cmd, self->RealOf(vao)); });
	}

	void ThreadedRenderDevice::BindArrayBuffer(const uint32 buffer)
	{
		Push([dev_ = real, buffer, self = this]() { dev_->BindArrayBuffer(self->RealOf(buffer)); });
	}

	void ThreadedRenderDevice::BindElementBuffer(const uint32 buffer)
	{
		Push([dev_ = real, buffer, self = this]() { dev_->BindElementBuffer(self->RealOf(buffer)); });
	}

	void ThreadedRenderDevice::SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset)
	{
		Push([dev_ = real, location, typeCount, nativeType, stride, offset]() { dev_->SetVertexAttribute(location, typeCount, nativeType, stride, offset); });
	}

	void ThreadedRenderDevice::SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset)
	{
		Push([dev_ = real, location, componentCount, stride, offset]() { dev_->SetFloatVertexAttribute(location, componentCount, stride, offset); });
	}

	void ThreadedRenderDevice::DisableVertexAttribute(const int32 location)
	{
		Push([dev_ = real, location]() { dev_->DisableVertexAttribute(location); });
	}

	void ThreadedRenderDevice::SetVertexAttributeDivisor(const int32 location, const uint32 divisor)
	{
		Push([dev_ = real, location, divisor]() { dev_->SetVertexAttributeDivisor(location, divisor); });
	}

	void ThreadedRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string & blockName, const uint32 bindingPoint, const DeviceHandle bufferHandle)
	{
		Push([dev_ = real, program, blockName = std::string(blockName), bindingPoint, bufferHandle, self = this]() { dev_->BindUniformBlockIfPresent(program, blockName, bindingPoint, self->RealOf(bufferHandle)); });
	}

	Matrix ThreadedRenderDevice::TranslateProjectionMatrix(const Matrix & projectionMatrix, const bool skipYFlip)
	{
		return real->TranslateProjectionMatrix(projectionMatrix, skipYFlip);
	}

	void ThreadedRenderDevice::SetPointShadowCubeFacePass(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetPointShadowCubeFacePass(enabled); });
	}

	Matrix ThreadedRenderDevice::TranslateShadowBiasMatrix()
	{
		return real->TranslateShadowBiasMatrix();
	}

	uint32 ThreadedRenderDevice::TranslateDrawType(const uint32 engineDrawType)
	{
		return real->TranslateDrawType(engineDrawType);
	}

	void ThreadedRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count)
	{
		Push([dev_ = real, nativeDrawType, first, count]() { dev_->DrawArrays(nativeDrawType, first, count); });
	}

	void ThreadedRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount)
	{
		Push([dev_ = real, cmd, nativeDrawType, indexCount]() { dev_->DrawElements(cmd, nativeDrawType, indexCount); });
	}

	void ThreadedRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount)
	{
		Push([dev_ = real, cmd, nativeDrawType, indexCount, instanceCount]() { dev_->DrawElementsInstanced(cmd, nativeDrawType, indexCount, instanceCount); });
	}

	void ThreadedRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void * data)
	{
		const void* data_ = data ? Keep(data, (size_t)(sizeBytes)) : NULL;
		Push([dev_ = real, buffer, self = this, offset, sizeBytes, data_]() { dev_->UpdateUniformBuffer(self->RealOf(buffer), offset, sizeBytes, data_); });
	}

	void ThreadedRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void * data)
	{
		const void* data_ = data ? Keep(data, (size_t)(sizeBytes)) : NULL;
		Push([dev_ = real, buffer, self = this, sizeBytes, data_]() { dev_->ReplaceUniformBuffer(self->RealOf(buffer), sizeBytes, data_); });
	}

	void ThreadedRenderDevice::ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void * data, const uint32 length)
	{
		const void* data_ = data ? Keep(data, (size_t)(length)) : NULL;
		Push([dev_ = real, buffer, self = this, bufferType, bufferDraw, data_, length]() { dev_->ReallocateBuffer(self->RealOf(buffer), bufferType, bufferDraw, data_, length); });
	}

	void ThreadedRenderDevice::UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void * data, const uint32 length)
	{
		const void* data_ = data ? Keep(data, (size_t)(length)) : NULL;
		Push([dev_ = real, buffer, self = this, bufferType, data_, length]() { dev_->UpdateBufferSubData(self->RealOf(buffer), bufferType, data_, length); });
	}

	void * ThreadedRenderDevice::MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType)
	{
		Drain("MapBuffer");
		return real->MapBuffer(RealOf(buffer), bufferType, mappingType);
	}

	void ThreadedRenderDevice::UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType)
	{
		Drain("UnmapBuffer");
		real->UnmapBuffer(RealOf(buffer), bufferType);
	}

	bool ThreadedRenderDevice::SupportsCompute() const
	{
		return real->SupportsCompute();
	}

	uint32 ThreadedRenderDevice::GetMaxComputeWorkGroupInvocations() const
	{
		return real->GetMaxComputeWorkGroupInvocations();
	}

	uint32 ThreadedRenderDevice::GetMaxComputeWorkGroupCount(const uint32 dimension) const
	{
		return real->GetMaxComputeWorkGroupCount(dimension);
	}

	DeviceHandle ThreadedRenderDevice::CreateComputePipeline(const DeviceHandle program)
	{
		Drain("CreateComputePipeline");
		return real->CreateComputePipeline(program);
	}

	void ThreadedRenderDevice::DestroyComputePipeline(const DeviceHandle pipeline)
	{
		Push([dev_ = real, pipeline]() { dev_->DestroyComputePipeline(pipeline); });
	}

	void ThreadedRenderDevice::BindComputePipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		Push([dev_ = real, cmd, pipeline]() { dev_->BindComputePipeline(cmd, pipeline); });
	}

	DeviceHandle ThreadedRenderDevice::CreateStorageBuffer(const uint32 sizeBytes, const uint32 bindingPoint, const void * data)
	{
		Drain("CreateStorageBuffer");
		return real->CreateStorageBuffer(sizeBytes, bindingPoint, data);
	}

	void ThreadedRenderDevice::UpdateStorageBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void * data)
	{
		const void* data_ = data ? Keep(data, (size_t)(sizeBytes)) : NULL;
		Push([dev_ = real, buffer, offset, sizeBytes, data_]() { dev_->UpdateStorageBuffer(buffer, offset, sizeBytes, data_); });
	}

	void ThreadedRenderDevice::ReadStorageBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, void * outData)
	{
		Drain("ReadStorageBuffer");
		real->ReadStorageBuffer(buffer, offset, sizeBytes, outData);
	}

	void ThreadedRenderDevice::BindStorageBuffer(const CommandBufferHandle cmd, const DeviceHandle buffer, const uint32 bindingPoint)
	{
		Push([dev_ = real, cmd, buffer, bindingPoint]() { dev_->BindStorageBuffer(cmd, buffer, bindingPoint); });
	}

	void ThreadedRenderDevice::DestroyStorageBuffer(const DeviceHandle buffer)
	{
		Push([dev_ = real, buffer]() { dev_->DestroyStorageBuffer(buffer); });
	}

	void ThreadedRenderDevice::Dispatch(const CommandBufferHandle cmd, const uint32 groupsX, const uint32 groupsY, const uint32 groupsZ)
	{
		Push([dev_ = real, cmd, groupsX, groupsY, groupsZ]() { dev_->Dispatch(cmd, groupsX, groupsY, groupsZ); });
	}

	void ThreadedRenderDevice::ComputeBarrier(const CommandBufferHandle cmd, const uint32 barrierBits)
	{
		Push([dev_ = real, cmd, barrierBits]() { dev_->ComputeBarrier(cmd, barrierBits); });
	}

	bool ThreadedRenderDevice::HasPendingComputeWork() const
	{
		const_cast<ThreadedRenderDevice*>(this)->Drain("HasPendingComputeWork");
		return real->HasPendingComputeWork();
	}

	uint32 ThreadedRenderDevice::TranslateAttributeType(const uint32 engineType)
	{
		return real->TranslateAttributeType(engineType);
	}

	std::string ThreadedRenderDevice::BuildShaderSource(const std::string & definitions, const std::string & shaderBody)
	{
		return real->BuildShaderSource(definitions, shaderBody);
	}

	DeviceHandle ThreadedRenderDevice::CreateShaderStage(const uint32 engineShaderType)
	{
		Drain("CreateShaderStage");
		return real->CreateShaderStage(engineShaderType);
	}

	bool ThreadedRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string & source, std::string & errorLog)
	{
		Drain("CompileShaderStage");
		return real->CompileShaderStage(shader, source, errorLog);
	}

	DeviceHandle ThreadedRenderDevice::CreateProgram()
	{
		Drain("CreateProgram");
		return real->CreateProgram();
	}

	void ThreadedRenderDevice::AttachShaderStage(const DeviceHandle program, const DeviceHandle shader)
	{
		Push([dev_ = real, program, shader]() { dev_->AttachShaderStage(program, shader); });
	}

	bool ThreadedRenderDevice::LinkProgram(const DeviceHandle program, std::string & errorLog)
	{
		Drain("LinkProgram");
		return real->LinkProgram(program, errorLog);
	}

	bool ThreadedRenderDevice::IsProgram(const DeviceHandle id)
	{
		Drain("IsProgram");
		return real->IsProgram(id);
	}

	bool ThreadedRenderDevice::IsShaderStage(const DeviceHandle id)
	{
		Drain("IsShaderStage");
		return real->IsShaderStage(id);
	}

	void ThreadedRenderDevice::DetachShaderStage(const DeviceHandle program, const DeviceHandle shader)
	{
		Push([dev_ = real, program, shader]() { dev_->DetachShaderStage(program, shader); });
	}

	void ThreadedRenderDevice::DeleteShaderStage(const DeviceHandle shader)
	{
		Push([dev_ = real, shader]() { dev_->DeleteShaderStage(shader); });
	}

	void ThreadedRenderDevice::DeleteProgram(const DeviceHandle program)
	{
		Push([dev_ = real, program]() { dev_->DeleteProgram(program); });
	}

	int32 ThreadedRenderDevice::GetUniformLocation(const uint32 program, const std::string & name)
	{
		return real->GetUniformLocation(program, name);
	}

	int32 ThreadedRenderDevice::GetAttributeLocation(const uint32 program, const std::string & name)
	{
		return real->GetAttributeLocation(program, name);
	}

	void ThreadedRenderDevice::SendUniformInt(const int32 handle, const int32 * data, const uint32 count)
	{
		const void* data_ = data ? Keep(data, (size_t)(count * 4)) : NULL;
		Push([dev_ = real, handle, data_, count]() { dev_->SendUniformInt(handle, (const int32 *)data_, count); });
	}

	void ThreadedRenderDevice::SendUniformFloat(const int32 handle, const f32 * data, const uint32 count)
	{
		const void* data_ = data ? Keep(data, (size_t)(count * 4)) : NULL;
		Push([dev_ = real, handle, data_, count]() { dev_->SendUniformFloat(handle, (const f32 *)data_, count); });
	}

	void ThreadedRenderDevice::SendUniformVec2(const int32 handle, const f32 * data, const uint32 count)
	{
		const void* data_ = data ? Keep(data, (size_t)(count * 8)) : NULL;
		Push([dev_ = real, handle, data_, count]() { dev_->SendUniformVec2(handle, (const f32 *)data_, count); });
	}

	void ThreadedRenderDevice::SendUniformVec3(const int32 handle, const f32 * data, const uint32 count)
	{
		const void* data_ = data ? Keep(data, (size_t)(count * 12)) : NULL;
		Push([dev_ = real, handle, data_, count]() { dev_->SendUniformVec3(handle, (const f32 *)data_, count); });
	}

	void ThreadedRenderDevice::SendUniformVec4(const int32 handle, const f32 * data, const uint32 count)
	{
		const void* data_ = data ? Keep(data, (size_t)(count * 16)) : NULL;
		Push([dev_ = real, handle, data_, count]() { dev_->SendUniformVec4(handle, (const f32 *)data_, count); });
	}

	void ThreadedRenderDevice::SendUniformMatrix(const int32 handle, const f32 * data, const uint32 count)
	{
		const void* data_ = data ? Keep(data, (size_t)(count * 64)) : NULL;
		Push([dev_ = real, handle, data_, count]() { dev_->SendUniformMatrix(handle, (const f32 *)data_, count); });
	}

	void ThreadedRenderDevice::TranslateTextureFormat(const uint32 engineDataType, uint32 & internalFormat, uint32 & format, uint32 & type)
	{
		real->TranslateTextureFormat(engineDataType, internalFormat, format, type);
	}

	void ThreadedRenderDevice::TranslateTextureTarget(const uint32 engineTextureType, uint32 & mode, uint32 & subMode)
	{
		real->TranslateTextureTarget(engineTextureType, mode, subMode);
	}

	void * ThreadedRenderDevice::GetImGuiTextureID(const DeviceHandle texture, const uint32 engineTextureType)
	{
		Drain("GetImGuiTextureID");
		return real->GetImGuiTextureID(texture, engineTextureType);
	}

	DeviceHandle ThreadedRenderDevice::CreateTextureObject()
	{
		Drain("CreateTextureObject");
		return real->CreateTextureObject();
	}

	void ThreadedRenderDevice::DestroyTextureObject(const DeviceHandle texture)
	{
		Push([dev_ = real, texture]() { dev_->DestroyTextureObject(texture); });
	}

	void ThreadedRenderDevice::BindTextureToTarget(const uint32 target, const DeviceHandle texture)
	{
		Push([dev_ = real, target, texture]() { dev_->BindTextureToTarget(target, texture); });
	}

	void ThreadedRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void * data, const bool willMipmap)
	{
		Drain("UploadTexture2D");
		real->UploadTexture2D(target, level, internalFormat, width, height, format, type, data, willMipmap);
	}

	void ThreadedRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height)
	{
		Drain("UploadTexture2DMultisample");
		real->UploadTexture2DMultisample(target, samples, internalFormat, width, height);
	}

	void ThreadedRenderDevice::GenerateMipmap(const uint32 target)
	{
		Push([dev_ = real, target]() { dev_->GenerateMipmap(target); });
	}

	void ThreadedRenderDevice::SetTextureWrapS(const uint32 target, const uint32 engineRepeat)
	{
		Push([dev_ = real, target, engineRepeat]() { dev_->SetTextureWrapS(target, engineRepeat); });
	}

	void ThreadedRenderDevice::SetTextureWrapT(const uint32 target, const uint32 engineRepeat)
	{
		Push([dev_ = real, target, engineRepeat]() { dev_->SetTextureWrapT(target, engineRepeat); });
	}

	void ThreadedRenderDevice::SetTextureWrapR(const uint32 target, const uint32 engineRepeat)
	{
		Push([dev_ = real, target, engineRepeat]() { dev_->SetTextureWrapR(target, engineRepeat); });
	}

	void ThreadedRenderDevice::SetTextureMagFilter(const uint32 target, const uint32 engineFilter)
	{
		Push([dev_ = real, target, engineFilter]() { dev_->SetTextureMagFilter(target, engineFilter); });
	}

	void ThreadedRenderDevice::SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap)
	{
		Push([dev_ = real, target, engineFilter, hasMipmap]() { dev_->SetTextureMinFilter(target, engineFilter, hasMipmap); });
	}

	void ThreadedRenderDevice::SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel)
	{
		Push([dev_ = real, target, baseLevel, maxLevel]() { dev_->SetTextureBaseMaxLevel(target, baseLevel, maxLevel); });
	}

	void ThreadedRenderDevice::SetTextureBorderColor(const uint32 target, const Vec4 & color)
	{
		Push([dev_ = real, target, color = Vec4(color)]() { dev_->SetTextureBorderColor(target, color); });
	}

	void ThreadedRenderDevice::SetTextureCompareMode(const uint32 target)
	{
		Push([dev_ = real, target]() { dev_->SetTextureCompareMode(target); });
	}

	void ThreadedRenderDevice::SetPixelUnpackAlignment(const uint32 value)
	{
		Push([dev_ = real, value]() { dev_->SetPixelUnpackAlignment(value); });
	}

	void ThreadedRenderDevice::ActivateTextureUnit(const uint32 unit)
	{
		Push([dev_ = real, unit]() { dev_->ActivateTextureUnit(unit); });
	}

	void ThreadedRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void * outBuffer)
	{
		Drain("ReadTexturePixels");
		real->ReadTexturePixels(target, level, format, type, outBuffer);
	}

	uint32 ThreadedRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height)
	{
		return real->GetTextureDataSize(nativeInternalFormat, width, height);
	}

	DeviceHandle ThreadedRenderDevice::CreateFramebuffer()
	{
		Drain("CreateFramebuffer");
		return real->CreateFramebuffer();
	}

	void ThreadedRenderDevice::DestroyFramebuffer(const DeviceHandle fbo)
	{
		Push([dev_ = real, fbo]() { dev_->DestroyFramebuffer(fbo); });
	}

	void ThreadedRenderDevice::SetFramebufferPreserveDepth(const DeviceHandle fbo, const bool preserve)
	{
		Push([dev_ = real, fbo, preserve]() { dev_->SetFramebufferPreserveDepth(fbo, preserve); });
	}

	uint32 ThreadedRenderDevice::TranslateFramebufferAccess(const uint32 engineAccess)
	{
		return real->TranslateFramebufferAccess(engineAccess);
	}

	uint32 ThreadedRenderDevice::TranslateFramebufferAttachment(const uint32 engineAttachmentFormat)
	{
		return real->TranslateFramebufferAttachment(engineAttachmentFormat);
	}

	void ThreadedRenderDevice::AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound)
	{
		Push([dev_ = real, nativeAttachmentFormat, nativeTextureTarget, textureId, wasAlreadyBound]() { dev_->AttachFramebufferTexture2D(nativeAttachmentFormat, nativeTextureTarget, textureId, wasAlreadyBound); });
	}

	void ThreadedRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer)
	{
		Push([dev_ = real, nativeAttachmentFormat, renderbuffer]() { dev_->AttachFramebufferRenderbuffer(nativeAttachmentFormat, renderbuffer); });
	}

	void ThreadedRenderDevice::SetDrawBufferNone()
	{
		Push([dev_ = real]() { dev_->SetDrawBufferNone(); });
	}

	void ThreadedRenderDevice::SetReadBufferNone()
	{
		Push([dev_ = real]() { dev_->SetReadBufferNone(); });
	}

	void ThreadedRenderDevice::SetDrawBufferBack()
	{
		Push([dev_ = real]() { dev_->SetDrawBufferBack(); });
	}

	void ThreadedRenderDevice::SetReadBufferBack()
	{
		Push([dev_ = real]() { dev_->SetReadBufferBack(); });
	}

	void ThreadedRenderDevice::SetDrawBuffers(const std::vector<uint32> & colorAttachmentIndices)
	{
		Push([dev_ = real, colorAttachmentIndices = std::vector<uint32>(colorAttachmentIndices)]() { dev_->SetDrawBuffers(colorAttachmentIndices); });
	}

	uint32 ThreadedRenderDevice::CheckFramebufferStatus()
	{
		Drain("CheckFramebufferStatus");
		return real->CheckFramebufferStatus();
	}

	uint32 ThreadedRenderDevice::TranslateFramebufferStatus(const uint32 nativeStatus)
	{
		return real->TranslateFramebufferStatus(nativeStatus);
	}

	DeviceHandle ThreadedRenderDevice::CreateRenderbuffer()
	{
		Drain("CreateRenderbuffer");
		return real->CreateRenderbuffer();
	}

	void ThreadedRenderDevice::DestroyRenderbuffer(const DeviceHandle rbo)
	{
		Push([dev_ = real, rbo]() { dev_->DestroyRenderbuffer(rbo); });
	}

	void ThreadedRenderDevice::BindRenderbuffer(const DeviceHandle rbo)
	{
		Push([dev_ = real, rbo]() { dev_->BindRenderbuffer(rbo); });
	}

	uint32 ThreadedRenderDevice::TranslateRenderbufferFormat(const uint32 engineDataType)
	{
		return real->TranslateRenderbufferFormat(engineDataType);
	}

	void ThreadedRenderDevice::RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height)
	{
		Push([dev_ = real, nativeFormat, width, height]() { dev_->RenderbufferStorage(nativeFormat, width, height); });
	}

	void ThreadedRenderDevice::RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height)
	{
		Push([dev_ = real, nativeFormat, samples, width, height]() { dev_->RenderbufferStorageMultisample(nativeFormat, samples, width, height); });
	}

	void ThreadedRenderDevice::SetMultisampleEnabled(const bool enabled)
	{
		Push([dev_ = real, enabled]() { dev_->SetMultisampleEnabled(enabled); });
	}

	void ThreadedRenderDevice::BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter)
	{
		Push([dev_ = real, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, engineMask, engineFilter]() { dev_->BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, engineMask, engineFilter); });
	}

	uint32 ThreadedRenderDevice::GetMaxSamples() const
	{
		return real->GetMaxSamples();
	}

	bool ThreadedRenderDevice::CanBlitResolveDepth() const
	{
		return real->CanBlitResolveDepth();
	}

	const char* ThreadedRenderDevice::TemporalUpscalerId() const
	{
		return real->TemporalUpscalerId();
	}

	bool ThreadedRenderDevice::RunTemporalUpscale(const TemporalUpscale & frame)
	{
		Drain("RunTemporalUpscale");
		return real->RunTemporalUpscale(frame);
	}

	void ThreadedRenderDevice::CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height)
	{
		Push([dev_ = real, srcTexture, dstTexture, width, height]() { dev_->CopyDepthTexture(srcTexture, dstTexture, width, height); });
	}

	bool ThreadedRenderDevice::GetAutoUniformBlockLayout(const uint32 program, const uint32 engineShaderType, uint32 & outBinding, std::string & outBlockName, uint32 & outSize, std::map<std::string, uint32> & outOffsets)
	{
		Drain("GetAutoUniformBlockLayout");
		return real->GetAutoUniformBlockLayout(program, engineShaderType, outBinding, outBlockName, outSize, outOffsets);
	}
