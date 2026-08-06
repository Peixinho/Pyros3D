//============================================================================
// Name        : AudioEffectChain.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Filter -> EQ -> delay node-graph chain
//============================================================================

#include "AudioEffectChain.h"
#include <Pyros3D/Ext/miniaudio/miniaudio.h>

namespace p3d { namespace detail {

	AudioEffectChain::AudioEffectChain()
		: filterType(AudioFilterType::None), filterCutoff(0.f), filterOrder(2),
		eqType(AudioEQType::None), eqFrequency(0.f), eqGainDB(0.f), eqQ(1.f),
		hasDelay(false), delaySeconds(0.f), delayDecay(0.f), delayWet(1.f), delayDry(1.f),
		filterNode(NULL), eqNode(NULL), delayNode(NULL)
	{
	}

	void AudioEffectChain::Destroy()
	{
		DestroyFilterNodeOnly();
		DestroyEQNodeOnly();
		DestroyDelayNodeOnly();
	}

	// ****************************** Rewiring ********************************

	void AudioEffectChain::Rewire(void* soundNodeVoid, void* targetVoid)
	{
		ma_node* soundNode = reinterpret_cast<ma_node*>(soundNodeVoid);
		ma_node* target = reinterpret_cast<ma_node*>(targetVoid);

		ma_node* prev = soundNode;
		if (filterNode != NULL)
		{
			ma_node_attach_output_bus(prev, 0, reinterpret_cast<ma_node*>(filterNode), 0);
			prev = reinterpret_cast<ma_node*>(filterNode);
		}
		if (eqNode != NULL)
		{
			ma_node_attach_output_bus(prev, 0, reinterpret_cast<ma_node*>(eqNode), 0);
			prev = reinterpret_cast<ma_node*>(eqNode);
		}
		if (delayNode != NULL)
		{
			ma_node_attach_output_bus(prev, 0, reinterpret_cast<ma_node*>(delayNode), 0);
			prev = reinterpret_cast<ma_node*>(delayNode);
		}
		ma_node_attach_output_bus(prev, 0, target, 0);
	}

	// ******************************** Filter *********************************

	void AudioEffectChain::DestroyFilterNodeOnly()
	{
		if (filterNode == NULL) return;

		if (filterType == AudioFilterType::LowPass)
		{
			ma_lpf_node_uninit(reinterpret_cast<ma_lpf_node*>(filterNode), NULL);
			delete reinterpret_cast<ma_lpf_node*>(filterNode);
		}
		else if (filterType == AudioFilterType::HighPass)
		{
			ma_hpf_node_uninit(reinterpret_cast<ma_hpf_node*>(filterNode), NULL);
			delete reinterpret_cast<ma_hpf_node*>(filterNode);
		}
		else if (filterType == AudioFilterType::BandPass)
		{
			ma_bpf_node_uninit(reinterpret_cast<ma_bpf_node*>(filterNode), NULL);
			delete reinterpret_cast<ma_bpf_node*>(filterNode);
		}
		filterNode = NULL;
	}

	void AudioEffectChain::SetFilter(ma_engine* engine, void* soundNode, void* target, const uint32 type, const f32 cutoffHz, const uint32 order)
	{
		if (type == AudioFilterType::None) { ClearFilter(soundNode, target); return; }

		filterCutoff = cutoffHz;
		filterOrder = order;

		if (engine == NULL)
		{
			// Not functional (no AudioManager) - cache the request (matching
			// every other setter on Sound/AudioSource) and stop; there is no
			// node to create.
			filterType = type;
			return;
		}

		DestroyFilterNodeOnly();

		ma_node_graph* graph = ma_engine_get_node_graph(engine);
		ma_uint32 channels = ma_engine_get_channels(engine);
		ma_uint32 sampleRate = ma_engine_get_sample_rate(engine);

		void* node = NULL;
		if (type == AudioFilterType::LowPass)
		{
			ma_lpf_node* n = new ma_lpf_node();
			ma_lpf_node_config cfg = ma_lpf_node_config_init(channels, sampleRate, cutoffHz, order);
			if (ma_lpf_node_init(graph, &cfg, NULL, n) == MA_SUCCESS) node = n; else delete n;
		}
		else if (type == AudioFilterType::HighPass)
		{
			ma_hpf_node* n = new ma_hpf_node();
			ma_hpf_node_config cfg = ma_hpf_node_config_init(channels, sampleRate, cutoffHz, order);
			if (ma_hpf_node_init(graph, &cfg, NULL, n) == MA_SUCCESS) node = n; else delete n;
		}
		else if (type == AudioFilterType::BandPass)
		{
			ma_bpf_node* n = new ma_bpf_node();
			ma_bpf_node_config cfg = ma_bpf_node_config_init(channels, sampleRate, cutoffHz, order);
			if (ma_bpf_node_init(graph, &cfg, NULL, n) == MA_SUCCESS) node = n; else delete n;
		}

		filterNode = node;
		// Reflects what is REALLY playing, not what was asked for - a failed
		// node creation must not claim a filter is active.
		filterType = (node != NULL) ? type : AudioFilterType::None;
		Rewire(soundNode, target);
	}

	void AudioEffectChain::ClearFilter(void* soundNode, void* target)
	{
		if (filterNode != NULL)
		{
			DestroyFilterNodeOnly();
			Rewire(soundNode, target);
		}
		filterType = AudioFilterType::None;
		filterCutoff = 0.f;
		filterOrder = 2;
	}

	// ********************************** EQ ***********************************

	void AudioEffectChain::DestroyEQNodeOnly()
	{
		if (eqNode == NULL) return;

		if (eqType == AudioEQType::Peak)
		{
			ma_peak_node_uninit(reinterpret_cast<ma_peak_node*>(eqNode), NULL);
			delete reinterpret_cast<ma_peak_node*>(eqNode);
		}
		else if (eqType == AudioEQType::Notch)
		{
			ma_notch_node_uninit(reinterpret_cast<ma_notch_node*>(eqNode), NULL);
			delete reinterpret_cast<ma_notch_node*>(eqNode);
		}
		else if (eqType == AudioEQType::LowShelf)
		{
			ma_loshelf_node_uninit(reinterpret_cast<ma_loshelf_node*>(eqNode), NULL);
			delete reinterpret_cast<ma_loshelf_node*>(eqNode);
		}
		else if (eqType == AudioEQType::HighShelf)
		{
			ma_hishelf_node_uninit(reinterpret_cast<ma_hishelf_node*>(eqNode), NULL);
			delete reinterpret_cast<ma_hishelf_node*>(eqNode);
		}
		eqNode = NULL;
	}

	void AudioEffectChain::SetEQ(ma_engine* engine, void* soundNode, void* target, const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q)
	{
		if (type == AudioEQType::None) { ClearEQ(soundNode, target); return; }

		eqFrequency = frequencyHz;
		eqGainDB = gainDB;
		eqQ = q;

		if (engine == NULL)
		{
			eqType = type;
			return;
		}

		DestroyEQNodeOnly();

		ma_node_graph* graph = ma_engine_get_node_graph(engine);
		ma_uint32 channels = ma_engine_get_channels(engine);
		ma_uint32 sampleRate = ma_engine_get_sample_rate(engine);

		void* node = NULL;
		if (type == AudioEQType::Peak)
		{
			ma_peak_node* n = new ma_peak_node();
			ma_peak_node_config cfg = ma_peak_node_config_init(channels, sampleRate, gainDB, q, frequencyHz);
			if (ma_peak_node_init(graph, &cfg, NULL, n) == MA_SUCCESS) node = n; else delete n;
		}
		else if (type == AudioEQType::Notch)
		{
			// No gain parameter - a notch only removes energy at
			// `frequencyHz`, it has nothing to boost/cut.
			ma_notch_node* n = new ma_notch_node();
			ma_notch_node_config cfg = ma_notch_node_config_init(channels, sampleRate, q, frequencyHz);
			if (ma_notch_node_init(graph, &cfg, NULL, n) == MA_SUCCESS) node = n; else delete n;
		}
		else if (type == AudioEQType::LowShelf)
		{
			ma_loshelf_node* n = new ma_loshelf_node();
			ma_loshelf_node_config cfg = ma_loshelf_node_config_init(channels, sampleRate, gainDB, q, frequencyHz);
			if (ma_loshelf_node_init(graph, &cfg, NULL, n) == MA_SUCCESS) node = n; else delete n;
		}
		else if (type == AudioEQType::HighShelf)
		{
			ma_hishelf_node* n = new ma_hishelf_node();
			ma_hishelf_node_config cfg = ma_hishelf_node_config_init(channels, sampleRate, gainDB, q, frequencyHz);
			if (ma_hishelf_node_init(graph, &cfg, NULL, n) == MA_SUCCESS) node = n; else delete n;
		}

		eqNode = node;
		eqType = (node != NULL) ? type : AudioEQType::None;
		Rewire(soundNode, target);
	}

	void AudioEffectChain::ClearEQ(void* soundNode, void* target)
	{
		if (eqNode != NULL)
		{
			DestroyEQNodeOnly();
			Rewire(soundNode, target);
		}
		eqType = AudioEQType::None;
		eqFrequency = 0.f;
		eqGainDB = 0.f;
		eqQ = 1.f;
	}

	// ******************************** Delay **********************************

	void AudioEffectChain::DestroyDelayNodeOnly()
	{
		if (delayNode == NULL) return;
		ma_delay_node_uninit(reinterpret_cast<ma_delay_node*>(delayNode), NULL);
		delete reinterpret_cast<ma_delay_node*>(delayNode);
		delayNode = NULL;
	}

	void AudioEffectChain::SetDelay(ma_engine* engine, void* soundNode, void* target, const f32 delaySecondsIn, const f32 decayIn, const f32 wetIn, const f32 dryIn)
	{
		delaySeconds = (delaySecondsIn > 0.f) ? delaySecondsIn : 0.f;
		// A decay of 1 or more would never die out - clamp just under so an
		// echo always eventually fades rather than repeating forever at (near)
		// full strength.
		delayDecay = (decayIn < 0.f) ? 0.f : ((decayIn >= 1.f) ? 0.999f : decayIn);
		delayWet = wetIn;
		delayDry = dryIn;

		if (engine == NULL)
		{
			hasDelay = true;
			return;
		}

		DestroyDelayNodeOnly();

		ma_node_graph* graph = ma_engine_get_node_graph(engine);
		ma_uint32 channels = ma_engine_get_channels(engine);
		ma_uint32 sampleRate = ma_engine_get_sample_rate(engine);

		// miniaudio's delay is expressed in whole PCM frames, not seconds -
		// converted here using the engine's real sample rate. A zero-length
		// buffer is degenerate (nothing to delay into), so the minimum is
		// one frame regardless of how small a `delaySeconds` was requested.
		ma_uint32 delayFrames = (ma_uint32)(delaySeconds * (f32)sampleRate);
		if (delayFrames == 0) delayFrames = 1;

		ma_delay_node* n = new ma_delay_node();
		ma_delay_node_config cfg = ma_delay_node_config_init(channels, sampleRate, delayFrames, delayDecay);
		if (ma_delay_node_init(graph, &cfg, NULL, n) == MA_SUCCESS)
		{
			ma_delay_node_set_wet(n, delayWet);
			ma_delay_node_set_dry(n, delayDry);
			delayNode = n;
			hasDelay = true;
		}
		else
		{
			delete n;
			delayNode = NULL;
			hasDelay = false;
		}
		Rewire(soundNode, target);
	}

	void AudioEffectChain::ClearDelay(void* soundNode, void* target)
	{
		if (delayNode != NULL)
		{
			DestroyDelayNodeOnly();
			Rewire(soundNode, target);
		}
		hasDelay = false;
		delaySeconds = 0.f;
		delayDecay = 0.f;
		delayWet = 1.f;
		delayDry = 1.f;
	}

} }
