// Foliage generation: deterministic, filtered by slope, height and density
// map, standing on the ground, and even in any prefix (which is what makes
// distance density a plain instance count). No render device.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/foliage.cpp -o /tmp/foliage \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/foliage
#include <filesystem>
#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
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

static size_t Count(const PreparedFoliageLayer &l)
{
	size_t n = 0;
	for (size_t b = 0; b < l.blocks.size(); b++) n += l.blocks[b].transforms.size();
	return n;
}

int main()
{
	// 128 m tile: flat at 10 m on the west half, a 45-degree ramp on the east.
	HeightfieldData ground;
	ground.samples = 129;
	ground.size = 128.f;
	ground.heights.resize(129 * 129);
	for (int r = 0; r < 129; r++)
		for (int c = 0; c < 129; c++)
			ground.heights[r * 129 + c] = c <= 64 ? 10.f : 10.f + (c - 64) * 1.f;
	ground.minHeight = 10.f; ground.maxHeight = 74.f;

	FoliageLayerSpec spec;
	spec.density = 0.5f;
	spec.blockSize = 32.f;
	spec.maxSlopeDegrees = 30.f;
	spec.seed = 7;

	PreparedFoliageLayer a, b;
	PreparedFoliageLayer::Generate(ground, spec, "", a);
	PreparedFoliageLayer::Generate(ground, spec, "", b);
	bool same = a.blocks.size() == b.blocks.size();
	for (size_t i = 0; same && i < a.blocks.size(); i++)
		same = a.blocks[i].transforms.size() == b.blocks[i].transforms.size()
			&& memcmp(&a.blocks[i].transforms[0], &b.blocks[i].transforms[0], a.blocks[i].transforms.size() * sizeof(Matrix)) == 0;
	check(same, "the same tile and settings grow the same field");

	// Only the flat half (64 x 128 m) passes a 30 degree limit: ~0.5 * 8192.
	const size_t n = Count(a);
	printf("instances: %zu\n", n);
	check(n > 3700 && n < 4500, "steep ground grows nothing, flat ground grows at the density asked");

	bool onGround = true, onFlat = true;
	for (size_t bi = 0; bi < a.blocks.size(); bi++)
		for (size_t i = 0; i < a.blocks[bi].transforms.size(); i++)
		{
			const Matrix &m = a.blocks[bi].transforms[i];
			const Vec3 p = a.blocks[bi].origin + Vec3(m.m[12], m.m[13], m.m[14]);
			if (std::fabs(p.y + spec.sink - ground.HeightAt(p.x, p.z)) > 1e-3f) onGround = false;
			if (p.x > 64.5f && onFlat) { printf("on the ramp: (%.2f, %.2f, %.2f)\n", p.x, p.y, p.z); onFlat = false; }
		}
	check(onGround, "every instance stands on the ground (less the sink)");
	check(onFlat, "none on the ramp");

	spec.maxHeight = 5.f;
	PreparedFoliageLayer none;
	PreparedFoliageLayer::Generate(ground, spec, "", none);
	check(Count(none) == 0, "a height band below the ground grows nothing");
	spec.maxHeight = 1e9f;

	// Density map: left half white, right half black, over the whole tile.
	{
		std::vector<unsigned char> px(64 * 64);
		for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) px[y * 64 + x] = x < 16 ? 255 : 0;
		const std::string path = (std::filesystem::temp_directory_path() / "pyros_foliage_density.png").string();
		stbi_write_png(path.c_str(), 64, 64, 1, px.data(), 64);
		PreparedFoliageLayer masked;
		PreparedFoliageLayer::Generate(ground, spec, path, masked);
		bool left = true;
		for (size_t bi = 0; bi < masked.blocks.size(); bi++)
			for (size_t i = 0; i < masked.blocks[bi].transforms.size(); i++)
				if (masked.blocks[bi].origin.x + masked.blocks[bi].transforms[i].m[12] > 33.f) left = false;
		check(Count(masked) > 1500 && left, "a density map keeps only its painted quarter");
		remove(path.c_str());
	}

	// Prefix evenness: the first quarter of a block covers all its quadrants.
	{
		const FoliageBlock &blk = a.blocks[0];
		const size_t q = blk.transforms.size() / 4;
		int quad[4] = { 0, 0, 0, 0 };
		for (size_t i = 0; i < q; i++)
		{
			const Matrix &m = blk.transforms[i];
			quad[(m.m[12] > 0.f ? 1 : 0) + (m.m[14] > 0.f ? 2 : 0)]++;
		}
		const int lo = std::min(std::min(quad[0], quad[1]), std::min(quad[2], quad[3]));
		const int hi = std::max(std::max(quad[0], quad[1]), std::max(quad[2], quad[3]));
		printf("prefix quadrants: %d %d %d %d\n", quad[0], quad[1], quad[2], quad[3]);
		check(lo > 0 && hi < lo * 2, "a quarter of a block is an even quarter of it");
	}

	// Distance density.
	spec.fullDistance = 50.f; spec.fadeDistance = 100.f;
	check(FoliageComponent::DensityAt(spec, 10.f) == 1.f && FoliageComponent::DensityAt(spec, 100.f) == 0.f
		&& FoliageComponent::DensityAt(spec, 75.f) > 0.5f && FoliageComponent::DensityAt(spec, 75.f) < 1.f,
		"full near, gone at fadeDistance, most of it held in between");

	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
