//============================================================================
// Name        : ReflectionProbes.cpp
// Description : What a place looks like from inside it, for what shines there.
//============================================================================

#include <Pyros3D/Rendering/GI/ReflectionProbes.h>
#include <Pyros3D/Rendering/GI/EnvironmentMap.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <memory>

namespace p3d {

	namespace {
		// (the panorama a probe's six views are turned into before filtering)
		const uint32 kPanoW = 512, kPanoH = 256;

		struct Job
		{
			ReflectionProbe* probe;
			int32 slot;
			std::future<std::vector<f32> > filtered;      // the block, RGBA, kBlockWidth wide
		};

		std::vector<ReflectionProbe*> g_probes;
		std::vector<Job> g_jobs;
		std::vector<bool> g_slotUsed(ReflectionProbes::kMaxProbes, false);
		std::vector<f32> g_atlasPixels;                  // the whole atlas, as uploaded
		Texture* g_atlas = NULL;                         // (never freed: no GPU object may die at exit)
		bool g_atlasIsBlack = true;
		uint32 g_revision = 1;

		// What draws the six views, made the first time one is wanted.
		// (six of each: the views are drawn in one frame and read back at the start of the next -
		// read in the frame they are drawn in, Metal hands back what was there before, the frame's
		// own drawing not yet carried out; a frame later the queue's order has it done)
		ForwardRenderer* g_renderer = NULL;
		FrameBuffer* g_fbo[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
		Texture* g_faceColour[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
		GameObject* g_eye = NULL;
		// drawn last frame, to be read back
		ReflectionProbe* g_drawn = NULL;
		Vec3 g_drawnAt, g_drawnBackground;

		uint32 BlockHeight() { return RadianceStackHeight(ReflectionProbes::kBlockWidth); }
		uint32 AtlasW() { return ReflectionProbes::kColumns * ReflectionProbes::kBlockWidth; }
		uint32 AtlasH() { return (ReflectionProbes::kMaxProbes / ReflectionProbes::kColumns) * BlockHeight(); }

		// The six views: where each looks and which way is up in it.
		const Vec3 kFaceDir[6] = { Vec3(1, 0, 0), Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, -1, 0), Vec3(0, 0, 1), Vec3(0, 0, -1) };
		const Vec3 kFaceUp[6] = { Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 0, 1), Vec3(0, 0, -1), Vec3(0, 1, 0), Vec3(0, 1, 0) };
		// (the view's own axes, as Matrix::LookAt makes them: x right, y up, looking down -z)
		void FaceAxes(const uint32 f, Vec3 &x, Vec3 &y)
		{
			const Vec3 z = kFaceDir[f].negate();
			x = kFaceUp[f].cross(z).normalize();
			y = z.cross(x);
		}

		bool MakeCapturer()
		{
			if (g_renderer) return true;
			if (!IsActiveRenderDeviceSet()) return false;
			const uint32 n = ReflectionProbes::kFaceSize;
			g_renderer = new ForwardRenderer(n, n);
			for (uint32 f = 0; f < 6; f++)
			{
				g_faceColour[f] = new Texture();
				g_faceColour[f]->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, n, n, false);
				g_faceColour[f]->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
				g_fbo[f] = new FrameBuffer();
				g_fbo[f]->SetDebugName("Reflection probe view");
				g_fbo[f]->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, n, n);
				g_fbo[f]->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, g_faceColour[f]);
			}
			g_eye = new GameObject();
			return true;
		}

		// One view, drawn.
		void DrawFace(SceneGraph* scene, const Vec3 &at, const uint32 f)
		{
			const uint32 n = ReflectionProbes::kFaceSize;
			Matrix view;
			view.LookAt(at, at + kFaceDir[f], kFaceUp[f]);
			g_eye->SetTransformationMatrix(view.Inverse());
			g_eye->RefreshTransformation();
			Projection projection;
			projection.Perspective(90.f, 1.f, 0.05f, 1000.f);

			g_fbo[f]->Bind();
			g_renderer->ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
			g_renderer->EnableClearDepthBuffer();
			g_renderer->PreRender(g_eye, scene);
			g_renderer->RenderScene(projection, g_eye, scene);
			g_fbo[f]->UnBind();
		}
		// One view drawn the frame before, read back: RGBA floats, rows from the top.
		bool ReadFace(const uint32 f, std::vector<f32> &out)
		{
			const uint32 n = ReflectionProbes::kFaceSize;
			const std::vector<uchar> raw = g_faceColour[f]->GetTextureData(0);
			if (raw.size() < (size_t)n * n * 16) return false;       // (no readback here: GLES3)
			out.resize((size_t)n * n * 4);
			const bool topDown = GetActiveRenderDevice().RenderTargetOriginIsTopLeft();
			for (uint32 row = 0; row < n; row++)
			{
				const uint32 src = topDown ? row : (n - 1 - row);
				memcpy(&out[(size_t)row * n * 4], &raw[(size_t)src * n * 16], (size_t)n * 16);
			}
			return true;
		}

		// The six views as one equirect, the sky put where they drew nothing.
		void ToPanorama(const std::vector<f32> faces[6], const Vec3 &background, std::vector<f32> &pano)
		{
			const uint32 n = ReflectionProbes::kFaceSize;
			const f32 kPI = 3.14159265f;
			const bool sky = EnvironmentMap::IsLoaded();
			const f32 skyScale = EnvironmentMap::GetIntensity() * IRenderer::GetAmbientScale();
			pano.assign((size_t)kPanoW * kPanoH * 4, 0.f);
			Vec3 axX[6], axY[6];
			for (uint32 f = 0; f < 6; f++) FaceAxes(f, axX[f], axY[f]);
			for (uint32 py = 0; py < kPanoH; py++)
			{
				const f32 theta = ((f32)py + 0.5f) / kPanoH * kPI, st = sinf(theta), ct = cosf(theta);
				for (uint32 px = 0; px < kPanoW; px++)
				{
					// (the direction EnvSpecular's lookup puts at this texel: u = 0.5 + atan(x, -z) / 2pi)
					const f32 phi = (((f32)px + 0.5f) / kPanoW - 0.5f) * 2.f * kPI;
					const Vec3 d(st * sinf(phi), ct, -st * cosf(phi));
					uint32 f = 0; f32 best = -1e9f;
					for (uint32 k = 0; k < 6; k++) { const f32 c = d.dotProduct(kFaceDir[k]); if (c > best) { best = c; f = k; } }
					const f32 sx = d.dotProduct(axX[f]) / best, sy = d.dotProduct(axY[f]) / best;
					const f32 fx = (sx * 0.5f + 0.5f) * n - 0.5f, fy = (0.5f - sy * 0.5f) * n - 0.5f;
					const int32 x0 = std::max(0, std::min((int32)n - 1, (int32)floorf(fx))), y0 = std::max(0, std::min((int32)n - 1, (int32)floorf(fy)));
					const int32 x1 = std::min((int32)n - 1, x0 + 1), y1 = std::min((int32)n - 1, y0 + 1);
					const f32 ax = std::max(0.f, std::min(1.f, fx - x0)), ay = std::max(0.f, std::min(1.f, fy - y0));
					f32 c[4] = { 0, 0, 0, 0 };
					const int32 xs[2] = { x0, x1 }, ys[2] = { y0, y1 };
					for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++)
					{
						const f32 w = (i ? ax : 1.f - ax) * (j ? ay : 1.f - ay);
						const f32 *p = &faces[f][((size_t)ys[j] * n + xs[i]) * 4];
						for (int q = 0; q < 4; q++) c[q] += p[q] * w;
					}
					f32 *o = &pano[((size_t)py * kPanoW + px) * 4];
					// (drawn nothing there - alpha left at the clear's 0: the sky, or the background)
					Vec3 behind = background;
					if (c[3] < 0.999f)
					{
						Vec3 s;
						if (sky && EnvironmentMap::SampleRadiance(d, s)) behind = s * skyScale;
					}
					const f32 a = std::max(0.f, std::min(1.f, c[3]));
					// (colour is premultiplied by nothing: what was drawn is whole, the edge blends)
					o[0] = c[0] * a + behind.x * (1.f - a);
					o[1] = c[1] * a + behind.y * (1.f - a);
					o[2] = c[2] * a + behind.z * (1.f - a);
					o[3] = 1.f;
				}
			}
		}

		void UploadAtlas()
		{
			if (!g_atlas) g_atlas = new Texture();
			g_atlas->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, AtlasW(), AtlasH(), false);
			g_atlas->UpdateData(g_atlasPixels.data());
			g_atlas->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
			g_atlasIsBlack = false;
			g_revision++;
		}

		int32 TakeSlot()
		{
			for (uint32 i = 0; i < ReflectionProbes::kMaxProbes; i++)
				if (!g_slotUsed[i]) { g_slotUsed[i] = true; return (int32)i; }
			return -1;
		}

		bool Trace() { static const bool on = getenv("PYROS_PROBE_TRACE") != NULL; return on; }
	}

	// ------------------------------------------------------------------ the component

	ReflectionProbe::ReflectionProbe()
		: scene(NULL), extents(5.f, 3.f, 5.f), blend(1.f), wanted(true), ready(false), bounces(0), slot(-1)
	{
	}
	ReflectionProbe::~ReflectionProbe() { ReflectionProbes::Remove(this); }

	void ReflectionProbe::Register(SceneGraph* Scene) { scene = Scene; ReflectionProbes::Add(this); }
	void ReflectionProbe::Unregister(SceneGraph* Scene) { ReflectionProbes::Remove(this); scene = NULL; }

	void ReflectionProbe::SetExtents(const Vec3 &e)
	{
		extents = Vec3(std::max(e.x, 0.05f), std::max(e.y, 0.05f), std::max(e.z, 0.05f));
		g_revision++;
	}

	// ------------------------------------------------------------------ all of them

	void ReflectionProbes::Add(ReflectionProbe* p)
	{
		if (std::find(g_probes.begin(), g_probes.end(), p) == g_probes.end()) g_probes.push_back(p);
		p->wanted = true;
		p->bounces = 0;
		if (Trace()) fprintf(stderr, "[probe] added (scene %p, owner %p) - %zu now\n", (void*)p->scene, (void*)p->GetOwner(), g_probes.size());
		g_revision++;
	}

	void ReflectionProbes::Remove(ReflectionProbe* p)
	{
		std::vector<ReflectionProbe*>::iterator it = std::find(g_probes.begin(), g_probes.end(), p);
		if (it == g_probes.end()) return;
		g_probes.erase(it);
		// (a filtering still running for it finishes into nothing)
		for (size_t i = 0; i < g_jobs.size(); i++) if (g_jobs[i].probe == p) g_jobs[i].probe = NULL;
		if (g_drawn == p) g_drawn = NULL;
		if (p->slot >= 0) { g_slotUsed[p->slot] = false; p->slot = -1; }
		p->ready = false;
		g_revision++;
	}

	void ReflectionProbes::CaptureAll(SceneGraph* scene)
	{
		for (size_t i = 0; i < g_probes.size(); i++)
			if (scene == NULL || g_probes[i]->scene == scene) g_probes[i]->Capture();
	}

	bool ReflectionProbes::Update(SceneGraph* scene, const bool followMoves)
	{
		if (followMoves && g_drawn == NULL)
			for (size_t i = 0; i < g_probes.size(); i++)
			{
				ReflectionProbe* p = g_probes[i];
				if (!p->ready || p->wanted || p->scene != scene || !p->GetOwner()) continue;
				const Vec3 d = p->GetOwner()->GetWorldPosition() - p->capturedAt;
				if (d.x * d.x + d.y * d.y + d.z * d.z > 1e-4f) p->Capture();
			}
		// What has finished filtering goes into the atlas.
		for (size_t i = 0; i < g_jobs.size(); )
		{
			if (g_jobs[i].filtered.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { i++; continue; }
			std::vector<f32> block = g_jobs[i].filtered.get();
			ReflectionProbe* p = g_jobs[i].probe;
			const int32 slot = g_jobs[i].slot;
			g_jobs.erase(g_jobs.begin() + i);
			if (p == NULL || p->slot != slot || block.empty()) continue;
			if (g_atlasPixels.empty()) g_atlasPixels.assign((size_t)AtlasW() * AtlasH() * 4, 0.f);
			const uint32 bw = kBlockWidth, bh = BlockHeight();
			const uint32 ox = (uint32)(slot % kColumns) * bw, oy = (uint32)(slot / kColumns) * bh;
			for (uint32 y = 0; y < bh; y++)
				memcpy(&g_atlasPixels[((size_t)(oy + y) * AtlasW() + ox) * 4], &block[(size_t)y * bw * 4], (size_t)bw * 16);
			UploadAtlas();
			p->ready = true;
			// Seen once more, now that it is in the atlas: what is shiny in it (a metal, a mirror) was
			// drawn reflecting nothing - black - the first time, and reflects the room the second.
			if (++p->bounces < 2) p->wanted = true;
			if (Trace()) fprintf(stderr, "[probe] block %d in the atlas\n", slot);
		}

		// What was drawn last frame is read back, made one picture and handed to be filtered.
		if (g_drawn != NULL)
		{
			ReflectionProbe* p = g_drawn;
			g_drawn = NULL;
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			std::vector<f32> faces[6];
			bool ok = std::find(g_probes.begin(), g_probes.end(), p) != g_probes.end();
			for (uint32 f = 0; f < 6 && ok; f++) ok = ReadFace(f, faces[f]);
			if (ok)
			{
				std::vector<f32> pano;
				ToPanorama(faces, g_drawnBackground, pano);
				// PYROS_PROBE_DUMP=<dir>: the six views and the panorama, raw RGBA floats, for looking at
				if (const char* dir = getenv("PYROS_PROBE_DUMP"))
				{
					for (uint32 f = 0; f < 6; f++)
					{
						FILE* o = fopen((std::string(dir) + "/face" + std::to_string(f) + ".f32").c_str(), "wb");
						if (o) { fwrite(faces[f].data(), 4, faces[f].size(), o); fclose(o); }
					}
					FILE* o = fopen((std::string(dir) + "/pano.f32").c_str(), "wb");
					if (o) { fwrite(pano.data(), 4, pano.size(), o); fclose(o); }
				}
				p->capturedAt = g_drawnAt;
				Job j;
				j.probe = p;
				j.slot = p->slot;
				j.filtered = std::async(std::launch::async, [pano]() {
					std::vector<f32> block;
					BuildRadianceStack(pano.data(), kPanoW, kPanoH, 4, ReflectionProbes::kBlockWidth, block);
					return block;
				});
				g_jobs.push_back(std::move(j));
			}
			if (Trace()) fprintf(stderr, "[probe] read back in %.1f ms%s\n",
				std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count(), ok ? "" : " - no readback here");
			return true;
		}

		// The next one waiting is drawn (and read back next frame).
		if (Trace()) { static int said = 0; if (said++ < 3) fprintf(stderr, "[probe] update: scene %p, %zu probes, %u pending\n", (void*)scene, g_probes.size(), Pending()); }
		ReflectionProbe* next = NULL;
		for (size_t i = 0; i < g_probes.size() && !next; i++)
			if (g_probes[i]->wanted && g_probes[i]->scene == scene && g_probes[i]->GetOwner() != NULL) next = g_probes[i];
		if (next && MakeCapturer())
		{
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			next->wanted = false;
			if (next->slot < 0) next->slot = TakeSlot();
			if (next->slot < 0)
			{
				static bool said = false;
				if (!said) { said = true; echo("WARNING: more than " + std::to_string(kMaxProbes) + " reflection probes - the rest are not captured"); }
			}
			else
			{
				const Vec3 at = next->GetOwner()->GetWorldPosition();
				// (the clear colour is everyone's: put back after)
				const Vec4 callerClear = GetActiveRenderDevice().GetClearColor();
				g_renderer->SetBackground(Vec4(callerClear.x, callerClear.y, callerClear.z, 0.f));
				for (uint32 f = 0; f < 6; f++) DrawFace(scene, at, f);
				GetActiveRenderDevice().SetClearColor(callerClear);
				g_drawn = next;
				g_drawnAt = at;
				g_drawnBackground = Vec3(callerClear.x, callerClear.y, callerClear.z);
				if (Trace()) fprintf(stderr, "[probe] drawn from (%.2f, %.2f, %.2f) in %.1f ms\n", at.x, at.y, at.z,
					std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count());
			}
		}
		return Pending() > 0;
	}

	Texture* ReflectionProbes::GetAtlas()
	{
		if (!g_atlas)
		{
			g_atlas = new Texture();
			const f32 black[4] = { 0.f, 0.f, 0.f, 0.f };
			g_atlas->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, 1, 1, false);
			g_atlas->UpdateData((void*)black);
			g_atlas->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		}
		return g_atlas;
	}

	void ReflectionProbes::FillBlock(Vec4 out[kBlockVec4s])
	{
		for (uint32 i = 0; i < kBlockVec4s; i++) out[i] = Vec4(0.f, 0.f, 0.f, 0.f);
		uint32 n = 0;
		// (the smallest boxes first: a pixel takes the first it is inside)
		std::vector<ReflectionProbe*> order;
		for (size_t i = 0; i < g_probes.size(); i++)
			if (g_probes[i]->IsCaptured() && g_probes[i]->GetOwner() != NULL) order.push_back(g_probes[i]);
		std::sort(order.begin(), order.end(), [](ReflectionProbe* a, ReflectionProbe* b) {
			return a->GetExtents().x * a->GetExtents().y * a->GetExtents().z < b->GetExtents().x * b->GetExtents().y * b->GetExtents().z; });
		for (size_t i = 0; i < order.size() && n < kMaxProbes; i++, n++)
		{
			ReflectionProbe* p = order[i];
			// (centred where it was seen from: a probe moved is seen again, and its box moves then)
			const Vec3 c = p->capturedAt, e = p->GetExtents();
			out[1 + n * 3 + 0] = Vec4(c.x - e.x, c.y - e.y, c.z - e.z, p->GetBlend());
			out[1 + n * 3 + 1] = Vec4(c.x + e.x, c.y + e.y, c.z + e.z, (f32)p->slot);
			out[1 + n * 3 + 2] = Vec4(p->capturedAt.x, p->capturedAt.y, p->capturedAt.z, 0.f);
		}
		// x: how many; y: the atlas's width in texels, z: one block's height (the shader works out
		// where a block and its levels are from these)
		out[0] = Vec4((f32)n, (f32)AtlasW(), (f32)BlockHeight(), g_atlasIsBlack ? 0.f : 1.f);
	}

	uint32 ReflectionProbes::Revision() { return g_revision; }

	uint32 ReflectionProbes::Count()
	{
		uint32 n = 0;
		for (size_t i = 0; i < g_probes.size(); i++) if (g_probes[i]->IsCaptured()) n++;
		return n;
	}

	uint32 ReflectionProbes::Pending()
	{
		uint32 n = (uint32)g_jobs.size() + (g_drawn ? 1u : 0u);
		for (size_t i = 0; i < g_probes.size(); i++) if (g_probes[i]->wanted) n++;
		return n;
	}
}
