//============================================================================
// Name        : ImGuiRenderer.cpp
// Author      : Duarte Peixinho
// Description : Dear ImGui drawn on the device layer - see ImGuiRenderer.h
//============================================================================

#include <Pyros3D/Rendering/Renderer/ImGuiRenderer.h>
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <vector>

#if defined(PYROS_RENDERER_WGSL)
#define PYROS_WGSL(desc, name) desc.wgsl = name##Wgsl
#else
#define PYROS_WGSL(desc, name)
#endif

namespace p3d {

	namespace {
		const uint32_t kImGuiVert[] = {
#include "shaders/imgui.vert.inc"
		};
		const uint32_t kImGuiFrag[] = {
#include "shaders/imgui.frag.inc"
		};
#if defined(PYROS_RENDERER_WGSL)
		const char* const kImGuiVertWgsl =
#include "shaders/imgui.vert.wgsl.inc"
			;
		const char* const kImGuiFragWgsl =
#include "shaders/imgui.frag.wgsl.inc"
			;
#endif
	}

	ImGuiRenderer::ImGuiRenderer(gfx::Device* device) : device(device)
	{
		ImGuiIO &io = ImGui::GetIO();
		io.BackendRendererName = "Pyros3D";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;

		gfx::ShaderDesc s;
		s.stage = gfx::Stage::Vertex; s.spirv = kImGuiVert; s.spirvWords = sizeof(kImGuiVert) / 4; PYROS_WGSL(s, kImGuiVert); s.debugName = "imgui.vert";
		vs = device->CreateShader(s);
		s.stage = gfx::Stage::Fragment; s.spirv = kImGuiFrag; s.spirvWords = sizeof(kImGuiFrag) / 4; PYROS_WGSL(s, kImGuiFrag); s.debugName = "imgui.frag";
		fs = device->CreateShader(s);

		gfx::GraphicsPipelineDesc p;
		p.vertex = vs; p.fragment = fs;
		p.material = { { 1, gfx::BindingKind::Texture2D, false, true }, { 2, gfx::BindingKind::Sampler, false, true } };
		p.drawDataBytes = 16;
		p.bindings = { { 0, (uint16_t)sizeof(ImDrawVert), false } };
		p.attributes = { { 0, 0, gfx::Format::RG32F, (uint32_t)offsetof(ImDrawVert, pos) }, { 1, 0, gfx::Format::RG32F, (uint32_t)offsetof(ImDrawVert, uv) },
			{ 2, 0, gfx::Format::RGBA8, (uint32_t)offsetof(ImDrawVert, col) } };
		p.cull = gfx::Cull::None; p.depthTest = false; p.depthWrite = false;
		p.colorCount = 1; p.colorFormats[0] = gfx::Format::Unknown;     // (the backbuffer's, or a texture made with it)
		p.blend[0] = gfx::Blend::Alpha;
		p.debugName = "ImGuiRenderer";
		pipeline = device->CreatePipeline(p);

		gfx::SamplerDesc sd; sd.u = sd.v = sd.w = gfx::Address::Clamp; sd.anisotropy = 1.f;
		sampler = device->CreateSampler(sd);
	}

	ImGuiRenderer::~ImGuiRenderer()
	{
		// (ImGui's own textures, still alive: theirs to let go)
		for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
			if (tex->RefCount == 1 && tex->TexID != ImTextureID_Invalid)
			{
				gfx::Texture t; t.id = (uint32_t)tex->TexID;
				device->Destroy(t);
				tex->SetTexID(ImTextureID_Invalid);
				tex->SetStatus(ImTextureStatus_Destroyed);
			}
		for (std::unordered_map<uint32_t, gfx::BindGroup>::iterator i = groups.begin(); i != groups.end(); ++i) if (i->second) device->Destroy(i->second);
		if (vertices) device->Destroy(vertices);
		if (indices) device->Destroy(indices);
		// (a sampler is the device's for its life: the device layer has no Destroy for one)
		ImGuiIO &io = ImGui::GetIO();
		io.BackendRendererName = nullptr;
		io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
	}

	void ImGuiRenderer::Forget(const gfx::Texture t)
	{
		std::unordered_map<uint32_t, gfx::BindGroup>::iterator i = groups.find(t.id);
		if (i == groups.end()) return;
		if (i->second) device->Destroy(i->second);
		groups.erase(i);
	}

	gfx::BindGroup ImGuiRenderer::GroupFor(const uint32_t textureId)
	{
		std::unordered_map<uint32_t, gfx::BindGroup>::iterator i = groups.find(textureId);
		if (i != groups.end()) return i->second;
		gfx::Device::BindGroupEntry e[2];
		e[0].binding = 1; e[0].texture.id = textureId;
		e[1].binding = 2; e[1].sampler = sampler;
		const gfx::BindGroup g = device->CreateBindGroup(pipeline, e, 2);
		groups[textureId] = g;
		return g;
	}

	void ImGuiRenderer::UpdateTexture(ImTextureData* tex)
	{
		if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
		{
			// (RGBA8, as ImGui keeps it; an update sends the whole picture again - the atlas grows seldom)
			gfx::Texture t; t.id = (uint32_t)tex->TexID;
			if (tex->Status == ImTextureStatus_WantCreate || !t)
			{
				gfx::TextureDesc d; d.width = (uint32_t)tex->Width; d.height = (uint32_t)tex->Height; d.format = gfx::Format::RGBA8;
				d.usage = gfx::TexSampled | gfx::TexCopyDst; d.debugName = "ImGuiRenderer.Texture";
				gfx::TextureData data; data.bytes = tex->GetPixels(); data.size = (uint64_t)tex->GetSizeInBytes();
				t = device->CreateTexture(d, &data, 1);
				tex->SetTexID((ImTextureID)t.id);
			}
			else
			{
				gfx::TextureData data; data.bytes = tex->GetPixels(); data.size = (uint64_t)tex->GetSizeInBytes();
				device->Upload(t, &data, 1);
			}
			tex->SetStatus(ImTextureStatus_OK);
		}
		else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
		{
			gfx::Texture t; t.id = (uint32_t)tex->TexID;
			if (t) { Forget(t); device->Destroy(t); }
			tex->SetTexID(ImTextureID_Invalid);
			tex->SetStatus(ImTextureStatus_Destroyed);
		}
	}

	void ImGuiRenderer::Render(ImDrawData* data, gfx::CommandList* cl, gfx::Texture target, const bool clear)
	{
		if (!data) return;
		if (data->Textures)
			for (ImTextureData* tex : *data->Textures) if (tex->Status != ImTextureStatus_OK) UpdateTexture(tex);
		const uint32_t w = (uint32_t)(data->DisplaySize.x * data->FramebufferScale.x), h = (uint32_t)(data->DisplaySize.y * data->FramebufferScale.y);
		if (w == 0 || h == 0) return;
		const gfx::Texture into = target ? target : device->AcquireBackbuffer();

		// This frame's vertices and indices, in its slot of the two rings (grown to hold them)
		slot = (slot + 1) % kSlots;
		const uint64_t vNeed = (uint64_t)std::max(data->TotalVtxCount, 1) * sizeof(ImDrawVert), iNeed = (uint64_t)std::max(data->TotalIdxCount, 1) * sizeof(ImDrawIdx);
		if (vNeed > vertexBytes)
		{
			if (vertices) device->Destroy(vertices);
			vertexBytes = (std::max<uint64_t>(vNeed * 2, 1 << 16) + 255) & ~255ull;     // (each slot starting on a whole word, and more)
			gfx::BufferDesc b; b.size = vertexBytes * kSlots; b.usage = gfx::BufVertex; b.memory = gfx::Memory::Upload; b.debugName = "ImGuiRenderer.Vertices";
			vertices = device->CreateBuffer(b);
		}
		if (iNeed > indexBytes)
		{
			if (indices) device->Destroy(indices);
			indexBytes = (std::max<uint64_t>(iNeed * 2, 1 << 15) + 255) & ~255ull;
			gfx::BufferDesc b; b.size = indexBytes * kSlots; b.usage = gfx::BufIndex; b.memory = gfx::Memory::Upload; b.debugName = "ImGuiRenderer.Indices";
			indices = device->CreateBuffer(b);
		}
		const uint64_t vBase = vertexBytes * slot, iBase = indexBytes * slot;
		{
			// (each in one piece: a list's 16-bit indices, an odd count of them, would leave the next list's off a word -
			// which WebGPU refuses to write at)
			staging.resize((size_t)std::max(vNeed, iNeed));
			uint64_t at = 0;
			for (const ImDrawList* list : data->CmdLists)
			{
				memcpy(staging.data() + at, list->VtxBuffer.Data, (size_t)list->VtxBuffer.Size * sizeof(ImDrawVert));
				at += (uint64_t)list->VtxBuffer.Size * sizeof(ImDrawVert);
			}
			if (at) device->Upload(vertices, vBase, staging.data(), at);
			at = 0;
			for (const ImDrawList* list : data->CmdLists)
			{
				memcpy(staging.data() + at, list->IdxBuffer.Data, (size_t)list->IdxBuffer.Size * sizeof(ImDrawIdx));
				at += (uint64_t)list->IdxBuffer.Size * sizeof(ImDrawIdx);
			}
			if (at) device->Upload(indices, iBase, staging.data(), at);
		}

		gfx::PassDesc pass;
		pass.colorCount = 1; pass.colors[0].texture = into; pass.colors[0].load = clear ? gfx::Load::Clear : gfx::Load::Keep;
		pass.width = w; pass.height = h; pass.debugName = "ImGui";
		cl->BeginPass(pass);
		cl->SetViewport(0.f, 0.f, (float)w, (float)h);
		if (!device->PipelineReady(pipeline)) { cl->EndPass(); return; }
		cl->SetPipeline(pipeline);
		// (ImGui's pixels to clip space: x right, y down)
		const float st[4] = { 2.f / data->DisplaySize.x, -2.f / data->DisplaySize.y,
			-1.f - data->DisplayPos.x * 2.f / data->DisplaySize.x, 1.f + data->DisplayPos.y * 2.f / data->DisplaySize.y };
		cl->SetVertexBuffer(0, vertices, vBase);
		cl->SetIndexBuffer(indices, iBase, sizeof(ImDrawIdx) == 4);
		const ImVec2 off = data->DisplayPos, scale = data->FramebufferScale;
		uint32_t bound = 0;
		int32_t vtx = 0, idx = 0;
		for (const ImDrawList* list : data->CmdLists)
		{
			for (const ImDrawCmd &c : list->CmdBuffer)
			{
				if (c.UserCallback)
				{
					if (c.UserCallback != ImDrawCallback_ResetRenderState) c.UserCallback(list, &c);
					bound = 0;
					continue;
				}
				// (its clip rectangle in the target's pixels, kept inside it)
				const float x0 = std::max((c.ClipRect.x - off.x) * scale.x, 0.f), y0 = std::max((c.ClipRect.y - off.y) * scale.y, 0.f);
				const float x1 = std::min((c.ClipRect.z - off.x) * scale.x, (float)w), y1 = std::min((c.ClipRect.w - off.y) * scale.y, (float)h);
				if (x1 <= x0 || y1 <= y0) continue;
				cl->SetScissor((int32_t)x0, (int32_t)y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0));
				const uint32_t texture = (uint32_t)c.GetTexID();
				if (!texture) continue;
				if (texture != bound) { cl->SetBindGroup(GroupFor(texture), nullptr, 0); bound = texture; }
				cl->DrawData(st, sizeof(st));
				cl->DrawIndexed(c.ElemCount, 1, (uint32_t)(c.IdxOffset + idx), (int32_t)(c.VtxOffset + vtx), 0);
			}
			vtx += list->VtxBuffer.Size; idx += list->IdxBuffer.Size;
		}
		cl->EndPass();
	}

}
