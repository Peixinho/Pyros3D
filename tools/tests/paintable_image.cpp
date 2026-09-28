// PaintableImage: brush falloff, painting toward a target, splat weights
// that stay normalised, and PNG round trips - 8-bit and the 16-bit greyscale
// heightmaps are saved as. No render device.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/paintable_image.cpp -o /tmp/paintable_image \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/paintable_image
#include <filesystem>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>

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

int main()
{
	check(PaintableImage::Falloff(0.f, 0.f) == 1.f && PaintableImage::Falloff(1.f, 0.f) == 0.f
		&& PaintableImage::Falloff(0.4f, 0.5f) == 1.f && PaintableImage::Falloff(0.75f, 0.5f) < 1.f,
		"falloff: full inside the hard core, zero at the rim");

	// Density map: paint a hole.
	PaintableImage d;
	d.Create(65, 65, 1, 255);
	check(d.Paint(0.5f, 0.5f, 0.2f, 0.2f, 0, 0.f, 1.f, 0.6f, false), "painting changes pixels");
	check(d.Sample(0.5f, 0.5f, 0) < 0.01f && d.Sample(0.05f, 0.05f, 0) > 0.99f, "a hole at the centre, untouched outside");
	// A soft rim keeps moving with every dab, as a brush should; a hard
	// full-strength one reaches its target in one and then stays put.
	d.Paint(0.2f, 0.2f, 0.1f, 0.1f, 0, 0.f, 1.f, 1.f, false);
	check(!d.Paint(0.2f, 0.2f, 0.1f, 0.1f, 0, 0.f, 1.f, 1.f, false), "a hard full-strength dab repeated changes nothing");

	// Splat: all on channel 0, paint channel 2; every pixel still sums to 255.
	PaintableImage s;
	s.Create(33, 33, 4, 0);
	for (size_t i = 0; i < s.pixels.size(); i += 4) { s.pixels[i] = 200; s.pixels[i + 1] = 55; }
	s.Paint(0.5f, 0.5f, 0.3f, 0.3f, 2, 1.f, 0.7f, 0.3f, true);
	bool sums = true;
	for (size_t i = 0; i < s.pixels.size(); i += 4)
		if (s.pixels[i] + s.pixels[i + 1] + s.pixels[i + 2] + s.pixels[i + 3] != 255) sums = false;
	check(sums, "splat weights still sum to one after painting");
	const uchar* c = &s.pixels[(16 * 33 + 16) * 4];
	check(c[2] > 150 && c[0] > c[1], "the painted layer took over; the others kept their ratio");

	// PNG round trips.
	const std::string p8 = (std::filesystem::temp_directory_path() / "pyros_paint_test.png").string();
	check(s.Save(p8), "8-bit save");
	PaintableImage back;
	check(back.Load(p8, 4) && back.pixels == s.pixels, "8-bit round trip is exact");
	remove(p8.c_str());

	std::vector<uint16> h(17 * 17);
	for (size_t i = 0; i < h.size(); i++) h[i] = (uint16)(i * 239 % 65536);
	const std::string p16 = (std::filesystem::temp_directory_path() / "pyros_paint_test16.png").string();
	check(PaintableImage::WritePNG16(p16, 17, 17, &h[0]), "16-bit save");
	HeightfieldData hd;
	check(HeightfieldData::LoadFile(p16, 16.f, 65535.f, 0.f, hd), "loads as a heightmap");
	bool exact = hd.samples == 17;
	for (size_t i = 0; exact && i < h.size(); i++) exact = std::fabs(hd.heights[i] - (f32)h[i]) < 0.01f;
	check(exact, "16-bit round trip is exact");
	remove(p16.c_str());

	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
