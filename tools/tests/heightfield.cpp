// Terrain tiles: heights and their interpolation, level-of-detail meshes and
// their skirts, and the physics shape - a ball dropped on a tile that sits
// inside a moved cell must land on the terrain where it is drawn, which also
// covers bodies being created in world space for children. No render device.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       -I src/Pyros3D/Ext/box3d/include \
//       tools/tests/heightfield.cpp -o /tmp/heightfield \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/heightfield
#include <filesystem>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/Physics/Components/HeightField/PhysicsHeightField.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image_write.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	fflush(stdout);
	if (!cond) failures++;
}
static bool near(f32 a, f32 b, f32 eps) { return std::fabs(a - b) <= eps; }

int main()
{
	// A 65x65 heightmap: a smooth hill. 8-bit, so a pixel is 1/255 of the
	// 51 m scale = 0.2 m per step, and every height is exact.
	const int N = 65;
	std::vector<unsigned char> px(N * N);
	for (int r = 0; r < N; r++)
		for (int c = 0; c < N; c++)
		{
			const f32 dx = (c - 32) / 32.f, dz = (r - 32) / 32.f;
			px[r * N + c] = (unsigned char)std::lround(255.f * std::max(0.f, 1.f - (dx * dx + dz * dz)));
		}
	const std::string png = (std::filesystem::temp_directory_path() / "pyros_heightfield_test.png").string();
	stbi_write_png(png.c_str(), N, N, 1, px.data(), N);

	HeightfieldData data;
	check(HeightfieldData::LoadFile(png, 128.f, 51.f, -1.f, data), "loads an 8-bit heightmap");
	check(data.samples == 65 && near(data.Spacing(), 2.f, 1e-5f), "65 samples over 128 m is 2 m apart");
	check(near(data.At(32, 32), 50.f, 1e-3f) && near(data.At(0, 0), -1.f, 1e-3f), "height = pixel * scale + offset");
	check(near(data.rangeMin, -1.f, 1e-5f) && near(data.rangeMax, 50.f, 1e-5f), "range is what the settings can reach");

	// Grid points are exact; between them, the 12-21 diagonal decides.
	check(near(data.HeightAt(64.f, 64.f), data.At(32, 32), 1e-4f), "HeightAt on a grid point");
	{
		const f32 h11 = data.At(10, 20), h12 = data.At(11, 20), h21 = data.At(10, 21), h22 = data.At(11, 21);
		const f32 x = 10 * 2.f, z = 20 * 2.f;
		check(near(data.HeightAt(x + 0.5f, z + 0.5f), h11 + (h12 - h11) * 0.25f + (h21 - h11) * 0.25f, 1e-4f),
			"below the diagonal, triangle (11, 21, 12)");
		check(near(data.HeightAt(x + 1.5f, z + 1.5f), h22 + (h21 - h22) * 0.25f + (h12 - h22) * 0.25f, 1e-4f),
			"above it, triangle (22, 12, 21)");
	}
	check(near(data.HeightAt(-50.f, -50.f), data.At(0, 0), 1e-4f), "outside the tile clamps to the edge");
	const Vec3 up = data.NormalAt(32, 32);
	check(near(up.y, 1.f, 1e-3f), "the hilltop's normal points up");

	// Levels.
	HeightfieldMesh m1, m4, m3;
	HeightfieldMesh::Build(data, 1, 3.f, m1);
	HeightfieldMesh::Build(data, 4, 3.f, m4);
	HeightfieldMesh::Build(data, 3, 3.f, m3);
	check(m1.vertex.size() == 65 * 65 + 4 * 65 && m1.index.size() == 64 * 64 * 6 + 4 * 64 * 6, "step 1: every point, plus skirts");
	check(m4.vertex.size() == 17 * 17 + 4 * 17, "step 4: every fourth point");
	check(m3.vertex.size() == 33 * 33 + 4 * 33, "step 3 rounds down to 2, which divides the grid");
	check(near(m4.vertex[16].x, 128.f, 1e-4f), "the last column lands on the tile's edge");
	bool skirtBelow = true;
	for (size_t i = 65 * 65; i < m1.vertex.size(); i++)
		if (!near(m1.vertex[i].y + 3.f, data.HeightAt(m1.vertex[i].x, m1.vertex[i].z), 1e-3f)) skirtBelow = false;
	check(skirtBelow, "skirt points hang 3 m below the edge");
	bool facesUp = true;
	for (size_t i = 0; i + 2 < 64 * 64 * 6; i += 3)
	{
		const Vec3 &a = m1.vertex[m1.index[i]], &b = m1.vertex[m1.index[i + 1]], &c = m1.vertex[m1.index[i + 2]];
		if ((b - a).cross(c - a).y <= 0.f) facesUp = false;
	}
	check(facesUp, "every surface triangle is wound to face up");
	check(m1.tangent.size() == m1.vertex.size() && near(m1.tangent[0].x, 1.f, 0.2f), "tangents run along +x");

	// Physics: a tile inside a cell at (1000, 0, -500). The ball starts
	// above the hilltop - flat, so it stays put - at the tile's local
	// (64, 64), and must come to rest on the height drawn there, in world
	// space.
	Box3DPhysics physics;
	physics.InitPhysics();
	SceneGraph scene;
	std::shared_ptr<HeightfieldData> shared = std::make_shared<HeightfieldData>(data);
	std::shared_ptr<GameObject> cell = std::make_shared<GameObject>();
	cell->SetPosition(Vec3(1000.f, 0.f, -500.f));
	std::shared_ptr<GameObject> tile = std::make_shared<GameObject>();
	tile->AddComponent(physics.CreateHeightField(shared));
	cell->Add(tile);
	scene.Add(cell);

	const f32 radius = 0.5f;
	const f32 ground = data.HeightAt(64.f, 64.f);
	std::shared_ptr<GameObject> ball = std::make_shared<GameObject>();
	ball->SetPosition(Vec3(1064.f, ground + 5.f, -436.f));
	ball->AddComponent(physics.CreateSphere(radius, 1.f));
	scene.Add(ball);

	f64 t = 0.0;
	for (int i = 0; i < 240; i++)
	{
		t += 1.0 / 60.0;
		physics.Update(1.0 / 60.0, 1);
		scene.Update(t);
	}
	const Vec3 p = ball->GetPosition();
	printf("ball at (%.3f, %.3f, %.3f), terrain %.3f\n", p.x, p.y, p.z, ground);
	check(near(p.y, ground + radius, 0.15f), "the ball rests on the terrain, in world space");
	check(near(p.x, 1064.f, 0.5f) && near(p.z, -436.f, 0.5f), "and where it was dropped, over the tile - not at the origin");

	remove(png.c_str());
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
