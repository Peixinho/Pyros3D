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

	printf("\n%s  sh_roundtrip: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
