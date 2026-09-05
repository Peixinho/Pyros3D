//============================================================================
// Name        : RenderTargetsTab.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See RenderTargetsTab.h. Ported from the demo launcher's
//               render-target viewer, comments and all - the traps it
//               documents (native vs engine enums on the attachment, raw
//               depth reading as a white rectangle) are the same here.
//============================================================================

#include "RenderTargetsTab.h"
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <set>
#include <string>
#include <vector>

using namespace p3d;

RenderTargetsTab::RenderTargetsTab(const std::string &name, bool* open)
	: Name(name), Open(open), thumbSize(200.f), refreshSeconds(0.25f), now(0.0)
{
}

RenderTargetsTab::~RenderTargetsTab()
{
	ClearPreviews();
}

void RenderTargetsTab::ClearPreviews()
{
	for (std::map<std::pair<Texture*, int32>, Preview>::iterator i = previews.begin(); i != previews.end(); i++)
		delete i->second.preview;
	previews.clear();
}

// Depth is normalised between the min and max actually present rather than
// shown raw. A raw depth buffer is almost entirely values near 1.0 and reads
// as a white rectangle - technically correct and completely useless.
RenderTargetsTab::Preview *RenderTargetsTab::GetPreview(Texture *src, const int32 face, const bool refresh)
{
	if (src == NULL)
		return NULL;

	std::pair<Texture*, int32> key(src, face);
	Preview &pv = previews[key];

	const bool due = (pv.lastUpdate < 0.0) || (now - pv.lastUpdate >= (f64)refreshSeconds);
	if (pv.preview != NULL && (!refresh || !due))
		return &pv;
	if (!refresh && pv.preview == NULL)
		return NULL;

	const uint32 srcW = src->GetWidth(), srcH = src->GetHeight();
	if (srcW == 0 || srcH == 0)
		return NULL;

	std::vector<uchar> data = src->GetTextureData(0, face);
	const size_t texels = (size_t)srcW * (size_t)srcH;
	if (data.size() < texels)
		return NULL;

	// Depth comes back as 32-bit float per texel on all three backends.
	const bool haveFloats = data.size() >= texels * 4;
	const float *depth = haveFloats ? (const float*)&data[0] : NULL;

	// Fixed preview size, point-sampled down - the source can be 2048 square
	// and the panel shows it a couple of hundred pixels wide.
	const uint32 pw = 256, ph = 256;
	std::vector<uchar> rgba((size_t)pw * ph * 4, 0);

	float lo = 1.0e30f, hi = -1.0e30f;
	std::vector<float> sampled((size_t)pw * ph, 0.0f);
	for (uint32 y = 0; y < ph; y++)
	{
		const uint32 sy = (uint32)((f32)y / (f32)ph * (f32)srcH);
		for (uint32 x = 0; x < pw; x++)
		{
			const uint32 sx = (uint32)((f32)x / (f32)pw * (f32)srcW);
			const size_t si = (size_t)sy * srcW + sx;
			const float v = depth != NULL ? depth[si] : (float)data[si] / 255.0f;
			sampled[(size_t)y * pw + x] = v;
			if (v < lo) lo = v;
			if (v > hi) hi = v;
		}
	}
	// A flat buffer (nothing drawn, all cleared) must not blow up into noise.
	const float span = (hi - lo) > 1.0e-6f ? (hi - lo) : 1.0f;
	for (size_t i = 0; i < sampled.size(); i++)
	{
		const float n = (sampled[i] - lo) / span;
		const uchar c = (uchar)(n * 255.0f);
		rgba[i * 4 + 0] = c; rgba[i * 4 + 1] = c; rgba[i * 4 + 2] = c; rgba[i * 4 + 3] = 255;
	}

	if (pv.preview == NULL)
	{
		pv.preview = new Texture();
		pv.preview->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, (int32)pw, (int32)ph, false);
		pv.width = pw; pv.height = ph;
	}
	pv.preview->UpdateData(&rgba[0], 0);
	pv.lastUpdate = now;
	pv.rangeMin = lo;
	pv.rangeMax = hi;
	return &pv;
}

void RenderTargetsTab::Show()
{
	if (!Open || !*Open)
		return;

	ImGui::SetNextWindowSize(ImVec2(560, 620), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(Name.c_str(), Open))
	{
		ImGui::End();
		return;
	}

	const std::vector<FrameBuffer*> &fbos = FrameBuffer::GetLiveFrameBuffers();

	// Drop previews whose source is gone. Unlike the demo launcher - one
	// scene at a time, previews cleared on the switch - an editor destroys
	// render targets constantly: every closed document, every stopped Play,
	// every viewport resize. Without this the map only ever grows, and each
	// entry holds a 256x256 texture. Keys are compared, never dereferenced,
	// so a dangling source pointer is safe to look at here.
	{
		std::set<Texture*> live;
		for (size_t f = 0; f < fbos.size(); f++)
		{
			if (fbos[f] == NULL) continue;
			const std::vector<FBOAttachment*> atts = fbos[f]->GetAttachments();
			for (size_t a = 0; a < atts.size(); a++)
				if (atts[a] != NULL && atts[a]->TexturePTR != NULL)
					live.insert(atts[a]->TexturePTR);
		}
		for (std::map<std::pair<Texture*, int32>, Preview>::iterator i = previews.begin(); i != previews.end(); )
		{
			if (live.find(i->first.first) == live.end())
			{
				delete i->second.preview;
				previews.erase(i++);
			}
			else ++i;
		}
	}

	ImGui::SliderFloat("Size", &thumbSize, 90.0f, 480.0f, "%.0f px");
	// Depth and cube previews are CPU readbacks and stall the pipeline, so
	// they refresh on a timer rather than every frame.
	ImGui::SliderFloat("Readback every", &refreshSeconds, 0.05f, 2.0f, "%.2f s");
	ImGui::Text("%d live framebuffer(s)", (int)fbos.size());
	ImGui::TextDisabled("Expand one to view it. Depth and cube previews are");
	ImGui::TextDisabled("CPU readbacks, so only expanded ones cost anything.");
	ImGui::Separator();

	if (fbos.empty())
		ImGui::TextDisabled("None yet.");

	for (size_t f = 0; f < fbos.size(); f++)
	{
		FrameBuffer *fbo = fbos[f];
		if (fbo == NULL)
			continue;
		const std::vector<FBOAttachment*> attachments = fbo->GetAttachments();

		ImGui::PushID((int)f);
		// Not "FBO f": the index is a slot in a list that changes as scenes,
		// documents and previews come and go, so show the GPU handle too -
		// that is what identifies it across frames. The debug name, when
		// whoever created it bothered to set one, is what identifies it to a
		// human.
		const std::string label = (fbo->GetDebugName().empty() ? std::string("Framebuffer") : fbo->GetDebugName())
			+ "  (#" + std::to_string(f)
			+ ", id " + std::to_string(fbo->GetBindID()) + ", "
			+ std::to_string(attachments.size()) + " attachment(s))";
		if (ImGui::CollapsingHeader(label.c_str(), 0))
		{
			// Attachments flow left to right and wrap, instead of stacking.
			// This panel docks into the bottom strip next to Assets and Log,
			// which is wide and short: a column of thumbnails puts one target
			// on screen and everything else behind a scrollbar, while the
			// space to the right of it sits empty. ImGui has no layout engine
			// to do this for us, so it is the manual wrap the demo window
			// uses - measure the card just emitted, and only SameLine() the
			// next one if it still fits before the content edge.
			//
			// rowRight is read once, here, with the cursor still at the left
			// margin: GetContentRegionAvail() is relative to the cursor, so
			// asking again mid-row would return the shrinking remainder
			// rather than the edge every card has to fit inside.
			const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
			const float cardW = thumbSize;
			size_t drawn = 0;

			for (size_t a = 0; a < attachments.size(); a++)
			{
				FBOAttachment *att = attachments[a];
				if (att == NULL)
					continue;
				ImGui::PushID((int)a);

				// Wrap decision for THIS card, measured off the previous one:
				// GetItemRectMax() still refers to the group just closed, so
				// the card goes on the same line only while there is room for
				// a whole card before rowRight. Narrow the panel far enough
				// and nothing fits, which puts one card per row - exactly the
				// column this replaced, reached the right way round.
				if (drawn > 0)
				{
					const float nextRight = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + cardW;
					if (nextRight <= rowRight)
						ImGui::SameLine();
				}
				drawn++;

				// One card = one group, so GetItemRectMax() above measures
				// the whole thing and not just its last line. Text inside
				// wraps at the card's width rather than pushing the card
				// wider than the thumbnail it is labelling.
				ImGui::BeginGroup();
				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cardW);

				// Depth is decided from the texture's data type, not from
				// att->NativeAttachmentFormat: that field holds a
				// backend-translated value (GL_DEPTH_ATTACHMENT = 36096 on
				// GL, the untranslated engine 16 on Vulkan), so comparing it
				// against the engine enum silently reported every GL depth
				// target as colour.
				Texture *tex = att->TexturePTR;
				const uint32 dataType = (tex != NULL) ? tex->GetDataType() : 0;
				const bool isDepth = (dataType == TextureDataType::DepthComponent ||
									  dataType == TextureDataType::DepthComponent16 ||
									  dataType == TextureDataType::DepthComponent24 ||
									  dataType == TextureDataType::DepthComponent32);
				const char *kind = isDepth ? "depth" : "colour";
				const float w = cardW;

				if (att->AttachmentType == FBOAttachmentType::RenderBuffer)
				{
					// A renderbuffer has no texture to sample at all.
					ImGui::Text("%d: %s renderbuffer", (int)a, kind);
					ImGui::TextDisabled("%ux%u", att->Width, att->Height);
					ImGui::TextDisabled("not sampleable");
				}
				else if (tex == NULL)
				{
					ImGui::Text("%d: %s", (int)a, kind);
					ImGui::TextDisabled("no texture");
				}
				else
				{
					// tex->GetTextureType(), NOT att->NativeTextureTarget: the
					// attachment stores the *native* target (GL_TEXTURE_2D,
					// 3553), while the device expects the engine enum.
					const bool isCube = (tex->GetTextureType() >= TextureType::CubemapPositive_X &&
										 tex->GetTextureType() <= TextureType::CubemapNegative_Z);
					const bool isMultisample = (tex->GetTextureType() == TextureType::Texture_Multisample);
					void *id = GetActiveRenderDevice().GetImGuiTextureID(tex->GetBindID(), tex->GetTextureType());
					ImGui::Text("%d: %s  %ux%u", (int)a, kind, tex->GetWidth(), tex->GetHeight());

					if (id != NULL && !isDepth)
					{
						// Fitted into a cardW square rather than drawn at
						// cardW wide: a 1884x1955 viewport and a 320x180
						// camera preview sitting side by side would otherwise
						// differ in height by a factor of three, and every
						// row would be as tall as its tallest member.
						const float tw = (float)tex->GetWidth(), th = (float)tex->GetHeight();
						const float aspect = (tw > 0.f) ? (th / tw) : 1.0f;
						ImVec2 size(w, w * aspect);
						if (size.y > w) { size.y = w; size.x = (aspect > 0.f) ? (w / aspect) : w; }
						// uv flipped vertically: render targets are written
						// bottom-up relative to how ImGui samples them.
						ImGui::Image((ImTextureID)id, size, ImVec2(0, 1), ImVec2(1, 0));
					}
					else if (isMultisample)
					{
						// Nothing can sample or read back a multisample image;
						// it needs resolving to a single-sample target first,
						// which is not this panel's job.
						ImGui::TextDisabled("(multisample - needs a resolve first)");
					}
					else if (isCube)
					{
						// Six faces, read back one at a time. Laid out 3 per
						// row rather than as a cross: this is for seeing
						// whether a face has content at all, not for judging
						// continuity across edges. Fixed at 3 columns so the
						// cube card stays one cardW wide like every other
						// card and the outer wrap keeps working.
						static const int32 kFaces[6] = {
							TextureType::CubemapPositive_X, TextureType::CubemapNegative_X,
							TextureType::CubemapPositive_Y, TextureType::CubemapNegative_Y,
							TextureType::CubemapPositive_Z, TextureType::CubemapNegative_Z };
						static const char *kNames[6] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
						const float fw = w / 3.2f;
						for (int fc = 0; fc < 6; fc++)
						{
							Preview *pv = GetPreview(tex, kFaces[fc], true);
							if (fc % 3 != 0) ImGui::SameLine();
							ImGui::BeginGroup();
							ImGui::TextDisabled("%s", kNames[fc]);
							void *pid = (pv != NULL && pv->preview != NULL)
								? GetActiveRenderDevice().GetImGuiTextureID(pv->preview->GetBindID(), TextureType::Texture)
								: NULL;
							if (pid != NULL)
								ImGui::Image((ImTextureID)pid, ImVec2(fw, fw));
							else
								ImGui::Dummy(ImVec2(fw, fw));
							ImGui::EndGroup();
						}
						Preview *any = GetPreview(tex, kFaces[0], false);
						if (any != NULL)
							ImGui::TextDisabled("%.4f .. %.4f", any->rangeMin, any->rangeMax);
					}
					else
					{
						// Depth: read back and normalised, because raw depth
						// is almost all values near 1.0 and draws as a flat
						// white rectangle.
						Preview *pv = GetPreview(tex, -1, true);
						void *pid = (pv != NULL && pv->preview != NULL)
							? GetActiveRenderDevice().GetImGuiTextureID(pv->preview->GetBindID(), TextureType::Texture)
							: NULL;
						if (pid != NULL)
						{
							ImGui::Image((ImTextureID)pid, ImVec2(w, w));
							ImGui::TextDisabled("%.4f .. %.4f", pv->rangeMin, pv->rangeMax);
						}
						else
						{
							ImGui::TextDisabled("(could not read back)");
						}
					}
				}

				ImGui::PopTextWrapPos();
				ImGui::EndGroup();
				ImGui::PopID();
			}
		}
		ImGui::PopID();
	}
	ImGui::End();
}
