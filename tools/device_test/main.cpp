// the device layer test scene: N objects, each its own draw with its own data, over M materials (a texture each).
// What it measures: CPU time to record and submit a frame, and the frame rate. Pipelines are made ahead
// and compile on the device's workers: no frame waits for one.
//   device_test [objects=10000] [materials=64] [seconds=8] [vsync=0]
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <SDL.h>
#include <SDL_syswm.h>
#if defined(__APPLE__)
#include <SDL_metal.h>
#endif
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

using namespace p3d::gfx;

static const char* kShader = R"(
struct Draw { mvp : mat4x4<f32>, tint : vec4<f32> };
@group(0) @binding(0) var<uniform> draw : Draw;
@group(1) @binding(0) var tex : texture_2d<f32>;
@group(1) @binding(1) var smp : sampler;
struct VOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32>, @location(1) n : vec3<f32> };
@vertex fn vs_main(@location(0) p : vec3<f32>, @location(1) n : vec3<f32>, @location(2) uv : vec2<f32>) -> VOut {
  var o : VOut; o.pos = draw.mvp * vec4<f32>(p, 1.0); o.uv = uv; o.n = n; return o;
}
@fragment fn fs_main(i : VOut) -> @location(0) vec4<f32> {
  let l = max(dot(normalize(i.n), normalize(vec3<f32>(0.4, 0.8, 0.3))), 0.0) * 0.8 + 0.2;
  return vec4<f32>(textureSample(tex, smp, i.uv).rgb * draw.tint.rgb * l, 1.0);
}
)";

struct M4 { float m[16]; };
static M4 Mul(const M4 &a, const M4 &b) { M4 r; for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) { float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k]; r.m[c * 4 + rr] = s; } return r; }
// (reversed Z, depth 0..1: far is 0)
static M4 Perspective(float fovy, float aspect, float zn) { float f = 1.f / tanf(fovy / 2); M4 r = {}; r.m[0] = f / aspect; r.m[5] = f; r.m[11] = -1.f; r.m[14] = zn; return r; }
static M4 LookAt(float ex, float ey, float ez, float tx, float ty, float tz)
{
	float fx = tx - ex, fy = ty - ey, fz = tz - ez, fl = sqrtf(fx * fx + fy * fy + fz * fz); fx /= fl; fy /= fl; fz /= fl;
	float sx = fy * 0 - fz * 1, sy = fz * 0 - fx * 0, sz = fx * 1 - fy * 0, sl = sqrtf(sx * sx + sy * sy + sz * sz); sx /= sl; sy /= sl; sz /= sl;
	float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;
	M4 r = { { sx, ux, -fx, 0, sy, uy, -fy, 0, sz, uz, -fz, 0, -(sx * ex + sy * ey + sz * ez), -(ux * ex + uy * ey + uz * ez), (fx * ex + fy * ey + fz * ez), 1 } };
	return r;
}

#if defined(_WIN32)
#undef main
#endif
int main(int argc, char** argv)
{
	const int N = argc > 1 ? atoi(argv[1]) : 10000, M = argc > 2 ? atoi(argv[2]) : 64;
	const double seconds = argc > 3 ? atof(argv[3]) : 8.0;
	const bool vsync = argc > 4 && atoi(argv[4]) != 0;
	SDL_Init(SDL_INIT_VIDEO);
	const int ww = getenv("PYROS_W") ? atoi(getenv("PYROS_W")) : 1280, wh = getenv("PYROS_H") ? atoi(getenv("PYROS_H")) : 720;
	SurfaceSource ss; ss.vsync = vsync;
	int pw = ww, ph = wh;
#if defined(__APPLE__)
	SDL_Window* win = SDL_CreateWindow("the device layer test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, SDL_WINDOW_METAL | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_MetalView view = SDL_Metal_CreateView(win);
	SDL_Metal_GetDrawableSize(win, &pw, &ph);
	ss.kind = SurfaceSource::MetalLayer; ss.a = SDL_Metal_GetLayer(view);
#else
	SDL_Window* win = SDL_CreateWindow("the device layer test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_SysWMinfo wm; SDL_VERSION(&wm.version); SDL_GetWindowWMInfo(win, &wm);
	SDL_GetWindowSizeInPixels(win, &pw, &ph);
#if defined(_WIN32)
	ss.kind = SurfaceSource::Win32; ss.a = wm.info.win.window; ss.b = wm.info.win.hinstance;
#else
	ss.kind = SurfaceSource::Xlib; ss.a = wm.info.x11.display; ss.b = (void*)(uintptr_t)wm.info.x11.window;
#endif
#endif
	ss.width = pw; ss.height = ph;
	// (PYROS_BACKEND: webgpu - the default - vulkan, or metal; Vulkan and Metal take the SPIR-V, WebGPU the WGSL)
	const char* backend = getenv("PYROS_BACKEND") ? getenv("PYROS_BACKEND") : "webgpu";
	const bool vulkan = !strcmp(backend, "vulkan"), metal = !strcmp(backend, "metal");
	Device* dev = vulkan ? CreateVulkanDevice(ss) : (metal ? CreateMetalDevice(ss) : CreateWebGPUDevice(ss));
	if (!dev) { fprintf(stderr, "no %s device\n", backend); return 1; }
	printf("device: %s, %dx%d, %d objects, %d materials\n", dev->Caps().name.c_str(), pw, ph, N, M);

	// a cube: 24 vertices (position, normal, uv), 36 indices
	std::vector<float> v; std::vector<uint16_t> idx;
	const float F[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
	for (int f = 0; f < 6; f++)
	{
		float n[3] = { F[f][0], F[f][1], F[f][2] }, u[3], w[3];
		if (fabsf(n[1]) > 0.5f) { u[0] = 1; u[1] = 0; u[2] = 0; } else { u[0] = 0; u[1] = 1; u[2] = 0; }
		w[0] = n[1] * u[2] - n[2] * u[1]; w[1] = n[2] * u[0] - n[0] * u[2]; w[2] = n[0] * u[1] - n[1] * u[0];
		const uint16_t b = (uint16_t)(v.size() / 8);
		const float c[4][2] = { {-1,-1},{1,-1},{1,1},{-1,1} };
		for (int k = 0; k < 4; k++)
		{
			for (int a = 0; a < 3; a++) v.push_back(0.5f * (n[a] + c[k][0] * w[a] + c[k][1] * u[a]));
			for (int a = 0; a < 3; a++) v.push_back(n[a]);
			v.push_back(c[k][0] * 0.5f + 0.5f); v.push_back(c[k][1] * 0.5f + 0.5f);
		}
		uint16_t q[6] = { 0, 2, 1, 0, 3, 2 };      // (counter-clockwise seen from outside)
		for (int k = 0; k < 6; k++) idx.push_back(b + q[k]);
	}
	BufferDesc vb; vb.size = v.size() * 4; vb.usage = BufVertex; Buffer vbuf = dev->CreateBuffer(vb, v.data());
	BufferDesc ib; ib.size = idx.size() * 2; ib.usage = BufIndex; Buffer ibuf = dev->CreateBuffer(ib, idx.data());

	// (WGSL for WebGPU, SPIR-V for Vulkan - the same shader, test.vert / test.frag, built by glslc)
	static const uint32_t vertSpv[] = {
#include "test.vert.inc"
	};
	static const uint32_t fragSpv[] = {
#include "test.frag.inc"
	};
	ShaderDesc vsd, fsd; vsd.wgsl = fsd.wgsl = kShader; vsd.debugName = "test.vert"; fsd.debugName = "test.frag";
	vsd.entry = "vs_main"; fsd.entry = "fs_main";      // (the WGSL's own entry points; the SPIR-V's are "main", which Vulkan and Metal take)
	vsd.stage = Stage::Vertex; vsd.spirv = vertSpv; vsd.spirvWords = sizeof(vertSpv) / 4;
	fsd.stage = Stage::Fragment; fsd.spirv = fragSpv; fsd.spirvWords = sizeof(fragSpv) / 4;
	Shader vsh = dev->CreateShader(vsd), fsh = (vulkan || metal) ? dev->CreateShader(fsd) : vsh;
	GraphicsPipelineDesc pd;
	pd.vertex = vsh; pd.fragment = fsh;
	pd.material = { { 0, BindingKind::Texture2D, false, true }, { 1, BindingKind::Sampler, false, true } };
	pd.drawDataBytes = 80;
	pd.bindings = { { 0, 32, false } };
	pd.attributes = { { 0, 0, Format::RGB32F, 0 }, { 1, 0, Format::RGB32F, 12 }, { 2, 0, Format::RG32F, 24 } };
	pd.depthFormat = Format::D32F; pd.depthCompare = Compare::Greater;
	pd.colorCount = 1; pd.colorFormats[0] = Format::Unknown;     // (the backbuffer's)
	auto t0 = std::chrono::steady_clock::now();
	Pipeline pipe = dev->CreatePipeline(pd);
	printf("CreatePipeline returned in %.3f ms (compiling on the device's workers)\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());

	SamplerDesc smd; Sampler smp = dev->CreateSampler(smd);
	std::vector<BindGroup> mats(M);
	for (int i = 0; i < M; i++)
	{
		std::vector<uint8_t> px(64 * 64 * 4);
		for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
		{
			const bool on = ((x / 8) + (y / 8) + i) & 1;
			uint8_t* p = &px[(y * 64 + x) * 4];
			p[0] = (uint8_t)(on ? 200 : 60 + (i * 37) % 120); p[1] = (uint8_t)(on ? 200 : 60 + (i * 71) % 120); p[2] = (uint8_t)(on ? 200 : 60 + (i * 13) % 120); p[3] = 255;
		}
		TextureDesc td; td.width = td.height = 64; td.format = Format::RGBA8_sRGB;
		TextureData data; data.bytes = px.data(); data.size = px.size();
		Texture t = dev->CreateTexture(td, &data, 1);
		while (!dev->PipelineReady(pipe)) { dev->BeginFrame(); }
		Device::BindGroupEntry e[2]; e[0].binding = 0; e[0].texture = t; e[1].binding = 1; e[1].sampler = smp;
		mats[i] = dev->CreateBindGroup(pipe, e, 2);
	}
	TextureDesc dd; dd.width = pw; dd.height = ph; dd.format = Format::D32F; dd.usage = TexDepthTarget; Texture depth = dev->CreateTexture(dd);

	const int side = (int)ceilf(sqrtf((float)N));
	double recordMs = 0, frameMsSum = 0, gpuMsSum = 0; int frames = 0, measured = 0, gpuMeasured = 0;
	// PYROS_PROFILE_LOG=<file>: a line a frame, as the engine's PYROS_PROFILE_LOG writes them (t= ms= Scope=...)
	FILE* prof = getenv("PYROS_PROFILE_LOG") ? fopen(getenv("PYROS_PROFILE_LOG"), "w") : nullptr;
	auto start = std::chrono::steady_clock::now(), last = start;
	bool quit = false;
	while (!quit)
	{
		SDL_Event e; while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) quit = true;
		const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		if (t > seconds) break;
		auto b0 = std::chrono::steady_clock::now();
		dev->BeginFrame();
		auto a0 = std::chrono::steady_clock::now();
		Texture back = dev->AcquireBackbuffer();
		if (!back) continue;
		auto r0 = std::chrono::steady_clock::now();
		// (the draws shared between PYROS_THREADS threads, each recording its own list: the first clears, the
		// rest draw over it; submitted in order)
		const int T = getenv("PYROS_THREADS") ? std::max(1, atoi(getenv("PYROS_THREADS"))) : 1;
		const M4 proj = Perspective(70.f * 3.14159265f / 180.f, (float)pw / ph, 0.1f);
		const M4 viewm = LookAt(0.f, 2.f, 10.f, 0.f, 2.f, -1.f);
		const M4 vp = Mul(proj, viewm);
		std::vector<CommandList*> lists(T);
		for (int i = 0; i < T; i++) lists[i] = dev->NewCommandList("scene");
		auto record = [&](int ti) {
			CommandList* cl = lists[ti];
			PassDesc pass; pass.colorCount = 1; pass.colors[0].texture = back; pass.colors[0].clear[0] = 0.45f; pass.colors[0].clear[1] = 0.6f; pass.colors[0].clear[2] = 0.8f;
			pass.depth.texture = depth; pass.depth.clear = 0.f; pass.width = pw; pass.height = ph; pass.debugName = ti == 0 ? "Scene" : "Scene+";
			if (ti > 0) { pass.colors[0].load = Load::Keep; pass.depth.load = Load::Keep; }
			cl->BeginPass(pass);
			cl->SetPipeline(pipe);
			cl->SetVertexBuffer(0, vbuf, 0); cl->SetIndexBuffer(ibuf, 0, false);
			const int k0 = (int)((long long)N * ti / T), k1 = (int)((long long)N * (ti + 1) / T);
			int lastMat = -1;
			for (int k = k0; k < k1; k++)
			{
				const int mat = (int)((long long)k * M / N);
				if (mat != lastMat) { cl->SetBindGroup(mats[mat]); lastMat = mat; }
				const int x = k % side, z = k / side;
				M4 model = { { 0.8f,0,0,0, 0,0.8f,0,0, 0,0,0.8f,0, (float)(x - side / 2), 0.f, (float)(z - side / 2), 1 } };
				struct { M4 mvp; float tint[4]; } d = { Mul(vp, model), { 1, 1, 1, 1 } };
				cl->DrawData(&d, sizeof(d));
				cl->DrawIndexed(36, 1, 0, 0, 0);
			}
			cl->EndPass();
		};
		if (T == 1) record(0);
		else { std::vector<std::thread> th; for (int i = 0; i < T; i++) th.emplace_back(record, i); for (auto &t : th) t.join(); }
		auto s0 = std::chrono::steady_clock::now();
		dev->Submit(lists.data(), (uint32_t)lists.size());
		auto p0 = std::chrono::steady_clock::now();
		const double rec = std::chrono::duration<double, std::milli>(p0 - r0).count();
		dev->Present();
		auto e0 = std::chrono::steady_clock::now();
		auto msOf = [](std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
		const double tBegin = msOf(b0, a0), tAcquire = msOf(a0, r0), tRecord = msOf(r0, s0), tSubmit = msOf(s0, p0), tPresent = msOf(p0, e0);
		dev->EndFrame();
		auto now = std::chrono::steady_clock::now();
		const double fms = std::chrono::duration<double, std::milli>(now - last).count(); last = now;
		frames++;
		const FrameStats &st = dev->LastFrameStats();
		if (t > 1.0) { recordMs += rec; frameMsSum += fms; measured++; if (st.gpuMs > 0.0) { gpuMsSum += st.gpuMs; gpuMeasured++; } }
		if (prof)
		{
			fprintf(prof, "frame=%d t=%.2f ms=%.3f CPU.BeginFrame=%.3f CPU.Acquire=%.3f CPU.Record=%.3f CPU.Submit=%.3f CPU.Present=%.3f GPU.Total=%.3f", frames, t, fms, tBegin, tAcquire, tRecord, tSubmit, tPresent, st.gpuMs);
			for (const FrameStats::Pass &p : st.passes) fprintf(prof, " GPU.%s=%.3f", p.name.c_str(), p.gpuMs);
			fprintf(prof, " #Draws=%u #Skipped=%u #PipelineChanges=%u #BindGroupChanges=%u #DrawDataKB=%.1f #PipelinesPending=%u #PipelinesFinished=%u\n",
				st.draws, st.skippedDraws, st.pipelineChanges, st.bindGroupChanges, st.drawDataBytes / 1024.0, st.pipelinesPending, st.pipelinesFinished);
		}
	}
	// the whole process's CPU, every thread (the driver's and Dawn's too), a frame
#if defined(_WIN32)
	FILETIME c0, e0, k0, u0; GetProcessTimes(GetCurrentProcess(), &c0, &e0, &k0, &u0);
	const double cpu = ((((uint64_t)k0.dwHighDateTime << 32) | k0.dwLowDateTime) + (((uint64_t)u0.dwHighDateTime << 32) | u0.dwLowDateTime)) * 1e-7;
#else
	rusage ru; getrusage(RUSAGE_SELF, &ru);
	const double cpu = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec * 1e-6 + ru.ru_stime.tv_sec + ru.ru_stime.tv_usec * 1e-6;
#endif
	printf("PROCESS cpu %.2f s over %d frames: %.2f ms a frame (all threads, loading included)\n", cpu, frames, cpu * 1000.0 / frames);
	if (measured)
		printf("RESULT objects=%d materials=%d frames=%d  record+submit %.3f ms/frame (%.2f us/draw)  GPU %.2f ms  frame %.2f ms (%.0f fps)\n",
			N, M, measured, recordMs / measured, recordMs / measured * 1000.0 / N, gpuMeasured ? gpuMsSum / gpuMeasured : 0.0, frameMsSum / measured, 1000.0 * measured / frameMsSum);
	if (prof) fclose(prof);
	delete dev;
#if defined(__APPLE__)
	SDL_Metal_DestroyView(view);
#endif
	SDL_DestroyWindow(win); SDL_Quit();
	return 0;
}
