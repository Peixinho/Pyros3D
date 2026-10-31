//============================================================================
// Name        : FrameGraph.h
// Author      : Duarte Peixinho
// Description : The renderer's frame, as a graph: each pass says what it writes (and how) and what it reads, and is
//               recorded later, when the frame is executed. The graph keeps every texture's state from pass to pass
//               and frame to frame, and makes the barriers itself - no pass says what state another left a texture
//               in. A pass whose writes nothing reads (and that writes nothing kept past the frame) is not recorded.
//
//               Between passes a texture rests where it can be sampled (ShaderRead): a pass's writes go to their
//               state when it starts and back when it ends - its readers, by bind group, find it sampleable. Within
//               a pass made of several steps (a blur's two ways, a chain of mips), Use() moves a texture between them.
//============================================================================

#ifndef PYROS_RENDERER_FRAMEGRAPH_H
#define PYROS_RENDERER_FRAMEGRAPH_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace p3d {

	class PYROS3D_API FrameGraph
	{
	public:
		// One texture as a pass touches it. `discard`: what it held before is not wanted (it comes from Undefined: a
		// tiler need not load it). `rest`: back to ShaderRead when the pass ends (off for one never sampled: a depth
		// target only drawn to).
		struct Write { gfx::Texture texture; gfx::Access access; bool discard = false; bool rest = true; };
		typedef std::function<void(gfx::CommandList*)> Record;

		// A new frame: last frame's passes gone, the textures' states kept
		void Begin();
		// `keep`: recorded whatever reads its writes (it writes something outside the graph - the backbuffer)
		void AddPass(const std::string &name, std::vector<Write> writes, std::vector<gfx::Texture> reads, Record record, const bool keep = false);
		// A texture whose contents are this frame's only (nothing reads it in the next): a pass writing only these, and
		// read by no pass recorded after it, is not recorded
		void Transient(const gfx::Texture texture) { transient.insert(texture.id); }
		// Within a pass being recorded: these textures into these states, from where they are
		void Use(gfx::CommandList* cl, std::initializer_list<std::pair<gfx::Texture, gfx::Access> > uses);
		// The passes, in the order added, culled, each with its barriers
		void Execute(gfx::CommandList* cl);
		// A texture destroyed: its state forgotten (a new one may be given its id)
		void Forget(const gfx::Texture texture) { state.erase(texture.id); transient.erase(texture.id); }
		// The targets made again (a new size): every state forgotten - between frames all rest in ShaderRead but the
		// ones that do not rest, made again with them - and which are transient said again
		void Reset() { state.clear(); transient.clear(); }

		// The last frame's: passes added, passes recorded, barriers made
		uint32_t passesAdded = 0, passesRecorded = 0, barriersMade = 0;
		std::vector<std::string> culled;

	private:
		struct Pass { std::string name; std::vector<Write> writes; std::vector<gfx::Texture> reads; Record record; bool keep; };
		std::vector<Pass> passes;
		std::unordered_map<uint32_t, gfx::Access> state;     // (by texture id; not there: ShaderRead, as the devices make them)
		std::unordered_set<uint32_t> transient;
		std::vector<gfx::Texture> touched;                   // (within the pass being recorded: what Use() moved)
		gfx::Access StateOf(const gfx::Texture t) const;
		void Move(gfx::CommandList* cl, const gfx::Texture t, const gfx::Access to, const bool discard, std::vector<gfx::Barrier> &out);
	};

}

#endif
