//============================================================================
// Name        : ImpostorBaker.cpp
// Author      : Duarte Peixinho
// Description : Octahedral impostors made by the engine - see ImpostorBaker.h
//============================================================================

#include <Pyros3D/Rendering/Renderer/ImpostorBaker.h>
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Utils/Bindings/PyrosEmbind.h>
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/ImpostorQuad.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace p3d {

	bool BakeOctahedralImpostor(gfx::Device* device, SceneRenderer &renderer, const std::shared_ptr<GameObject> &subject,
		const uint32 frames, const uint32 size, const bool hostFrame, OctahedralImpostorBake &out, std::string &err, const bool fullSphere,
		const uint32 firstView, const uint32 viewCount, const int onlyPhase, std::vector<gfx::Texture>* keepTargets, std::vector<std::vector<uint8_t> >* readBackIn)
	{
		if (!device || !subject) { err = "no device or nothing to bake"; return false; }
		const uint32 N = std::max(2u, std::min(16u, frames));
		const uint32 F = std::max(32u, std::min(1024u, size));
		// A view of its own, the light switched off for the surface's outputs - see the loop
		SceneRenderer* r2r = &renderer;
		// (the view's target: the device's backbuffer format, as an editor view's - the bytes read back are what was said)
		gfx::TextureDesc td; td.width = td.height = F; td.format = device->BackbufferFormat();
		td.usage = gfx::TexColorTarget | gfx::TexSampled | gfx::TexCopySrc; td.debugName = "Renderer.ImpostorBake";
		const gfx::Texture target = device->CreateTexture(td);
		auto draw = [&](SceneGraph* sc, View vw, const gfx::Texture into) { vw.target = into; vw.hostFrame = hostFrame; r2r->Render(sc, vw); };
		// (each view's three pictures into targets of their own - all drawn first, then all read back: read back one by one,
		// each read waited for the GPU to finish the draw just made, 20 ms apiece, 432 times a bake)
		std::vector<gfx::Texture> targets;
		auto targetFor = [&](const size_t i) { while (targets.size() <= i) targets.push_back(device->CreateTexture(td)); return targets[i]; };
		r2r->SetToneCurve(false); r2r->SetAutoExposure(false, 0.f); r2r->SetTemporalAA(false); r2r->SetBackground(Vec3(0.f, 0.f, 0.f));

		SceneGraph* scene = new SceneGraph();
		scene->Add(subject);
		std::shared_ptr<GameObject> cam = std::make_shared<GameObject>();
		scene->Add(cam);
		scene->Update(0);

		// Its bounds, in its own space: every renderable's box through its owner's transform
		Vec3 bmin, bmax; bool any = false;
		std::function<void(GameObject*)> visit = [&](GameObject* g) {
			for (size_t c = 0; c < g->GetComponents().size(); c++)
				if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(g->GetComponents()[c].get()))
				{
					if (!rc->GetRenderable()) continue;
					const Vec3 lo = rc->GetRenderable()->GetBoundingMinValue(), hi = rc->GetRenderable()->GetBoundingMaxValue();
					const Matrix m = g->GetWorldTransformation();
					for (int k = 0; k < 8; k++)
					{
						const Vec3 p = m * Vec3(k & 1 ? hi.x : lo.x, k & 2 ? hi.y : lo.y, k & 4 ? hi.z : lo.z);
						if (!any) { bmin = bmax = p; any = true; }
						bmin = Vec3(Min(bmin.x, p.x), Min(bmin.y, p.y), Min(bmin.z, p.z));
						bmax = Vec3(Max(bmax.x, p.x), Max(bmax.y, p.y), Max(bmax.z, p.z));
					}
				}
			for (size_t i = 0; i < g->GetChildren().size(); i++) visit(g->GetChildren()[i].get());
		};
		visit(subject.get());
		if (!any) { delete scene; device->Destroy(target); err = "nothing to bake"; return false; }
		// on its own upright axis (what an instance turns about), at half its height
		const f32 centreY = 0.5f * (bmin.y + bmax.y);
		const Vec3 C(0.f, centreY, 0.f);
		// (the farthest point of the tree itself from the centre, not its box's corner: a round crown's box
		// corners stand a third further out, and every view would spend that much of its picture on nothing)
		f32 radius = 0.f;
		std::function<void(GameObject*)> reach = [&](GameObject* g) {
			const Matrix m = g->GetWorldTransformation();
			for (size_t c = 0; c < g->GetComponents().size(); c++)
				if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(g->GetComponents()[c].get()))
					if (rc->GetRenderable())
						for (size_t gi = 0; gi < rc->GetRenderable()->Geometries.size(); gi++)
						{
							const std::vector<Vec3> &v = rc->GetRenderable()->Geometries[gi]->GetVertexData();
							for (size_t k = 0; k < v.size(); k++) radius = Max(radius, (m * v[k] - C).magnitude());
						}
			for (size_t i = 0; i < g->GetChildren().size(); i++) reach(g->GetChildren()[i].get());
		};
		reach(subject.get());
		if (radius <= 0.f)
			for (int k = 0; k < 8; k++)
			{
				const Vec3 p(k & 1 ? bmax.x : bmin.x, k & 2 ? bmax.y : bmin.y, k & 4 ? bmax.z : bmin.z);
				radius = Max(radius, (p - C).magnitude());
			}
		radius *= 1.02f;   // (a leaf's edge, filtered, not cut off)

		auto hemiOctDecode = [](f32 ex, f32 ey) {
			const f32 tx = (ex + ey) * 0.5f, ty = (ex - ey) * 0.5f;
			return Vec3(tx, 1.f - fabsf(tx) - fabsf(ty), ty).normalize();
		};
		auto halfToFloat = [](uint16_t h) -> f32 {
			const uint32 s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
			uint32 f;
			if (e == 0) { if (m == 0) f = s << 31; else { int ee = -1; uint32 mm = m; do { ee++; mm <<= 1; } while (!(mm & 1024)); f = (s << 31) | ((uint32)(127 - 15 - ee) << 23) | ((mm & 1023) << 13); } }
			else if (e == 31) f = (s << 31) | 0x7f800000 | (m << 13);
			else f = (s << 31) | ((e - 15 + 127) << 23) | (m << 13);
			f32 r; memcpy(&r, &f, 4); return r;
		};
		auto toSrgb8 = [](f32 c) {
			c = Max(0.f, Min(1.f, c));
			c = c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.f / 2.4f) - 0.055f;
			return (unsigned char)(c * 255.f + 0.5f);
		};

		const uint32 A = N * F;
		// (the atlases kept in `out` from one call to the next: a bake spread over frames - firstView, viewCount - fills
		// them a few views at a time)
		const bool firstCall = firstView == 0 || out.albedo.size() != (size_t)A * A * 4;
		if (firstCall) { out.albedo.assign((size_t)A * A * 4, 0); out.normals.assign((size_t)A * A * 4, 0); }
		std::vector<unsigned char> &albedo = out.albedo, &normals = out.normals;
		const uint32 lastView = std::min(N * N, firstView + std::min(viewCount, N * N));
		bool ok = true, notReady = false;
		std::vector<std::vector<uint8_t> > readBack;
		// (onlyPhase 0: drawn only, into *keepTargets, for the caller to read back; 1: taken in only, from *readBackIn)
		if (onlyPhase == 0 && keepTargets) targets.swap(*keepTargets);
		for (int phase = onlyPhase == 1 ? 1 : 0; phase < (onlyPhase == 0 ? 1 : 2) && ok; phase++)     // (0: every view drawn; 1: every view read back and taken in)
		{
		// (all of them read back at once, between the two: one submission, one wait - or read already, by the caller)
		if (phase == 1 && readBackIn) readBack.swap(*readBackIn);
		else if (phase == 1 && !targets.empty() && !device->ReadTextures(targets.data(), (uint32_t)targets.size(), readBack)) { ok = false; err = "read back failed"; break; }
		for (uint32 fy = 0; fy < N && ok && !notReady; fy++)
			for (uint32 fx = 0; fx < N && ok && !notReady; fx++)
			{
				if (fy * N + fx < firstView || fy * N + fx >= lastView) continue;     // (another call's views)
				const size_t slot = (size_t)(fy * N + fx - firstView) * 3;
				// the frame's direction (grid cell -> -1..1 -> hemi-octahedral)
				const f32 ex = (f32)fx / (f32)(N - 1) * 2.f - 1.f, ey = (f32)fy / (f32)(N - 1) * 2.f - 1.f;
				// (the frame's way: the upper half's, or - a full sphere - the octahedron's, its lower half folded to the corners)
				Vec3 d;
				if (fullSphere)
				{
					Vec3 o(ex, 1.f - fabsf(ex) - fabsf(ey), ey);
					if (o.y < 0.f) { const f32 ox = o.x; o.x = (1.f - fabsf(o.z)) * (ox >= 0.f ? 1.f : -1.f); o.z = (1.f - fabsf(ox)) * (o.z >= 0.f ? 1.f : -1.f); }
					d = o.normalize();
				}
				else d = hemiOctDecode(ex, ey);
				const Vec3 up0 = fabsf(d.y) > 0.999f ? Vec3(0.f, 0.f, -1.f) : Vec3(0.f, 1.f, 0.f);
				const Vec3 r = up0.cross(d).normalize();
				const Vec3 u = d.cross(r);
				const f32 dist = radius * 3.f;
				const Vec3 eye = C + d * dist;
				// the camera's frame written out (right r, up u, looking down -d): the same basis the shader
				// rebuilds, whatever LookAt's own convention is
				Matrix world;
				world.identity();
				world.m[0] = r.x; world.m[1] = r.y; world.m[2] = r.z;
				world.m[4] = u.x; world.m[5] = u.y; world.m[6] = u.z;
				world.m[8] = d.x; world.m[9] = d.y; world.m[10] = d.z;
				world.m[12] = eye.x; world.m[13] = eye.y; world.m[14] = eye.z;
				cam->SetTransformationMatrix(world);
				scene->Update(0);
				// This view's surface, in one form whichever renderer drew it: its albedo (sRGB bytes), its normal in view space
				// (x right, y up, z toward the camera; zero where nothing was drawn) and its depth across the view (0 near, 1 far
				// or nothing) - rows from the top
				std::vector<unsigned char> albB((size_t)F * F * 3, 0);
				std::vector<Vec3> nrmV((size_t)F * F, Vec3(0.f, 0.f, 0.f));
				std::vector<f32> depV((size_t)F * F, 1.f);
				bool haveDepth = false;
				{
					// (three views of the surface's own outputs - SetDebugView 6, 7, 8 - on black, read back)
					View v;
					v.cameraWorld = world; v.orthographic = true; v.orthoHeight = radius * 2.f;
					v.nearZ = dist - radius * 1.1f; v.farZ = dist + radius * 1.1f; v.width = v.height = F;
					std::vector<uint8_t> px[3];
					for (int k = 0; k < 3 && ok; k++)
					{
						if (phase == 1)
						{
							if (slot + (size_t)k >= readBack.size() || readBack[slot + k].size() != (size_t)F * F * 4) { ok = false; err = "read back failed"; }
							else px[k].swap(readBack[slot + k]);
							continue;
						}
						const gfx::Texture into = targetFor(slot + (size_t)k);
						r2r->SetDebugView(6 + k);
						// (drawn again, a moment apart, while any of it was left undrawn: its pipelines are made as it is first drawn,
						// and until they are its draws are skipped - the first view came out black, on WebGPU every view)
						draw(scene, v, into);
						// (a bake spread over frames does not wait: the slice is given up, its pipelines left to be made, and
						// drawn again next frame - waited for here, the first slice stood still the game 1.5 s on Vulkan)
						if (onlyPhase == 0 && r2r->PendingDraws() > 0) { device->PollPipelines(); notReady = true; break; }
						for (int again = 0; again < 600 && r2r->PendingDraws() > 0; again++)
						{
							device->PollPipelines();
							std::this_thread::sleep_for(std::chrono::milliseconds(5));
							draw(scene, v, into);
						}
					}
					if (phase == 0 || notReady) continue;     // (drawn: read back in the second pass)
					if (!ok) break;
					const gfx::Format bf = device->BackbufferFormat();
					const bool bgra = bf == gfx::Format::BGRA8 || bf == gfx::Format::BGRA8_sRGB;
					for (size_t t = 0; t < (size_t)F * F; t++)
					{
						const uint8_t* n8 = &px[1][t * 4];
						const f32 nx = (bgra ? n8[2] : n8[0]) / 255.f, ny = n8[1] / 255.f, nz = (bgra ? n8[0] : n8[2]) / 255.f;
						if (nx + ny + nz < 0.02f) continue;     // (the black behind: nothing drawn)
						nrmV[t] = Vec3(nx * 2.f - 1.f, ny * 2.f - 1.f, nz * 2.f - 1.f);
						const uint8_t* a8 = &px[0][t * 4];
						albB[t * 3] = bgra ? a8[2] : a8[0]; albB[t * 3 + 1] = a8[1]; albB[t * 3 + 2] = bgra ? a8[0] : a8[2];
						depV[t] = px[2][t * 4 + 1] / 255.f;
					}
					haveDepth = true;
				}
				// The crown's own shadow, the same whatever the sun: how much of the leaves round a point stand
				// in front of it, from this view's depth (orthographic: linear, 0..1 across 2.2 radii). A leaf deep
				// in the crown, seen through a gap, is mostly shut in; one on the outside is open. (Without it an
				// impostor was every leaf in full sun - flat and lighter than the tree it stood for.)
				std::vector<f32> occ((size_t)F * F, 1.f);
				if (haveDepth)
				{
					auto depthAt = [&](int x, int y) {
						x = std::max(0, std::min((int)F - 1, x)); y = std::max(0, std::min((int)F - 1, y));
						return depV[(size_t)y * F + x];
					};
					const int R = std::max(2, (int)(F * 0.06f));                 // (a disc of an eighth of the radius... in pixels)
					const f32 nearDz = 0.04f / 2.2f, farDz = 0.6f / 2.2f;        // (in front by 0.04 to 0.6 radii counts - not near/far: Windows' headers make those empty macros)
					static const f32 kDisc[16][2] = { {0.18f,0.05f},{-0.31f,0.22f},{0.07f,-0.44f},{0.52f,0.33f},{-0.58f,-0.27f},{0.29f,-0.71f},{-0.12f,0.79f},{0.81f,-0.18f},
						{-0.86f,0.31f},{0.45f,0.82f},{-0.47f,-0.80f},{0.95f,0.21f},{-0.22f,-0.97f},{-0.93f,-0.05f},{0.69f,-0.69f},{-0.62f,0.73f} };
					for (int y = 0; y < (int)F; y++)
						for (int x = 0; x < (int)F; x++)
						{
							const f32 z = depV[(size_t)y * F + x];
							if (z >= 0.9999f) continue;
							int shut = 0;
							for (int k = 0; k < 16; k++)
							{
								const f32 dzk = z - depthAt(x + (int)(kDisc[k][0] * R), y + (int)(kDisc[k][1] * R));
								if (dzk > nearDz && dzk < farDz) shut++;
							}
							occ[(size_t)y * F + x] = 1.f - (f32)shut / 16.f;
						}
				}
				else if (fy == 0 && fx == 0) fprintf(stderr, "bake_octahedral_impostor: depth not read back - no crown occlusion\n");
				for (uint32 y = 0; y < F; y++)
					for (uint32 x = 0; x < F; x++)
					{
						const size_t t = (size_t)y * F + x;
						const Vec3 nv = nrmV[t];
						const f32 len = nv.magnitude();
						const size_t di = ((size_t)(fy * F + y) * A + (fx * F + x)) * 4;
						if (len < 0.5f) continue;            // (nothing drawn: left clear - the loader bleeds colour into it)
						// view space (x right, y up, z toward the camera) into the object's own frame
						const Vec3 n = (r * nv.x + u * nv.y + d * nv.z) * (1.f / len);
						albedo[di] = albB[t * 3]; albedo[di + 1] = albB[t * 3 + 1]; albedo[di + 2] = albB[t * 3 + 2];
						albedo[di + 3] = 255;
						normals[di] = (unsigned char)((n.x * 0.5f + 0.5f) * 255.f + 0.5f);
						normals[di + 1] = (unsigned char)((n.y * 0.5f + 0.5f) * 255.f + 0.5f);
						normals[di + 2] = (unsigned char)((n.z * 0.5f + 0.5f) * 255.f + 0.5f);
						// (alpha: the crown's occlusion - for an impostor's shading to darken the leaves by)
						normals[di + 3] = (unsigned char)(Max(0.f, Min(1.f, occ[t])) * 255.f + 0.5f);
					}
			}
		}
		delete scene;
		device->Destroy(target);
		if (onlyPhase == 0 && keepTargets && !notReady) keepTargets->swap(targets);     // (not ready: none kept - the caller tries again)
		for (gfx::Texture t : targets) device->Destroy(t);
		if (!ok) return false;
		if (onlyPhase == 0) return true;     // (drawn: taken in by a later call)
		out.frames = N; out.size = F; out.radius = radius; out.centre = centreY; out.fullSphere = fullSphere;
		if (lastView < N * N) return true;     // (more views to come, in later calls)
		// (nothing drawn into it - every view left undrawn - is no impostor: a quad of nothing, where the model was)
		size_t covered = 0;
		for (size_t t = 3; t < albedo.size(); t += 4) covered += albedo[t] != 0;
		if (covered == 0) { err = "nothing was drawn (its pipelines never became ready)"; return false; }
		return true;
	}

	namespace
	{
		struct BankItem { int state = 0; ImpostorBank::Entry entry; std::string path; std::shared_ptr<IMaterial> material; };     // (state: 0 asked, 1 made, 2 could not be)
		std::mutex bankMutex;
		std::map<std::string, BankItem> bankItems;     // (by MaterialKey: the model and the material's values)
		std::vector<std::string> bankQueue;
		// What an impostor of the model looks like through this material: its colour, maps and shine - not which object it
		// is (ten thousand teapots of one grey, each its own material, are one impostor)
		std::string MaterialKey(const std::string &path, const std::shared_ptr<IMaterial> &material)
		{
			std::string k = path;
			const GenericShaderMaterial* gm = dynamic_cast<const GenericShaderMaterial*>(material.get());
			if (!gm) return material ? k + "|" + std::to_string((unsigned long long)(uintptr_t)material.get()) : k;
			char b[256];
			const Vec4 &c = gm->GetColor(), &sp = gm->GetSpecular();
			snprintf(b, sizeof(b), "|%llx|%.4f,%.4f,%.4f,%.4f|%.4f,%.4f,%.4f|%.3f,%.3f", (unsigned long long)gm->GetOptions(), c.x, c.y, c.z, c.w, sp.x, sp.y, sp.z, gm->GetRoughness(), gm->GetMetallic());
			k += b;
			if (gm->GetColorMap()) k += "|" + gm->GetColorMap()->GetFilename();
			return k;
		}
		bool bankEnabled = !(std::getenv("PYROS_AUTO_IMPOSTOR") && std::getenv("PYROS_AUTO_IMPOSTOR")[0] == '0');
		const uint32 kBankFrames = 12, kBankSize = 128;     // (a full sphere: 144 views, every way round - 1536 square)

		std::string BankCacheDir()
		{
			namespace fs = std::filesystem;
			std::error_code ec;
			const char* env = std::getenv("PYROS_IMPOSTOR_CACHE");
			const fs::path dir = env && env[0] ? fs::path(env) : fs::temp_directory_path(ec) / "pyros3d-impostors";
			fs::create_directories(dir, ec);
			return dir.string();
		}
		// (the model's file, as it is now: its path, size and time - a changed model is baked again)
		std::string BankKey(const std::string &path, const std::string &look)
		{
			namespace fs = std::filesystem;
			std::error_code ec;
			uint64 h = 14695981039346656037ull;
			auto mix = [&h](const std::string &s) { for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; } };
			mix(path); mix(std::to_string((unsigned long long)fs::file_size(path, ec)));
			mix(std::to_string((long long)fs::last_write_time(path, ec).time_since_epoch().count()));
			mix(look); mix("oct-v3-sphere-" + std::to_string(kBankFrames) + "-" + std::to_string(kBankSize));
			char k[24]; snprintf(k, sizeof(k), "%016llx", (unsigned long long)h);
			return BankCacheDir() + "/" + k;
		}
		bool MakeEntry(ImpostorBank::Entry &e, const std::string &base, const f32 radius, const f32 centre, const uint32 frames, const std::shared_ptr<IMaterial> &source)
		{
			e.albedo = std::make_shared<Texture>(); e.normal = std::make_shared<Texture>();
			if (!e.albedo->LoadTexture(base + "_albedo.png", TextureType::Texture, true) || !e.normal->LoadTexture(base + "_normal.png", TextureType::Texture, true))
				return false;
			e.material = std::make_shared<GenericShaderMaterial>(ShaderUsage::Texture | ShaderUsage::Diffuse | ShaderUsage::SpecularColor | ShaderUsage::AlphaTest | ShaderUsage::Impostor);
			e.material->SetColorMap(e.albedo);
			e.material->SetNormalMap(e.normal);
			// (its shine as the object's own material has it - the colour is in the atlas)
			const GenericShaderMaterial* src = dynamic_cast<const GenericShaderMaterial*>(source.get());
			if (src) { e.material->SetSpecular(src->GetSpecular()); e.material->SetRoughness(src->GetRoughness()); e.material->SetMetallic(src->GetMetallic()); }
			else e.material->SetSpecular(Vec4(0.25f, 0.25f, 0.25f, 1.f));
			e.material->SetImpostorFullSphere(true);     // (the bank bakes every way round: what it is given may be seen from below)
			e.material->SetImpostor((f32)frames, radius, centre);
			e.material->SetAlphaCutoff(0.5f);
			e.material->SetCullFace(CullFace::DoubleSided);
			e.quad = std::make_shared<ImpostorQuad>(radius, centre);
			e.radius = radius; e.centre = centre;
			return true;
		}
		bool LoadCached(const std::string &path, const std::string &look, const std::shared_ptr<IMaterial> &material, ImpostorBank::Entry &e)
		{
			const std::string base = BankKey(path, look);
			std::ifstream in(base + ".txt");
			f32 radius = 0.f, centre = 0.f; uint32 frames = 0;
			if (!(in >> radius >> centre >> frames) || radius <= 0.f || frames < 2) return false;
			return MakeEntry(e, base, radius, centre, frames, material);
		}
	}

	bool ImpostorBank::Enabled() { return bankEnabled; }
	void ImpostorBank::SetEnabled(const bool on) { bankEnabled = on; }
	bool ImpostorBank::AnyPending() { std::lock_guard<std::mutex> l(bankMutex); return !bankQueue.empty(); }

	const ImpostorBank::Entry* ImpostorBank::Request(const std::string &modelPath, const std::shared_ptr<IMaterial> &material)
	{
		if (!bankEnabled || modelPath.empty()) return nullptr;
		const std::string key = MaterialKey(modelPath, material);
		std::lock_guard<std::mutex> l(bankMutex);
		std::map<std::string, BankItem>::iterator i = bankItems.find(key);
		if (i == bankItems.end())
		{
			i = bankItems.insert(std::make_pair(key, BankItem())).first;
			i->second.path = modelPath; i->second.material = material;
			if (LoadCached(modelPath, key, material, i->second.entry)) i->second.state = 1;      // (made on an earlier run)
			else bankQueue.push_back(key);
		}
		return i->second.state == 1 ? &i->second.entry : nullptr;
	}

	namespace
	{
		// The bake under way: a row of views a frame - a stall of seconds in one frame, before - its model loaded once
		struct BankJob
		{
			std::string key, path;
			std::shared_ptr<IMaterial> material;
			std::shared_ptr<Model> mesh;
			OctahedralImpostorBake bake;
			uint32 nextView = 0;
			// (a slice drawn, its reads in flight: its targets, the device's ticket for them, its first view)
			std::vector<gfx::Texture> targets;
			uint64_t ticket = 0;
			uint32 readingView = 0, unready = 0;     // (unready: frames its pipelines were still being made)
			f64 ms = 0.0, worst = 0.0;     // (its work in all, and the most of it in one frame)
			bool active = false;
		};
		BankJob bankJob;
		const uint32 kBankViewsAFrame = 4;     // (a third of a row a frame: 36 frames for a full sphere, each a small share)
	}

	void ImpostorBank::BakePending(gfx::Device* device, std::unique_ptr<SceneRenderer> &renderer)
	{
		if (!device) return;
		BankJob &job = bankJob;
		if (!job.active)
		{
			std::lock_guard<std::mutex> l(bankMutex);
			if (bankQueue.empty()) return;
			job = BankJob();
			job.key = bankQueue.front();
			job.path = bankItems[job.key].path; job.material = bankItems[job.key].material;
			job.active = true;
		}
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		if (!renderer) { renderer.reset(new SceneRenderer(device)); renderer->SetName("Impostor bakes"); }
		if (!job.mesh) job.mesh = std::make_shared<Model>(job.path, true);
		std::string err;
		bool ok = job.mesh && !job.mesh->Geometries.empty();
		if (ok)
		{
			std::shared_ptr<GameObject> subject = std::make_shared<GameObject>();
			// (through the object's own material where it has one - the colour it is drawn in - else the model's own)
			if (job.material) subject->Add(std::make_shared<RenderingComponent>(job.mesh, job.material));
			else subject->Add(std::make_shared<RenderingComponent>(job.mesh, ShaderUsage::Diffuse | ShaderUsage::SpecularColor));
			// A slice a frame, its reads not waited for: drawn, the copies of it submitted, and taken in a frame or two on,
			// when the GPU has got to them - the next slice drawn as the last is taken in. (Read at once, each slice
			// waited for everything the GPU had before it, the frame too: 30 to 80 ms a frame for the whole bake.)
			if (job.ticket)
			{
				std::vector<std::vector<uint8_t> > got;
				if (device->ReadTexturesDone(job.ticket, got))
				{
					ok = BakeOctahedralImpostor(device, *renderer, subject, kBankFrames, kBankSize, true, job.bake, err, true, job.readingView, kBankViewsAFrame, 1, nullptr, &got);
					for (gfx::Texture t : job.targets) device->Destroy(t);
					job.targets.clear(); job.ticket = 0;
				}
			}
			if (ok && !job.ticket && job.nextView < kBankFrames * kBankFrames)
			{
				ok = BakeOctahedralImpostor(device, *renderer, subject, kBankFrames, kBankSize, true, job.bake, err, true, job.nextView, kBankViewsAFrame, 0, &job.targets);
				if (ok && job.targets.empty() && ++job.unready > 1200) { ok = false; err = "its pipelines were never made"; }
				if (ok && !job.targets.empty())     // (none: its pipelines still being made - the same slice next frame)
				{
					job.ticket = device->ReadTexturesLater(job.targets.data(), (uint32_t)job.targets.size());
					if (!job.ticket) { ok = false; err = "read back failed"; }
					job.readingView = job.nextView;
					job.nextView += kBankViewsAFrame;
				}
			}
		}
		const f64 spent = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
		job.ms += spent; job.worst = std::max(job.worst, spent);
		if (ok && (job.ticket || job.nextView < kBankFrames * kBankFrames)) return;     // (the next slice next frame)
		for (gfx::Texture t : job.targets) device->Destroy(t);
		job.targets.clear();
		ImpostorBank::Entry entry;
		if (ok)
		{
			const std::string base = BankKey(job.path, job.key);
			const int32 A = (int32)(job.bake.frames * job.bake.size);
			ok = PaintableImage::WritePNG(base + "_albedo.png", A, A, 4, job.bake.albedo.data()) && PaintableImage::WritePNG(base + "_normal.png", A, A, 4, job.bake.normals.data());
			if (ok) { std::ofstream o(base + ".txt"); o << job.bake.radius << " " << job.bake.centre << " " << job.bake.frames << "\n"; }
			ok = ok && MakeEntry(entry, base, job.bake.radius, job.bake.centre, job.bake.frames, job.material);
			if (!ok) err = "could not write or read back its atlases in " + BankCacheDir();
		}
		if (ok) fprintf(stderr, "[renderer] impostor of %s made (%.0f ms of work, %.0f at most in a frame, over %u slices) - cached in %s\n", job.path.c_str(), job.ms, job.worst, (kBankFrames * kBankFrames + kBankViewsAFrame - 1) / kBankViewsAFrame, BankCacheDir().c_str());
		else fprintf(stderr, "[renderer] no impostor for %s: %s\n", job.path.c_str(), err.c_str());
		std::lock_guard<std::mutex> l(bankMutex);
		bankQueue.erase(std::remove(bankQueue.begin(), bankQueue.end(), job.key), bankQueue.end());
		BankItem &item = bankItems[job.key];
		item.state = ok ? 1 : 2;
		if (ok) item.entry = entry;
		job = BankJob();
	}
};
