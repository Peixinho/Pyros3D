//============================================================================
// Name        : FrameGraph.cpp
// Author      : Duarte Peixinho
// Description : The renderer's frame graph - see FrameGraph.h
//============================================================================

#include <Pyros3D/Rendering/Renderer/FrameGraph.h>

namespace p3d {

	void FrameGraph::Begin()
	{
		passes.clear();
	}

	void FrameGraph::AddPass(const std::string &name, std::vector<Write> writes, std::vector<gfx::Texture> reads, Record record, const bool keep)
	{
		Pass p;
		p.name = name; p.writes = std::move(writes); p.reads = std::move(reads); p.record = std::move(record); p.keep = keep;
		passes.push_back(std::move(p));
	}

	gfx::Access FrameGraph::StateOf(const gfx::Texture t) const
	{
		std::unordered_map<uint32_t, gfx::Access>::const_iterator i = state.find(t.id);
		return i == state.end() ? gfx::Access::ShaderRead : i->second;
	}

	void FrameGraph::Move(gfx::CommandList* cl, const gfx::Texture t, const gfx::Access to, const bool discard, std::vector<gfx::Barrier> &out)
	{
		if (!t) return;
		const gfx::Access from = discard ? gfx::Access::Undefined : StateOf(t);
		state[t.id] = to;
		// (the same state twice: nothing between - but a discard is said, and storage written twice is ordered)
		if (from == to && to != gfx::Access::ShaderWrite) return;
		gfx::Barrier b; b.texture = t; b.from = from; b.to = to;
		out.push_back(b);
	}

	void FrameGraph::Use(gfx::CommandList* cl, std::initializer_list<std::pair<gfx::Texture, gfx::Access> > uses)
	{
		std::vector<gfx::Barrier> b;
		for (const std::pair<gfx::Texture, gfx::Access> &u : uses)
		{
			Move(cl, u.first, u.second, false, b);
			touched.push_back(u.first);
		}
		if (!b.empty()) { cl->Barriers(b.data(), (uint32_t)b.size()); barriersMade += (uint32_t)b.size(); }
	}

	void FrameGraph::Execute(gfx::CommandList* cl)
	{
		passesAdded = (uint32_t)passes.size(); passesRecorded = 0; barriersMade = 0;
		culled.clear();
		// Which are recorded: from the last back - a pass is wanted when it is kept, writes what lasts past the frame,
		// or writes what a wanted pass after it reads
		std::vector<bool> wanted(passes.size(), false);
		std::unordered_set<uint32_t> read;
		for (size_t k = passes.size(); k-- > 0;)
		{
			const Pass &p = passes[k];
			bool w = p.keep || p.writes.empty();
			for (const Write &x : p.writes)
				if (!transient.count(x.texture.id) || read.count(x.texture.id)) w = true;
			wanted[k] = w;
			if (!w) { culled.push_back(p.name); continue; }
			// (what it writes, it made: what passes before it wrote, they need not, unless it read it too)
			for (const Write &x : p.writes) if (x.discard) read.erase(x.texture.id);
			for (const gfx::Texture &t : p.reads) read.insert(t.id);
			for (const Write &x : p.writes) if (!x.discard) read.insert(x.texture.id);     // (drawn over: what was there is read)
		}
		for (size_t k = 0; k < passes.size(); k++)
		{
			if (!wanted[k]) continue;
			Pass &p = passes[k];
			std::vector<gfx::Barrier> b;
			for (const gfx::Texture &t : p.reads) Move(cl, t, gfx::Access::ShaderRead, false, b);
			for (const Write &x : p.writes) Move(cl, x.texture, x.access, x.discard, b);
			if (!b.empty()) { cl->Barriers(b.data(), (uint32_t)b.size()); barriersMade += (uint32_t)b.size(); }
			touched.clear();
			p.record(cl);
			passesRecorded++;
			// (back where its readers sample it)
			b.clear();
			for (const Write &x : p.writes) if (x.rest) Move(cl, x.texture, gfx::Access::ShaderRead, false, b);
			for (const gfx::Texture &t : touched) Move(cl, t, gfx::Access::ShaderRead, false, b);
			if (!b.empty()) { cl->Barriers(b.data(), (uint32_t)b.size()); barriersMade += (uint32_t)b.size(); }
		}
		passes.clear();
	}

}
