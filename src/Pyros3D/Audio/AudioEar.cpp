//============================================================================
// Name        : AudioEar.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : What two ears make of where a sound is
//============================================================================

#include "AudioEar.h"
#include <Pyros3D/Ext/miniaudio/miniaudio.h>
#include <atomic>
#include <cstdlib>

namespace p3d { namespace detail {

	namespace {
		std::atomic<float> g_head[9];        // position, forward, up
		std::atomic<bool> g_headSaid(false);

		struct EarNode
		{
			ma_node_base base;
			EarModel ears;
			std::atomic<bool> placed;
			std::atomic<float> x, y, z;
			std::atomic<uint32_t> turn;      // each time it is placed anew: the ears begin again
			uint32_t turnSeen;
			float sampleRate;
		};

		void EarProcess(ma_node* node, const float** in, ma_uint32* inCount, float** out, ma_uint32* outCount)
		{
			EarNode* n = reinterpret_cast<EarNode*>(node);
			const ma_uint32 frames = *outCount;
			const float* src = in[0];
			float* dst = out[0];
			(void)inCount;
			if (!n->placed.load(std::memory_order_relaxed) || !g_headSaid.load(std::memory_order_relaxed))
			{
				std::memcpy(dst, src, sizeof(float) * 2 * frames);
				return;
			}
			const uint32_t turn = n->turn.load(std::memory_order_relaxed);
			if (turn != n->turnSeen) { n->turnSeen = turn; n->ears.Reset(); }
			const float px = g_head[0].load(std::memory_order_relaxed), py = g_head[1].load(std::memory_order_relaxed), pz = g_head[2].load(std::memory_order_relaxed);
			const float fx = g_head[3].load(std::memory_order_relaxed), fy = g_head[4].load(std::memory_order_relaxed), fz = g_head[5].load(std::memory_order_relaxed);
			const float ux = g_head[6].load(std::memory_order_relaxed), uy = g_head[7].load(std::memory_order_relaxed), uz = g_head[8].load(std::memory_order_relaxed);
			// right = forward x up
			const float rx = fy * uz - fz * uy, ry = fz * ux - fx * uz, rz = fx * uy - fy * ux;
			const float dx = n->x.load(std::memory_order_relaxed) - px, dy = n->y.load(std::memory_order_relaxed) - py, dz = n->z.load(std::memory_order_relaxed) - pz;
			const float side = dx * rx + dy * ry + dz * rz;
			const float front = dx * fx + dy * fy + dz * fz;
			const float flat = std::sqrt(side * side + front * front);
			// (on top of the head, or at it: neither side)
			float sinA = 0.f, cosA = 1.f;
			if (flat > 0.15f) { sinA = side / flat; cosA = front / flat; }
			n->ears.Process(src, dst, frames, sinA, cosA, n->sampleRate);
		}

		ma_node_vtable g_earVtable = { EarProcess, NULL, 1, 1, 0 };
	}

	void EarListenerSet(const float px, const float py, const float pz, const float fx, const float fy, const float fz, const float ux, const float uy, const float uz)
	{
		const float v[9] = { px, py, pz, fx, fy, fz, ux, uy, uz };
		for (int i = 0; i < 9; i++) g_head[i].store(v[i], std::memory_order_relaxed);
		g_headSaid.store(true, std::memory_order_relaxed);
	}

	void* EarNodeCreate(ma_engine* engine)
	{
		// PYROS_AUDIO_EARS=0: sounds shared out between left and right by angle alone, as before
		static const bool off = std::getenv("PYROS_AUDIO_EARS") != NULL && std::getenv("PYROS_AUDIO_EARS")[0] == '0';
		if (off || engine == NULL || ma_engine_get_channels(engine) != 2) return NULL;
		EarNode* n = new EarNode();
		n->placed.store(false); n->x.store(0.f); n->y.store(0.f); n->z.store(0.f); n->turn.store(0); n->turnSeen = 0;
		n->sampleRate = (float)ma_engine_get_sample_rate(engine);
		ma_node_config config = ma_node_config_init();
		const ma_uint32 channels[1] = { 2 };
		config.vtable = &g_earVtable;
		config.pInputChannels = channels;
		config.pOutputChannels = channels;
		if (ma_node_init(ma_engine_get_node_graph(engine), &config, NULL, &n->base) != MA_SUCCESS) { delete n; return NULL; }
		return n;
	}

	void EarNodeDestroy(void* node)
	{
		if (node == NULL) return;
		EarNode* n = reinterpret_cast<EarNode*>(node);
		ma_node_uninit(&n->base, NULL);
		delete n;
	}

	void EarNodeSet(void* node, const bool placed, const float x, const float y, const float z)
	{
		if (node == NULL) return;
		EarNode* n = reinterpret_cast<EarNode*>(node);
		n->x.store(x, std::memory_order_relaxed); n->y.store(y, std::memory_order_relaxed); n->z.store(z, std::memory_order_relaxed);
		if (placed != n->placed.load(std::memory_order_relaxed)) n->turn.fetch_add(1, std::memory_order_relaxed);
		n->placed.store(placed, std::memory_order_relaxed);
	}

} }
