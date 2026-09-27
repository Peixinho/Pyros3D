//============================================================================
// Name        : LoadStats.cpp
// Author      : Duarte Peixinho
// Description : See LoadStats.h.
//============================================================================

#include <Pyros3D/Utils/Streaming/LoadStats.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace p3d {

	namespace {
		// Microseconds, so a plain integer atomic will do.
		std::atomic<uint64> g_us[LoadStats::StageCount];
		std::atomic<uint32> g_calls[LoadStats::StageCount];
	}

	void LoadStats::Add(const Stage s, const f64 ms)
	{
		g_us[s].fetch_add((uint64)(ms * 1000.0), std::memory_order_relaxed);
		g_calls[s].fetch_add(1, std::memory_order_relaxed);
	}

	f64 LoadStats::Ms(const Stage s) { return (f64)g_us[s].load(std::memory_order_relaxed) / 1000.0; }
	uint32 LoadStats::Calls(const Stage s) { return g_calls[s].load(std::memory_order_relaxed); }

	void LoadStats::Reset()
	{
		for (uint32 i = 0; i < StageCount; i++)
		{
			g_us[i].store(0, std::memory_order_relaxed);
			g_calls[i].store(0, std::memory_order_relaxed);
		}
	}

	const char* LoadStats::Name(const Stage s)
	{
		switch (s)
		{
		case FileRead: return "FileRead";
		case TextureDecode: return "TextureDecode";
		case TextureUpload: return "TextureUpload";
		case ModelParse: return "ModelParse";
		case ModelBuild: return "ModelBuild";
		case MaterialBuild: return "MaterialBuild";
		case ObjectBuild: return "ObjectBuild";
		default: return "?";
		}
	}

	bool LoadStats::TraceEnabled()
	{
		static const bool on = [] {
			const char* v = std::getenv("PYROS_LOAD_TRACE");
			return v && v[0] && v[0] != '0';
		}();
		return on;
	}

	void LoadStats::Print(const char* label, const f64 wallMs)
	{
		std::fprintf(stderr, "[load] %s: %.1f ms wall\n", label, wallMs);
		for (uint32 i = 0; i < StageCount; i++)
			if (Calls((Stage)i))
				std::fprintf(stderr, "[load]   %-14s %8.1f ms  x%u\n", Name((Stage)i), Ms((Stage)i), Calls((Stage)i));
		std::fflush(stderr);
	}

}
