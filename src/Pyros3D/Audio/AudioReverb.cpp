//============================================================================
// Name        : AudioReverb.cpp
// Description : ReverbModel (AudioReverb.h) as a miniaudio node.
//============================================================================

#include "AudioReverb.h"
#include <Pyros3D/Ext/miniaudio/miniaudio.h>
#include <atomic>
#include <mutex>

namespace p3d { namespace detail {

	namespace {
		struct ReverbNode
		{
			ma_node_base base;
			ReverbModel model;
			// (settings are made on the game's thread and used on the audio one: handed over
			// here, and taken by the audio thread when it can have the lock without waiting)
			std::mutex guard;
			ReverbSettings pending;
			std::atomic<bool> changed;
		};

		void ReverbProcess(ma_node* node, const float** in, ma_uint32* inCount, float** out, ma_uint32* outCount)
		{
			ReverbNode* n = reinterpret_cast<ReverbNode*>(node);
			(void)inCount;
			if (n->changed.load(std::memory_order_acquire) && n->guard.try_lock())
			{
				n->model.Set(n->pending);
				n->changed.store(false, std::memory_order_release);
				n->guard.unlock();
			}
			n->model.Process(in[0], out[0], *outCount);
		}

		ma_node_vtable g_reverbVtable = { ReverbProcess, NULL, 1, 1, 0 };
	}

	void* ReverbNodeCreate(ma_engine* engine)
	{
		if (engine == NULL || ma_engine_get_channels(engine) != 2) return NULL;
		ReverbNode* n = new ReverbNode();
		n->model.Init((float)ma_engine_get_sample_rate(engine));
		n->changed.store(false);
		ma_node_config config = ma_node_config_init();
		const ma_uint32 channels[1] = { 2 };
		config.vtable = &g_reverbVtable;
		config.pInputChannels = channels;
		config.pOutputChannels = channels;
		if (ma_node_init(ma_engine_get_node_graph(engine), &config, NULL, &n->base) != MA_SUCCESS) { delete n; return NULL; }
		return n;
	}

	void ReverbNodeDestroy(void* node)
	{
		if (node == NULL) return;
		ReverbNode* n = reinterpret_cast<ReverbNode*>(node);
		ma_node_uninit(&n->base, NULL);
		delete n;
	}

	void ReverbNodeSet(void* node, const ReverbSettings &settings)
	{
		if (node == NULL) return;
		ReverbNode* n = reinterpret_cast<ReverbNode*>(node);
		std::lock_guard<std::mutex> lock(n->guard);
		n->pending = settings;
		n->changed.store(true, std::memory_order_release);
	}

} }
