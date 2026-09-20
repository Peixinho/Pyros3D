// Does a baked environment survive save/load?
//
// The SH projection being correct is worth nothing if the nine numbers
// do not reach the .json and come back. That failure is silent in the
// worst way: the scene just relights itself the next time it is opened,
// and the bake looks like it never happened.
//
//   c++ -std=c++17 -DLUA_BINDINGS -I include -I shared \
//       -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       -I /opt/homebrew/opt/lua@5.4/include/lua \
//       tools/tests/sh_scene_roundtrip.cpp -o /tmp/sh_scene_roundtrip \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   /tmp/sh_scene_roundtrip
//
// REBUILD libPyrosEngine first if SceneMeta changed. This test adds a
// field to that struct, and linking it against a stale library segfaults
// inside SaveScene with no output at all - which is exactly the stale
// standalone-binary trap, and cost time here before being recognised.
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
using namespace p3d;

static int failures = 0;
static void check(bool c, const char* what, const std::string& extra = "")
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", what, extra.empty()?"":" - ", extra.c_str());
	if(!c) failures++;
}

static void checkNear(f32 got, f32 want, f32 tol, const char* what)
{
	char buf[160];
	snprintf(buf, sizeof(buf), "got %.6f want %.6f", got, want);
	check(fabsf(got - want) <= tol, what, buf);
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);
	const std::string path = std::string(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp") + "/sh_roundtrip_scene.json";

	SceneMeta saved;
	saved.ambientMode = 2;
	// Distinct, non-round values per coefficient and per channel, so a
	// transposed index or a dropped channel cannot pass by symmetry.
	for (uint32 i = 0; i < 9; i++)
		saved.ambientSH[i] = Vec4(0.1f*(i+1), -0.25f*(i+1), 0.37f*(i+1), 0.f);

	// Heap-allocated and deliberately never destroyed: SceneGraph's
	// destructor tears down GPU-owning resources, and there is no render
	// device in this process to tear them down against. Not what is being
	// tested, and the same reason the other standalone tests here avoid
	// full teardown.
	check(SceneSerializer::SaveScene(new SceneGraph(), path, NULL, &saved), "SaveScene with a baked environment");

	{
		std::ifstream f(path);
		std::stringstream ss; ss << f.rdbuf();
		check(ss.str().find("ambientSH") != std::string::npos, "the .json actually contains ambientSH");
	}

	SceneMeta loaded;
	check(SceneSerializer::LoadScene(new SceneGraph(), path, NULL, NULL, NULL, &loaded), "LoadScene");

	check(loaded.ambientMode == 2, "ambientMode survived");
	f32 worst = 0.f; uint32 worstIdx = 0;
	for (uint32 i = 0; i < 9; i++)
	{
		const f32 d = fabsf(loaded.ambientSH[i].x - saved.ambientSH[i].x)
		            + fabsf(loaded.ambientSH[i].y - saved.ambientSH[i].y)
		            + fabsf(loaded.ambientSH[i].z - saved.ambientSH[i].z);
		if (d > worst) { worst = d; worstIdx = i; }
	}
	check(worst < 1e-6f, "all nine coefficients survived, in order and per channel",
		"worst delta " + std::to_string(worst) + " at index " + std::to_string(worstIdx));

	// A scene that never baked must not carry 27 zeroes, and must load
	// with the defaults rather than a black environment.
	{
		SceneMeta plain;
		const std::string p2 = path + ".plain.json";
		SceneSerializer::SaveScene(new SceneGraph(), p2, NULL, &plain);
		std::ifstream f(p2); std::stringstream ss; ss << f.rdbuf();
		check(ss.str().find("ambientSH") == std::string::npos,
			"an un-baked scene writes no ambientSH key at all");
	}

	// ---- the probe grid ------------------------------------------------
	//
	// Bigger payload, same silent failure: a bake that does not survive
	// save/load is a bake nobody gets. Values are distinct per probe, per
	// coefficient and per channel so a flattening bug, a transposed index
	// or a dropped channel cannot pass by symmetry.
	{
		SceneMeta withGrid;
		withGrid.ambientMode = 2;
		check(withGrid.ambientProbes.Allocate(Vec3(-4.f, 0.5f, 2.f), Vec3(1.5f, 2.5f, 3.5f), 3, 2, 4),
			"allocate a 3x2x4 probe grid");
		for (uint32 p = 0; p < withGrid.ambientProbes.ProbeCount(); p++)
			for (uint32 c = 0; c < SphericalHarmonicsL2::kCoefficientCount; c++)
				withGrid.ambientProbes.probes[p].coefficients[c] =
					Vec3(0.01f * (f32)(p * 9 + c), -0.02f * (f32)(p + 1), 0.03f * (f32)(c + 1));

		const std::string gp = path + ".grid.json";
		check(SceneSerializer::SaveScene(new SceneGraph(), gp, NULL, &withGrid), "SaveScene with a probe grid");

		SceneMeta back;
		check(SceneSerializer::LoadScene(new SceneGraph(), gp, NULL, NULL, NULL, &back), "LoadScene with a probe grid");
		check(back.ambientProbes.IsValid(), "the loaded grid is valid");
		check(back.ambientProbes.counts[0] == 3 && back.ambientProbes.counts[1] == 2
			&& back.ambientProbes.counts[2] == 4, "grid dimensions survived");
		checkNear(back.ambientProbes.origin.x, -4.f, 1e-5f, "grid origin survived");
		checkNear(back.ambientProbes.spacing.z, 3.5f, 1e-5f, "grid spacing survived");

		f32 worst = 0.f;
		for (uint32 p = 0; p < withGrid.ambientProbes.ProbeCount(); p++)
			for (uint32 c = 0; c < SphericalHarmonicsL2::kCoefficientCount; c++)
			{
				const Vec3 &a2 = withGrid.ambientProbes.probes[p].coefficients[c];
				const Vec3 &b2 = back.ambientProbes.probes[p].coefficients[c];
				worst = std::max(worst, fabsf(a2.x-b2.x) + fabsf(a2.y-b2.y) + fabsf(a2.z-b2.z));
			}
		check(worst < 1e-6f, "every probe coefficient survived, in order and per channel",
			"worst delta " + std::to_string(worst));

		// And that sampling the reloaded grid agrees with the original -
		// the numbers surviving is not the same as the geometry surviving.
		const Vec3 q(-1.2f, 3.0f, 7.0f);
		const Vec3 e1 = withGrid.ambientProbes.AmbientIrradianceAt(q, Vec3(0,1,0));
		const Vec3 e2 = back.ambientProbes.AmbientIrradianceAt(q, Vec3(0,1,0));
		checkNear(e2.x, e1.x, 1e-5f, "sampling the reloaded grid matches the original");
	}

	printf("\n%s  sh_roundtrip: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
