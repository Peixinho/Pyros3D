//============================================================================
// Name        : LoadStats.h
// Author      : Duarte Peixinho
// Description : Process-wide accumulators for what asset loading costs, by
//               stage. Always on - a clock read and an atomic add per asset
//               - and safe from any thread, so a stage run on a loader
//               thread is counted the same as one on the main thread.
//
//               PYROS_LOAD_TRACE=1 prints the breakdown to stderr after
//               every scene load (see SceneSerializer::LoadSceneFromText).
//               Stage times are summed over threads, so with parallel
//               decoding they can exceed the wall time printed beside them.
//============================================================================

#ifndef LOADSTATS_H
#define LOADSTATS_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <chrono>

namespace p3d {

	class PYROS3D_API LoadStats
	{
	public:
		enum Stage
		{
			FileRead,		// bytes off disk, any asset
			TextureDecode,	// stb_image + edge bleed
			TextureUpload,	// CreateTexture: GPU upload and mips
			ModelParse,		// ModelLoader::Load (.p3dm / assimp)
			ModelBuild,		// Model ctor after the parse: submesh merging
			MaterialBuild,	// a scene/cell's material pool
			ObjectBuild,	// GameObjects and their components, all of it
			StageCount
		};

		static void Add(const Stage s, const f64 ms);
		static f64 Ms(const Stage s);
		static uint32 Calls(const Stage s);
		static void Reset();
		static const char* Name(const Stage s);

		static bool TraceEnabled();
		// One line per stage to stderr, headed by label and wall time.
		static void Print(const char* label, const f64 wallMs);

		struct Scope
		{
			explicit Scope(const Stage s) : stage(s), t0(std::chrono::steady_clock::now()) {}
			~Scope()
			{
				Add(stage, std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count());
			}
			Stage stage;
			std::chrono::steady_clock::time_point t0;
		};
	};

}

#endif /* LOADSTATS_H */
