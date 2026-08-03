//============================================================================
// Name        : AudioFilterNode.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shared low-pass/high-pass node-graph wiring for Sound and
//               AudioSource - not part of the public API (lives under src/,
//               not include/Pyros3D).
//============================================================================

#ifndef PYROS3D_AUDIO_FILTERNODE_H
#define	PYROS3D_AUDIO_FILTERNODE_H

#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Ext/miniaudio/miniaudio.h>

// `inline` (not an anonymous namespace) deliberately - this header is
// included by two translation units (Sound.cpp, AudioSource.cpp), and inline
// is what lets the same definition live in both without an ODR violation,
// while still keeping this out of any installed/public header.
namespace p3d { namespace detail {

	// Both `soundNode`/`target` are ma_sound*/ma_sound_group* respectively,
	// upcast to ma_node* - valid per ma_sound's "engineNode must be the first
	// member" guarantee (ma_sound_group is a plain typedef of ma_sound, not a
	// distinct type).

	// Reroutes `soundNode` directly to `target`, then tears down whatever
	// filter node it used to run through, if any. Every filter node type
	// fully detaches itself from the graph on uninit (see ma_node_uninit()'s
	// doc), so the reroute and this teardown are each independently safe
	// regardless of order - sequenced this way purely so `soundNode` is never
	// silent even for one buffer.
	inline void DetachFilterNode(ma_node* soundNode, ma_node* target, void* &filterNode, const uint32 filterType)
	{
		if (filterNode == NULL) return;

		ma_node_attach_output_bus(soundNode, 0, target, 0);

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
		filterNode = NULL;
	}

	// Tears a filter node down without rerouting anything first - for object
	// destruction, where `soundNode` is about to be fully uninitialized
	// itself (which detaches it from the graph regardless), so rerouting it
	// first would be wasted work on a node that's disappearing anyway.
	inline void DestroyFilterNode(void* &filterNode, const uint32 filterType)
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
		filterNode = NULL;
	}

	// Creates and inserts a new filter node between `soundNode` and `target`.
	// Returns the new node on success; on failure, `soundNode` is left
	// attached directly to `target` (the safe no-filter fallback) rather than
	// partially wired, and this returns NULL.
	inline void* AttachFilterNode(ma_engine* engine, ma_node* soundNode, ma_node* target, const uint32 type, const f32 cutoffHz, const uint32 order)
	{
		ma_node_graph* graph = ma_engine_get_node_graph(engine);
		ma_uint32 channels = ma_engine_get_channels(engine);
		ma_uint32 sampleRate = ma_engine_get_sample_rate(engine);

		if (type == AudioFilterType::LowPass)
		{
			ma_lpf_node* node = new ma_lpf_node();
			ma_lpf_node_config cfg = ma_lpf_node_config_init(channels, sampleRate, cutoffHz, order);
			if (ma_lpf_node_init(graph, &cfg, NULL, node) != MA_SUCCESS)
			{
				delete node;
				ma_node_attach_output_bus(soundNode, 0, target, 0);
				return NULL;
			}
			ma_node_attach_output_bus(reinterpret_cast<ma_node*>(node), 0, target, 0);
			ma_node_attach_output_bus(soundNode, 0, reinterpret_cast<ma_node*>(node), 0);
			return node;
		}
		else if (type == AudioFilterType::HighPass)
		{
			ma_hpf_node* node = new ma_hpf_node();
			ma_hpf_node_config cfg = ma_hpf_node_config_init(channels, sampleRate, cutoffHz, order);
			if (ma_hpf_node_init(graph, &cfg, NULL, node) != MA_SUCCESS)
			{
				delete node;
				ma_node_attach_output_bus(soundNode, 0, target, 0);
				return NULL;
			}
			ma_node_attach_output_bus(reinterpret_cast<ma_node*>(node), 0, target, 0);
			ma_node_attach_output_bus(soundNode, 0, reinterpret_cast<ma_node*>(node), 0);
			return node;
		}

		// AudioFilterType::None (or anything unrecognized) - straight to target.
		ma_node_attach_output_bus(soundNode, 0, target, 0);
		return NULL;
	}

} }

#endif	/* PYROS3D_AUDIO_FILTERNODE_H */
