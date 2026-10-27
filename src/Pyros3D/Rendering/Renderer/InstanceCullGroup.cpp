//============================================================================
// Name        : InstanceCullGroup.cpp
// Author      : Duarte Peixinho
// Description : See InstanceCullGroup.h.
//============================================================================

#include <Pyros3D/Rendering/Renderer/InstanceCullGroup.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace p3d {

	namespace {

		// Views a frame the groups have room for (the camera's, and one more - a scope, a reflection),
		// times two frames in flight: what the drawers' buffers are regions of. A view past them draws
		// the blocks, as before.
		const uint32 kViews = 2;
		const uint32 kRegions = kViews * 2;
		// One dispatch's parameters: 16 vec4s
		const uint32 kParamBytes = 256;
		// One view's draw arguments: two draws of five uint32s, in 64 bytes
		const uint32 kArgsBytes = 64;

		const char* kCullShader =
			"layout(local_size_x = 64) in;\n"
			// (this dispatch's own 256 bytes of a buffer of them: InstanceCullGroup::Cull)
			"layout(std430, binding = 0) buffer CullParams { vec4 p[]; };\n"
			"layout(std430, binding = 1) buffer SourceM { vec4 srcM[]; };\n"
			"layout(std430, binding = 2) buffer SourceC { vec4 srcC[]; };\n"
			"layout(std430, binding = 3) buffer NearM { vec4 nearM[]; };\n"
			"layout(std430, binding = 4) buffer NearC { vec4 nearC[]; };\n"
			"layout(std430, binding = 5) buffer FarM { vec4 farM[]; };\n"
			"layout(std430, binding = 6) buffer FarC { vec4 farC[]; };\n"
			"layout(std430, binding = 7) buffer DrawArgs { uint args[]; };\n"
			// (counted by the work group first, and added to the draw's count once a group: every instance
			// adding itself to one count, they queued for it - 5.6 ms of the GPU's for 67 000 of them)
			"shared uint groupCount[2];\n"
			"shared uint groupBase[2];\n"
			"void main() {\n"
			"    uint i = gl_GlobalInvocationID.x;\n"
			"    uint li = gl_LocalInvocationIndex;\n"
			"    if (li == 0u) { groupCount[0] = 0u; groupCount[1] = 0u; }\n"
			"    barrier();\n"
			"    uvec4 u = floatBitsToUint(p[12]);\n"      // kept, near count at, far count at, flags (1 tinted, 2 has far)
			"    uvec4 v = floatBitsToUint(p[13]);\n"      // near base, far base, capacity, the block's whole count
			"    int which = -1;\n"
			"    uint localN = 0u, localF = 0u;\n"
			"    float fade = 0.0, thin = 1.0;\n"
			"    vec2 grow = vec2(1.0);\n"
			"    mat4 m = mat4(1.0);\n"
			"    if (i < u.x) {\n"
			"        mat4 block = mat4(p[0], p[1], p[2], p[3]);\n"
			"        m = block * mat4(srcM[i * 4u], srcM[i * 4u + 1u], srcM[i * 4u + 2u], srcM[i * 4u + 3u]);\n"
			"        vec3 c = m[3].xyz;\n"
			"        float s = max(length(m[0].xyz), max(length(m[1].xyz), length(m[2].xyz)));\n"
			"        grow = p[11].xy;\n"
			"        bool inside = true;\n"
			// (thinned at its own distance, flags 4: p[14] thin from, to, least, amount; p[15] full, fade -
			// metres; its rank among its block's is its place in it, the plants being made in no order)
			"        if ((u.w & 4u) != 0u) {\n"
			"            float pd = length(c - p[10].xyz);\n"
			"            float tk = p[14].w;\n"
			"            if (pd > p[14].x) tk *= (pd >= p[14].y || p[14].y <= p[14].x) ? p[14].z : 1.0 + (p[14].z - 1.0) * smoothstep(p[14].x, p[14].y, pd);\n"
			"            float dk = pd <= p[15].x ? 1.0 : (pd >= p[15].y ? 0.0 : 1.0 - pow((pd - p[15].x) / max(0.001, p[15].y - p[15].x), 2.0));\n"
			"            float keep = tk * dk;\n"
			"            float rank = (float(i) + 0.5) / float(max(v.w, 1u));\n"
			"            if (rank >= keep) inside = false;\n"
			"            else {\n"
			"                float q = (keep - rank) / max(0.0001, keep * 0.12);\n"
			"                if (q < 1.0) thin = max(q, 0.002);\n"
			"                float area = 1.0 / max(tk, 0.05);\n"
			"                grow = vec2(pow(area, 0.75), pow(area, 0.25));\n"
			"            }\n"
			"        }\n"
			"        float r = p[11].z * s * max(grow.x, grow.y);\n"
			"        if (inside) for (int k = 0; k < 6; k++) if (dot(p[4 + k].xyz, c) + p[4 + k].w < -r) inside = false;\n"
			"        if (inside) {\n"
			"            vec3 d = c - p[10].xyz;\n"
			"            float dist = length(d);\n"
			// (p[11].w: half the band round the far distance over which a plant's two levels crossfade -
			// in it the plant is in both lists, each with how far it has come, and the shader dithers them)
			"            if ((u.w & 2u) == 0u) which = 0;\n"
			"            else if (dist < p[10].w - p[11].w) which = 0;\n"
			"            else if (dist >= p[10].w + p[11].w) which = 1;\n"
			"            else { which = 2; fade = clamp((dist - (p[10].w - p[11].w)) / (2.0 * p[11].w), 0.002, 0.998); }\n"
			// (a block thinned - fewer of its plants kept with distance - has its last ones kept fade
			// out by the same dither as the count comes down over them, not go from one frame to the next:
			// v.w its whole count)

			"            if (which != 1) localN = atomicAdd(groupCount[0], 1u);\n"
			"            if (which != 0) localF = atomicAdd(groupCount[1], 1u);\n"
			"        }\n"
			"    }\n"
			"    barrier();\n"
			"    if (li == 0u) {\n"
			"        groupBase[0] = groupCount[0] > 0u ? atomicAdd(args[u.y], groupCount[0]) : 0u;\n"
			"        groupBase[1] = groupCount[1] > 0u ? atomicAdd(args[u.z], groupCount[1]) : 0u;\n"
			"    }\n"
			"    barrier();\n"
			"    if (which < 0) return;\n"
			// (its growth, carried in the matrix's last row - see PyrosShader.glsl - and in a crossfade
			// its share: the near level draws where the dither is under 1 - fade, the far where it is not)
			"    m[0][3] = grow.x; m[1][3] = grow.y;\n"
			"    if (which != 1) {\n"
			"        uint at = groupBase[0] + localN;\n"
			"        if (at < v.z) {\n"
			"            uint o = v.x + at;\n"
			"            m[2][3] = which == 2 ? 1.0 - fade : (thin < 1.0 ? thin : 0.0);\n"
			"            nearM[o * 4u] = m[0]; nearM[o * 4u + 1u] = m[1]; nearM[o * 4u + 2u] = m[2]; nearM[o * 4u + 3u] = m[3]; if ((u.w & 1u) != 0u) nearC[o] = srcC[i];\n"
			"        }\n"
			"    }\n"
			"    if (which != 0) {\n"
			"        uint at = groupBase[1] + localF;\n"
			"        if (at < v.z) {\n"
			"            uint o = v.y + at;\n"
			// (the far level comes in quicker than the near goes - whole a third of the way in - on the pixels
			// the near is leaving, and every one the near has left besides: the two shapes are not the same,
			// and where only one of them is, an even split left the ground showing through)
			"            float farShare = min(1.0, fade / 0.3);\n"
			"            m[2][3] = which == 2 ? (farShare >= 0.998 ? 0.0 : -farShare) : (thin < 1.0 ? thin : 0.0);\n"
			"            farM[o * 4u] = m[0]; farM[o * 4u + 1u] = m[1]; farM[o * 4u + 2u] = m[2]; farM[o * 4u + 3u] = m[3]; if ((u.w & 1u) != 0u) farC[o] = srcC[i];\n"
			"        }\n"
			"    }\n"
			"}\n";

		bool g_enabled = !(std::getenv("PYROS_GPU_CULL") && std::getenv("PYROS_GPU_CULL")[0] == '0');
		// Half the band a plant's near and far levels crossfade over, as a share of the far distance:
		// a tree that changed level from one frame to the next was seen to. 0: the change as it was.
		f32 g_crossfade = []() { const char* v = std::getenv("PYROS_FOLIAGE_CROSSFADE"); return v ? (f32)atof(v) : 0.15f; }();
		// (GL 4.3 has all it needs and is written for, but it has not been run there yet: on GL only when
		// asked - PYROS_GPU_CULL=1)
		const bool g_onGLAsked = std::getenv("PYROS_GPU_CULL") && std::getenv("PYROS_GPU_CULL")[0] == '1';

		// The kernel and a buffer of nothing for a binding with nothing to give, made once a device
		struct Kernel { IRenderDevice* device = NULL; DeviceHandle stage = 0, program = 0, pipeline = 0, nothing = 0; bool failed = false; };
		Kernel g_kernel;

		bool EnsureKernel(IRenderDevice &dev)
		{
			if (g_kernel.device == &dev) return g_kernel.pipeline != 0;
			g_kernel = Kernel();
			g_kernel.device = &dev;
			g_kernel.stage = dev.CreateShaderStage(ShaderType::ComputeShader);
			std::string errorLog;
			if (!dev.CompileShaderStage(g_kernel.stage, dev.BuildShaderSource(std::string(), std::string(kCullShader)), errorLog))
			{
				echo("InstanceCullGroup: no GPU culling - the kernel did not compile: " + errorLog);
				return false;
			}
			g_kernel.program = dev.CreateProgram();
			dev.AttachShaderStage(g_kernel.program, g_kernel.stage);
			if (!dev.LinkProgram(g_kernel.program, errorLog))
			{
				echo("InstanceCullGroup: no GPU culling - the kernel did not link: " + errorLog);
				return false;
			}
			g_kernel.pipeline = dev.CreateComputePipeline(g_kernel.program);
			const Vec4 zero[4];
			g_kernel.nothing = dev.CreateStorageBuffer((uint32)sizeof(zero), 0, zero);
			return g_kernel.pipeline != 0 && g_kernel.nothing != 0;
		}

		// The frame's views so far: a new frame (frameKey changed) turns to the other frame in flight
		f64 g_frameKey = -1.0;
		uint32 g_parity = 0, g_views = 0;

		// A plane from rows of a matrix (column-major: element (r, c) at m[c * 4 + r]), normalised
		Vec4 Plane(const Matrix &m, const int row, const f32 sign)
		{
			Vec4 p(m.m[3] + sign * m.m[row], m.m[7] + sign * m.m[4 + row], m.m[11] + sign * m.m[8 + row], m.m[15] + sign * m.m[12 + row]);
			const f32 l = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
			if (l > 1e-12f) { p.x /= l; p.y /= l; p.z /= l; p.w /= l; }
			return p;
		}

		inline f32 FromBits(const uint32 u) { f32 f; memcpy(&f, &u, 4); return f; }
	}

	bool InstanceCullGroup::Available()
	{
		if (!g_enabled || !IsActiveRenderDeviceSet()) return false;
		IRenderDevice &dev = GetActiveRenderDevice();
		const bool gl = !dev.IsVulkan() && !dev.RenderTargetOriginIsTopLeft();
		return dev.SupportsCompute() && dev.SupportsDrawIndirect() && (!gl || g_onGLAsked);
	}
	bool InstanceCullGroup::DrawerSphere(RenderingComponent* rc, Vec3 &centre, f32 &radius)
	{
		if (rc == NULL || !rc->IsInstanced()) return false;
		IRenderingInstancedComponent* irc = static_cast<IRenderingInstancedComponent*>(rc);
		InstanceCullGroup* g = irc->cullGroup;
		if (g == NULL || irc->indirectArgs == 0 || (irc != g->nearDrawer.get() && irc != g->farDrawer.get())) return false;
		centre = g->viewCentre;
		radius = g->viewRadius;
		return true;
	}

	void InstanceCullGroup::SetEnabled(const bool on) { g_enabled = on; }
	bool InstanceCullGroup::GetEnabled() { return g_enabled; }

	InstanceCullGroup::InstanceCullGroup(GameObject* owner, const std::shared_ptr<Renderable> &mesh, const std::shared_ptr<IMaterial> &material,
		const std::shared_ptr<Renderable> &farMesh, const f32 farFrom, const bool tinted, const f32 meshRadius)
		: owner(owner), farFrom(farFrom), meshRadius(meshRadius), tinted(tinted)
	{
		// The drawers: on a child of the owner where it is (their instances are in its space), with
		// no instances of their own - so no list the renderer keeps has them, and only a view that
		// culls on the GPU draws them (CullView puts them in its list).
		drawerObject = std::make_shared<GameObject>();
		drawerObject->SetName("gpu_cull_drawers");
		drawerObject->SetTransient(true);
		std::vector<Matrix> none(1);
		std::vector<Vec4> noTints;
		if (tinted) noTints.resize(1, Vec4(1.f, 1.f, 1.f, 1.f));
		nearDrawer = std::make_shared<RenderingInstancedComponent>(mesh, material, std::vector<Matrix>(none), std::vector<Vec4>(noTints), 1.f);
		nearDrawer->SetNumberInstances(0);
		nearDrawer->DisableCastShadows();
		nearDrawer->cullGroup = this;
		drawerObject->AddComponent(nearDrawer);
		if (farMesh)
		{
			farDrawer = std::make_shared<RenderingInstancedComponent>(farMesh, material, std::vector<Matrix>(none), std::vector<Vec4>(noTints), 1.f);
			farDrawer->SetNumberInstances(0);
			farDrawer->DisableCastShadows();
			farDrawer->cullGroup = this;
			drawerObject->AddComponent(farDrawer);
		}
		if (owner) owner->Add(drawerObject);
	}

	InstanceCullGroup::~InstanceCullGroup()
	{
		for (size_t i = 0; i < blocks.size(); i++) if (blocks[i] && blocks[i]->cullGroup == this) blocks[i]->cullGroup = NULL;
		if (nearDrawer) nearDrawer->cullGroup = NULL;
		if (farDrawer) farDrawer->cullGroup = NULL;
		if (IsActiveRenderDeviceSet() && g_kernel.device == &GetActiveRenderDevice())
		{
			IRenderDevice &dev = GetActiveRenderDevice();
			for (uint32 k = 0; k < kMaxRegions; k++)
			{
				if (params[k] != 0) dev.DestroyStorageBuffer(params[k]);
				if (args[k] != 0) dev.DestroyStorageBuffer(args[k]);
			}
		}
	}

	void InstanceCullGroup::Detach()
	{
		if (owner && drawerObject) owner->Remove(drawerObject);
		owner = NULL;
	}

	void InstanceCullGroup::Add(RenderingInstancedComponent* block)
	{
		if (!block) return;
		block->cullGroup = this;
		blocks.push_back(block);
	}

	void InstanceCullGroup::EnsureCapacity(const uint32 instances)
	{
		if (instances <= capacity) return;
		// (a quarter more than asked for, so a view turning about does not make it again every frame)
		uint32 want = std::max<uint32>(4096, instances + instances / 4);
		want = (want + 1023u) & ~1023u;
		capacity = want;
		nearDrawer->ResizeForGpu(capacity * kRegions, tinted);
		if (farDrawer) farDrawer->ResizeForGpu(capacity * kRegions, tinted);
	}

	bool InstanceCullGroup::Cull(const uint32 region, std::vector<RenderingInstancedComponent*> &visibleBlocks, const Vec4 worldPlanes[6], const Vec3 &eyeWorld)
	{
		IRenderDevice &dev = GetActiveRenderDevice();
		if (!owner || !nearDrawer || nearDrawer->GetMeshes().size() != 1 || (farDrawer && farDrawer->GetMeshes().size() != 1)) return false;

		uint32 total = 0;
		for (size_t b = 0; b < visibleBlocks.size(); b++) total += visibleBlocks[b]->InstancesToDraw();
		// (the blocks' bounds, together: DrawerSphere)
		{
			Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
			for (size_t b = 0; b < visibleBlocks.size(); b++)
			{
				const Vec3 c = visibleBlocks[b]->GetOwner()->GetDrawWorld() * visibleBlocks[b]->GetBoundingSphereCenter();
				const f32 r = visibleBlocks[b]->GetBoundingSphereRadius();
				lo = Vec3(std::min(lo.x, c.x - r), std::min(lo.y, c.y - r), std::min(lo.z, c.z - r));
				hi = Vec3(std::max(hi.x, c.x + r), std::max(hi.y, c.y + r), std::max(hi.z, c.z + r));
			}
			viewCentre = (lo + hi) * 0.5f;
			viewRadius = visibleBlocks.empty() ? 0.f : (hi - lo).magnitude() * 0.5f;
		}
		EnsureCapacity(std::max<uint32>(total, 1));

		// This view's buffers: room for every block of the group
		const uint32 wantParams = (uint32)std::max<size_t>(blocks.size(), 1) * kParamBytes;
		if (paramsBytes[region] < wantParams)
		{
			if (params[region] != 0) dev.DestroyStorageBuffer(params[region]);
			params[region] = dev.CreateStorageBuffer(wantParams, 0, NULL);
			paramsBytes[region] = params[region] != 0 ? wantParams : 0;
		}
		if (args[region] == 0) args[region] = dev.CreateStorageBuffer(kArgsBytes, 7, NULL);
		if (params[region] == 0 || args[region] == 0) return false;

		// In the owner's space: the planes, the eye, each block's place
		const Matrix &ownerWorld = owner->GetDrawWorld();
		const Matrix ownerInverse = ownerWorld.Inverse();
		Vec4 planes[6];
		for (int k = 0; k < 6; k++)
		{
			const Vec4 &w = worldPlanes[k];
			Vec4 l;
			l.x = w.x * ownerWorld.m[0] + w.y * ownerWorld.m[1] + w.z * ownerWorld.m[2] + w.w * ownerWorld.m[3];
			l.y = w.x * ownerWorld.m[4] + w.y * ownerWorld.m[5] + w.z * ownerWorld.m[6] + w.w * ownerWorld.m[7];
			l.z = w.x * ownerWorld.m[8] + w.y * ownerWorld.m[9] + w.z * ownerWorld.m[10] + w.w * ownerWorld.m[11];
			l.w = w.x * ownerWorld.m[12] + w.y * ownerWorld.m[13] + w.z * ownerWorld.m[14] + w.w * ownerWorld.m[15];
			planes[k] = l;
		}
		const Vec3 eye = ownerInverse * eyeWorld;

		const uint32 base = region * capacity;
		const uint32 nearCountAt = 1, farCountAt = 6;
		// The arguments: counts at none, the cull adds them up
		uint32 a[kArgsBytes / 4];
		memset(a, 0, sizeof(a));
		a[0] = (uint32)nearDrawer->GetMeshes()[0]->Geometry->GetIndexData().size();
		a[4] = base;
		if (farDrawer) { a[5] = (uint32)farDrawer->GetMeshes()[0]->Geometry->GetIndexData().size(); a[9] = base; }
		dev.UpdateStorageBuffer(args[region], 0, kArgsBytes, a);

		static thread_local std::vector<Vec4> p;
		p.assign(visibleBlocks.size() * (kParamBytes / 16), Vec4());
		// (the far distance itself now, not its square: the kernel measures each plant's for the crossfade)
		const f32 farAt = farDrawer ? farFrom : 3.0e38f;
		const f32 band = farDrawer ? std::max(0.f, farFrom * g_crossfade) : 0.f;
		for (size_t b = 0; b < visibleBlocks.size(); b++)
		{
			RenderingInstancedComponent* blk = visibleBlocks[b];
			Vec4* q = &p[b * (kParamBytes / 16)];
			const Matrix rel = ownerInverse * blk->GetOwner()->GetDrawWorld();
			for (int c = 0; c < 4; c++) q[c] = Vec4(rel.m[c * 4], rel.m[c * 4 + 1], rel.m[c * 4 + 2], rel.m[c * 4 + 3]);
			for (int k = 0; k < 6; k++) q[4 + k] = planes[k];
			q[10] = Vec4(eye.x, eye.y, eye.z, farAt);
			const Vec2 &growth = blk->GrowthToDraw();
			q[11] = Vec4(growth.x, growth.y, meshRadius, band);
			const uint32 flags = (tinted && blk->color_buffer != NULL ? 1u : 0u) | (farDrawer ? 2u : 0u) | (thinning ? 4u : 0u);
			if (thinning)
			{
				const f32 reach = FoliageComponent::GetReach();
				const bool thins = FoliageComponent::GetThinning() && thin[2] < 1.f;
				q[14] = Vec4(thin[0] * reach, thins ? thin[1] * reach : 0.f, thins ? std::max(0.05f, thin[2]) : 1.f, FoliageComponent::GetAmount());
				q[15] = Vec4(thin[3] * reach, thin[4] * reach, 0.f, 0.f);
			}
			q[12] = Vec4(FromBits(blk->InstancesToDraw()), FromBits(nearCountAt), FromBits(farCountAt), FromBits(flags));
			q[13] = Vec4(FromBits(base), FromBits(base), FromBits(capacity), FromBits(blk->MostInstances()));
		}
		if (!p.empty()) dev.UpdateStorageBuffer(params[region], 0, (uint32)(p.size() * sizeof(Vec4)), &p[0]);

		const DeviceHandle nearM = (DeviceHandle)nearDrawer->transform_buffer->Buffer->ID;
		const DeviceHandle nearC = nearDrawer->color_buffer ? (DeviceHandle)nearDrawer->color_buffer->Buffer->ID : g_kernel.nothing;
		const DeviceHandle farM = farDrawer ? (DeviceHandle)farDrawer->transform_buffer->Buffer->ID : g_kernel.nothing;
		const DeviceHandle farC = farDrawer && farDrawer->color_buffer ? (DeviceHandle)farDrawer->color_buffer->Buffer->ID : g_kernel.nothing;
		for (size_t b = 0; b < visibleBlocks.size(); b++)
		{
			RenderingInstancedComponent* blk = visibleBlocks[b];
			const uint32 kept = blk->InstancesToDraw();
			if (kept == 0) continue;
			dev.BindComputePipeline(0, g_kernel.pipeline);
			dev.BindStorageBufferRange(0, params[region], 0, (uint32)b * kParamBytes, kParamBytes);
			dev.BindStorageBuffer(0, (DeviceHandle)blk->transform_buffer->Buffer->ID, 1);
			dev.BindStorageBuffer(0, blk->color_buffer ? (DeviceHandle)blk->color_buffer->Buffer->ID : g_kernel.nothing, 2);
			dev.BindStorageBuffer(0, nearM, 3);
			dev.BindStorageBuffer(0, nearC, 4);
			dev.BindStorageBuffer(0, farM, 5);
			dev.BindStorageBuffer(0, farC, 6);
			dev.BindStorageBuffer(0, args[region], 7);
			dev.Dispatch(0, (kept + 63u) / 64u, 1, 1);
		}
		nearDrawer->indirectArgs = args[region];
		nearDrawer->indirectOffset = 0;
		if (farDrawer)
		{
			farDrawer->indirectArgs = args[region];
			farDrawer->indirectOffset = 20;
		}
		return true;
	}

	void InstanceCullGroup::CullView(std::vector<RenderingMesh*> &visible, GameObject* camera, const Matrix &projection, const f64 frameKey)
	{
		if (camera == NULL || visible.empty() || !Available()) return;
		// PYROS_GPU_CULL_TOGGLE=<seconds>: on and off in turn, that long each - to compare the two in one
		// run (from one run to the next, this machine's GPU is not the same GPU: GpuCull.On says which)
		static const f64 toggle = std::getenv("PYROS_GPU_CULL_TOGGLE") ? std::atof(std::getenv("PYROS_GPU_CULL_TOGGLE")) : 0.0;
		if (toggle > 0.0)
		{
			const bool on = ((long long)(frameKey / toggle) % 2) == 0;
			FrameProfiler::Instance().Counter("GpuCull.On", on ? 1.0 : 0.0);
			if (!on) return;
		}
		// The groups in the view, and their blocks in it
		static thread_local std::unordered_map<InstanceCullGroup*, std::vector<RenderingInstancedComponent*> > found;
		for (std::unordered_map<InstanceCullGroup*, std::vector<RenderingInstancedComponent*> >::iterator it = found.begin(); it != found.end(); ++it) it->second.clear();
		bool any = false;
		// (a block once, however many of its meshes the view lists - both its levels while it fades from
		// one to the other: counted twice it was culled twice, and its group's parameters overran)
		static thread_local std::unordered_map<const RenderingInstancedComponent*, bool> seen;
		seen.clear();
		for (size_t i = 0; i < visible.size(); i++)
		{
			RenderingComponent* rc = visible[i]->renderingComponent;
			if (rc == NULL || !rc->IsInstanced()) continue;
			IRenderingInstancedComponent* irc = static_cast<IRenderingInstancedComponent*>(rc);
			// (a block of a group - not one of its drawers)
			if (irc->cullGroup == NULL || irc == irc->cullGroup->nearDrawer.get() || irc == irc->cullGroup->farDrawer.get()) continue;
			std::vector<RenderingInstancedComponent*> &list = found[irc->cullGroup];
			RenderingInstancedComponent* blk = static_cast<RenderingInstancedComponent*>(irc);
			if (seen.insert(std::make_pair(blk, true)).second) list.push_back(blk);
			any = true;
		}
		if (!any) return;

		IRenderDevice &dev = GetActiveRenderDevice();
		if (!EnsureKernel(dev)) return;
		if (frameKey != g_frameKey) { g_frameKey = frameKey; g_parity ^= 1u; g_views = 0; }
		if (g_views >= kViews) return;
		const uint32 region = g_parity * kViews + g_views++;

		const Matrix viewProjection = projection * camera->GetDrawWorld().Inverse();
		Vec4 planes[6] = { Plane(viewProjection, 0, 1.f), Plane(viewProjection, 0, -1.f), Plane(viewProjection, 1, 1.f),
			Plane(viewProjection, 1, -1.f), Plane(viewProjection, 2, 1.f), Plane(viewProjection, 2, -1.f) };
		const Vec3 eye = camera->GetDrawWorldPosition();

		static thread_local std::unordered_map<const RenderingComponent*, bool> taken;
		taken.clear();
		static thread_local std::vector<RenderingMesh*> drawers;
		drawers.clear();
		uint32 candidates = 0, groupsCulled = 0;
		for (std::unordered_map<InstanceCullGroup*, std::vector<RenderingInstancedComponent*> >::iterator it = found.begin(); it != found.end(); ++it)
		{
			if (it->second.empty()) continue;
			for (size_t b = 0; b < it->second.size(); b++) candidates += it->second[b]->InstancesToDraw();
			if (!it->first->Cull(region, it->second, planes, eye)) continue;
			groupsCulled++;
			for (size_t b = 0; b < it->second.size(); b++) taken[it->second[b]] = true;
			drawers.push_back(it->first->nearDrawer->GetMeshes()[0]);
			if (it->first->farDrawer) drawers.push_back(it->first->farDrawer->GetMeshes()[0]);
		}
		if (groupsCulled == 0) return;
		dev.ComputeBarrier(0, ComputeBarrierBit::VertexBuffer | ComputeBarrierBit::DrawIndirect);
		// PYROS_GPU_CULL_TRACE=<seconds>: what the cull counted that second of the scene's time, read back
		// to stderr, group by group (a wait for the GPU - for finding out, not for playing)
		static const f64 traceAt = std::getenv("PYROS_GPU_CULL_TRACE") ? std::atof(std::getenv("PYROS_GPU_CULL_TRACE")) : -1.0;
		if (traceAt >= 0.0 && frameKey >= traceAt && frameKey < traceAt + 1.0)
		{
			for (std::unordered_map<InstanceCullGroup*, std::vector<RenderingInstancedComponent*> >::iterator it = found.begin(); it != found.end(); ++it)
			{
				if (it->second.empty()) continue;
				InstanceCullGroup* g = it->first;
				uint32 a[kArgsBytes / 4];
				dev.ReadStorageBuffer(g->nearDrawer->indirectArgs, 0, kArgsBytes, a);
				uint32 kept = 0;
				for (size_t b = 0; b < it->second.size(); b++) kept += it->second[b]->InstancesToDraw();
				fprintf(stderr, "[gpucull] region %u: %zu blocks, %u kept -> near %u of %u indices (base %u), far %u of %u (base %u); capacity %u\n",
					region, it->second.size(), kept, a[1], a[0], a[4], a[6], a[5], a[9], g->capacity);
				const Vec3 dw = g->drawerObject->GetDrawWorldPosition(), ow = g->owner ? g->owner->GetDrawWorldPosition() : Vec3();
				fprintf(stderr, "[gpucull]   drawers at (%.1f %.1f %.1f), owner at (%.1f %.1f %.1f), drawer in scene %d\n", dw.x, dw.y, dw.z, ow.x, ow.y, ow.z, (int)(g->drawerObject->GetParent() != NULL));
			}
		}

		// The blocks out of the view's list, the drawers in
		size_t w = 0;
		for (size_t i = 0; i < visible.size(); i++)
			if (taken.find(visible[i]->renderingComponent) == taken.end()) visible[w++] = visible[i];
		visible.resize(w);
		visible.insert(visible.end(), drawers.begin(), drawers.end());
		FrameProfiler::Instance().Counter("GpuCull.Groups", (f64)groupsCulled);
		FrameProfiler::Instance().Counter("GpuCull.Candidates", (f64)candidates);
	}

}
