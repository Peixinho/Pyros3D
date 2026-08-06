//============================================================================
// Name        : AudioEffectChain.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Filter -> EQ -> delay node-graph chain, shared by Sound and
//               AudioSource - not part of the public API (lives under
//               src/, not include/Pyros3D). Sound.h/AudioSource.h only
//               forward-declare p3d::detail::AudioEffectChain directly
//               (see either header's comment) - nothing besides
//               Sound.cpp/AudioSource.cpp needs this file.
//============================================================================

#ifndef PYROS3D_AUDIO_EFFECTCHAIN_H
#define	PYROS3D_AUDIO_EFFECTCHAIN_H

#include <Pyros3D/Audio/AudioManager.h>

struct ma_engine;
// ma_node is `typedef void ma_node;` in miniaudio - literally void, not a
// real struct - so the method signatures below take `void*` directly
// rather than redeclaring that typedef here (redundant, and one more
// thing to keep byte-for-byte identical to miniaudio's own definition for
// no benefit). Every real caller already has miniaudio.h included and
// passes an `ma_node*`, which is exactly a `void*` under the hood.

namespace p3d { namespace detail {

	// Up to three effects chained, in this fixed order, between one voice
	// (an ma_sound*, upcast to ma_node* - see AudioSource.cpp's comment on
	// why that upcast is valid) and wherever it already routes to (a bus, or
	// the engine's master endpoint): a shaping filter (low/high/band-pass),
	// a parametric EQ (peak/notch/shelf), then delay/echo. Shared by Sound
	// (one instance per pooled voice - all of a pool's voices are kept
	// configured identically, same as the filter-only version this replaced)
	// and AudioSource (one instance), so the node-graph wiring - the part
	// that's easy to get subtly wrong - is written exactly once.
	//
	// Any one stage can be set or cleared independently of the others.
	// Rewire() recomputes every currently-active stage's OUTPUT attachment
	// after each such call - it never recreates a node that did not change,
	// only re-points existing attachments - which is simpler to get right
	// than only patching the two attachments a given change actually
	// altered, at the cost of a couple of harmless redundant re-attachments
	// on unrelated, unchanged stages.
	class AudioEffectChain
	{
	public:

		AudioEffectChain();
		// Tears down every active stage. Does NOT reroute anything first -
		// the owning voice is assumed to be going away too (uninitializing
		// it fully detaches it from the graph regardless) - see
		// AudioSource::~AudioSource()'s identical reasoning for the filter it
		// used to own directly.
		void Destroy();

		// Every method below takes `engine`/`soundNode`/`target` as NULL when
		// the owning Sound/AudioSource isn't actually functional (no
		// AudioManager at construction) - in that state this only caches the
		// requested parameters (matching every other setter on
		// Sound/AudioSource - see AudioSource.h's comment on why) and never
		// touches miniaudio.

		void SetFilter(ma_engine* engine, void* soundNode, void* target, const uint32 type, const f32 cutoffHz, const uint32 order);
		void ClearFilter(void* soundNode, void* target);

		// gainDB is meaningless (and ignored) for AudioEQType::Notch - see
		// its enum comment.
		void SetEQ(ma_engine* engine, void* soundNode, void* target, const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q);
		void ClearEQ(void* soundNode, void* target);

		void SetDelay(ma_engine* engine, void* soundNode, void* target, const f32 delaySeconds, const f32 decay, const f32 wet, const f32 dry);
		void ClearDelay(void* soundNode, void* target);

		// Read-back - miniaudio exposes no config getters for any of these
		// node types, so every setter above also caches its own arguments
		// here (same reasoning as AudioSource.h's read-back section).
		uint32 filterType; f32 filterCutoff; uint32 filterOrder;
		uint32 eqType; f32 eqFrequency; f32 eqGainDB; f32 eqQ;
		bool hasDelay; f32 delaySeconds; f32 delayDecay; f32 delayWet; f32 delayDry;

	private:

		// Uninitializes and frees just the named stage's node, if any -
		// no rerouting. Dispatches on the stage's own cached *Type since
		// different filter/EQ types need different _node_uninit calls.
		void DestroyFilterNodeOnly();
		void DestroyEQNodeOnly();
		void DestroyDelayNodeOnly();

		// Re-attaches soundNode -> [active stages, in fixed order] -> target,
		// skipping whichever stages are currently inactive.
		void Rewire(void* soundNode, void* target);

		// Opaque (ma_lpf_node and friends are anonymous structs in
		// miniaudio - no tag to forward-declare, unlike ma_sound/ma_engine -
		// so these are plain void*, cast back in the .cpp; the same idea one
		// level blunter than the ma_sound* PIMPL already used elsewhere).
		void* filterNode;
		void* eqNode;
		void* delayNode;
	};

} }

#endif	/* PYROS3D_AUDIO_EFFECTCHAIN_H */
