// Checks the .p3ds sprite sheet: cutting, clip timing, atlas import, the file
// round-trip and the quads a sprite is built from. Standalone on purpose - the
// repo has no test framework (see prefab_roundtrip.cpp). Build and run it
// against an existing engine build:
//
//   c++ -std=c++17 -I include tools/tests/spritesheet2d.cpp -o /tmp/spritesheet2d \
//       -L build_vulkan -lPyrosEngine -Wl,-rpath,$PWD/build_vulkan && /tmp/spritesheet2d
//
// No window and no render device: SpriteSheet2D is arithmetic over an image
// size, and BuildSpriteMesh2D fills plain vectors.
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	if (!cond) failures++;
}
static void nearly(const f32 got, const f32 want, const std::string &what)
{
	const bool ok = std::fabs(got - want) < 1e-5f;
	printf("%s  %s (got %.6f, want %.6f)\n", ok ? "PASS" : "FAIL",
		what.c_str(), (double)got, (double)want);
	if (!ok) failures++;
}

int main()
{
	// --- grid slicing ------------------------------------------------------
	{
		SpriteSheet2D s;
		check(s.SliceGrid(16, 16) == 0, "slicing before the image size is known makes nothing");
		s.SetImageSize(64, 32);
		check(s.SliceGrid(16, 16) == 8, "64x32 in 16px cells is 8 frames");
		check(s.frames[5].x == 16 && s.frames[5].y == 16, "frame 5 is column 1 of row 1");
		check(s.frames[5].name == "frame5", "frames are named by index");
		check(s.FindFrame("frame7") == 7 && s.FindFrame("nope") == -1, "frames are found by name");

		const Vec4 uv = s.UVRect(5);
		nearly(uv.x, 16.5f / 64.f, "uv left is half a texel inside the cell");
		nearly(uv.y, 16.5f / 32.f, "uv top is half a texel inside the cell");
		nearly(uv.z, 31.5f / 64.f, "uv right is half a texel inside the cell");
		nearly(uv.w, 31.5f / 32.f, "uv bottom is half a texel inside the cell");
		check(s.UVRect(8).z == 0.f, "a frame out of range is a zero rect, not a clamped one");

		// margin + spacing: 2 + 3*(10+1) - 1 + 2 = 36 wide holds exactly 3.
		SpriteSheet2D m;
		m.SetImageSize(36, 14);
		check(m.SliceGrid(10, 10, 2, 1) == 3, "margin and spacing: a flush last cell is kept");
		check(m.frames[2].x == 24 && m.frames[2].y == 2, "margin and spacing place the third cell");
		m.SetImageSize(35, 14);
		check(m.SliceGrid(10, 10, 2, 1) == 2, "one pixel short drops the last column");

		SpriteSheet2D c;
		c.SetImageSize(96, 64);
		check(c.SliceCount(6, 4) == 24 && c.frames[0].w == 16 && c.frames[0].h == 16, "slicing by count");
	}

	// --- clips --------------------------------------------------------------
	{
		SpriteSheet2D s;
		s.SetImageSize(64, 16);
		s.SliceGrid(16, 16);
		const int32 ci = s.AddClipRange("run", 0, 3, 10.f, true);
		check(ci == 0 && s.clips[0].frames.size() == 4, "a clip over frames 0..3");
		check(s.AddClipRange("bad", 0, 9) == -1, "a clip outside the sheet is refused");
		check(s.AddClipRange("back", 3, 1) == 1 && s.clips[1].frames[0] == 3 && s.clips[1].frames[2] == 1,
			"a range given backwards plays backwards");

		const SpriteClip2D &run = s.clips[0];
		nearly(SpriteClip2DLength(s, run), 0.4f, "4 frames at 10 fps last 0.4s");
		check(SpriteClip2DAt(s, run, 0.f).frame == 0, "t=0 is the first frame");
		check(SpriteClip2DAt(s, run, 0.25f).frame == 2, "t=0.25 is frame 2");
		check(SpriteClip2DAt(s, run, 0.45f).frame == 0, "a looping clip wraps");
		check(!SpriteClip2DAt(s, run, 5.f).finished, "a looping clip never finishes");

		SpriteClip2D once = run;
		once.loop = false;
		const SpriteClipCursor2D end = SpriteClip2DAt(s, once, 0.4f);
		check(end.finished && end.frame == 3, "a one-shot clip ends on its last frame");
		check(!SpriteClip2DAt(s, once, 0.39f).finished, "and is not finished a moment before");

		SpriteClip2D pp = run;
		pp.pingPong = true;
		nearly(SpriteClip2DLength(s, pp), 0.6f, "ping-pong of 4 is 6 steps: the ends are not doubled");
		check(SpriteClip2DAt(s, pp, 0.35f).frame == 3, "ping-pong reaches the end");
		check(SpriteClip2DAt(s, pp, 0.45f).frame == 2, "ping-pong comes back");
		check(SpriteClip2DAt(s, pp, 0.55f).frame == 1, "ping-pong comes back further");
		check(SpriteClip2DAt(s, pp, 0.65f).frame == 0, "ping-pong starts again");

		// A held frame: frame 1 lasts half a second whatever the rate says.
		s.frames[1].duration = 0.5f;
		nearly(SpriteClip2DLength(s, run), 0.8f, "a frame's own duration replaces 1/fps");
		check(SpriteClip2DAt(s, run, 0.55f).frame == 1, "the held frame is still showing at 0.55s");
		check(SpriteClip2DAt(s, run, 0.65f).frame == 2, "and has passed by 0.65s");

		SpriteClip2D empty;
		check(SpriteClip2DAt(s, empty, 1.f).frame == -1, "an empty clip has no frame");
	}

	// --- cutting by content -----------------------------------------------------
	{
		// 12x8, three islands: a 3x3 at (1,1), a 2x4 at (6,0) with a detached
		// pixel one away from it, and a 2x2 on a second row at (2,6).
		const int w = 12, h = 8;
		std::vector<uchar> px((size_t)w * h * 4, 0);
		auto put = [&](int x, int y) { px[((size_t)y * w + x) * 4 + 3] = 255; };
		for (int y = 1; y <= 3; y++) for (int x = 1; x <= 3; x++) put(x, y);
		for (int y = 0; y <= 3; y++) for (int x = 6; x <= 7; x++) put(x, y);
		put(9, 1);
		for (int y = 6; y <= 7; y++) for (int x = 2; x <= 3; x++) put(x, y);

		std::vector<SpriteRect2D> r = SpriteSheet2DAutoSlice(&px[0], w, h, 8, 1, 2);
		check(r.size() == 3, "three sprites: the stray pixel joins its neighbour");
		if (r.size() == 3)
		{
			check(r[0].x == 1 && r[0].y == 1 && r[0].w == 3 && r[0].h == 3, "first in reading order is the left one of the top row");
			check(r[1].x == 6 && r[1].y == 0 && r[1].w == 4 && r[1].h == 4, "second swallowed the detached pixel");
			check(r[2].x == 2 && r[2].y == 6 && r[2].w == 2 && r[2].h == 2, "third is the second row");
		}
		r = SpriteSheet2DAutoSlice(&px[0], w, h, 8, 0, 2);
		check(r.size() == 3, "with no merge reach the lone pixel is dropped as too small");
		r = SpriteSheet2DAutoSlice(&px[0], w, h, 8, 0, 1);
		check(r.size() == 4, "and kept when single pixels are allowed");
	}

	// --- other tools' atlases ---------------------------------------------------
	{
		// Aseprite JSON hash. "walk 10" must stay AFTER "walk 2": the tags
		// refer to frames by position.
		std::string a = "{ \"frames\": {";
		for (int i = 0; i < 11; i++)
		{
			a += "\"walk " + std::to_string(i) + ".aseprite\": {\"frame\":{\"x\":" + std::to_string(i * 8)
				+ ",\"y\":0,\"w\":8,\"h\":8},\"rotated\":false,\"trimmed\":false,"
				"\"spriteSourceSize\":{\"x\":0,\"y\":0,\"w\":8,\"h\":8},\"sourceSize\":{\"w\":8,\"h\":8},\"duration\":100}";
			if (i < 10) a += ",";
		}
		a += "}, \"meta\": {\"image\":\"walk.png\",\"size\":{\"w\":88,\"h\":8},"
			"\"frameTags\":[{\"name\":\"step\",\"from\":9,\"to\":10,\"direction\":\"forward\"},"
			"{\"name\":\"back\",\"from\":0,\"to\":2,\"direction\":\"reverse\"}]}}";
		SpriteSheet2D s;
		std::string err;
		check(ImportSpriteAtlasJson(a, s, &err), "an Aseprite JSON hash imports (" + err + ")");
		check(s.frames.size() == 11, "eleven frames");
		check(s.frames.size() == 11 && s.frames[2].name == "walk 2" && s.frames[2].x == 16, "frame order is the file's, not alphabetical");
		check(s.frames.size() == 11 && s.frames[10].name == "walk 10" && s.frames[10].x == 80, "'walk 10' is last");
		check(s.image == "walk.png" && s.GetImageWidth() == 88, "image and size come from meta");
		check(s.clips.size() == 2 && s.clips[0].name == "step" && s.clips[0].frames.size() == 2 && s.clips[0].frames[1] == 10, "frame tags become clips");
		check(s.clips.size() == 2 && s.clips[1].frames[0] == 2 && s.clips[1].frames[2] == 0, "a reverse tag plays backwards");
		nearly(s.frames.size() == 11 ? s.frames[0].duration : 0.f, 0.1f, "durations are seconds");
		nearly(s.clips.empty() ? 0.f : s.clips[0].fps, 10.f, "a tag's rate follows its frames' durations");
		check(s.frames.size() == 11 && s.frames[0].sourceW == 0, "an untrimmed frame carries no trim");

		// TexturePacker JSON array, trimmed, with a pivot.
		const std::string t = "{\"frames\":[{\"filename\":\"hero/idle.png\",\"frame\":{\"x\":2,\"y\":2,\"w\":20,\"h\":30},"
			"\"rotated\":false,\"trimmed\":true,\"spriteSourceSize\":{\"x\":6,\"y\":2,\"w\":20,\"h\":30},"
			"\"sourceSize\":{\"w\":32,\"h\":32},\"pivot\":{\"x\":0.5,\"y\":1.0}}],"
			"\"meta\":{\"image\":\"atlas.png\",\"size\":{\"w\":64,\"h\":64}}}";
		SpriteSheet2D p;
		check(ImportSpriteAtlasJson(t, p, &err), "a TexturePacker JSON array imports (" + err + ")");
		check(p.frames.size() == 1 && p.frames[0].name == "hero/idle", "the extension is dropped from the name");
		check(p.frames.size() == 1 && p.frames[0].sourceW == 32 && p.frames[0].offsetX == 6 && p.frames[0].offsetY == 2, "trim is kept");
		nearly(p.frames.empty() ? -1.f : p.frames[0].pivot.y, 0.f, "a packer's bottom pivot (y=1, measured down) is y=0 up");

		const std::string rot = "{\"frames\":{\"a\":{\"frame\":{\"x\":0,\"y\":0,\"w\":4,\"h\":8},\"rotated\":true}}}";
		check(!ImportSpriteAtlasJson(rot, p, &err) && err.find("rotated") != std::string::npos, "a rotated frame is refused by name");
		check(!ImportSpriteAtlasJson("{\"meta\":{}}", p, &err), "JSON with no frames is not an atlas");
		check(!ImportSpriteAtlasJson("not json", p, &err), "garbage is refused");
	}

	// --- the file ------------------------------------------------------------------
	{
		SpriteSheet2D s;
		s.image = "assets/textures/hero.png";
		s.pixelsPerUnit = 32.f;
		s.filter = SpriteFilter2D::Nearest;
		s.SetImageSize(64, 32);
		s.SliceGrid(16, 16, 0, 0, "hero");
		s.frames[1].pivot = Vec2(0.5f, 0.f);
		s.frames[2].border = Vec4(3.f, 4.f, 5.f, 6.f);
		s.frames[3].sourceW = 20; s.frames[3].sourceH = 24; s.frames[3].offsetX = 2; s.frames[3].offsetY = 4;
		s.frames[4].duration = 0.25f;
		s.AddClipRange("run", 0, 3, 14.f, true);
		s.clips[0].pingPong = true;
		SpriteEvent2D ev; ev.at = 2; ev.name = "step";
		s.clips[0].events.push_back(ev);

		SpriteSheet2D b;
		std::string err;
		check(SpriteSheet2DFromString(SpriteSheet2DToString(s), b, &err), "a sheet round-trips (" + err + ")");
		check(b.image == s.image && b.pixelsPerUnit == 32.f && b.filter == SpriteFilter2D::Nearest, "image, scale and filter survive");
		check(b.frames.size() == 8 && b.frames[5].name == "hero5" && b.frames[5].x == 16 && b.frames[5].y == 16, "frames survive");
		check(b.frames.size() == 8 && b.frames[1].pivot.y == 0.f && b.frames[0].pivot.y == 0.5f, "a pivot survives, and the default stays the default");
		check(b.frames.size() == 8 && b.frames[2].border.x == 3.f && b.frames[2].border.w == 6.f, "borders survive");
		check(b.frames.size() == 8 && b.frames[3].sourceW == 20 && b.frames[3].offsetY == 4, "trim survives");
		check(b.frames.size() == 8 && b.frames[4].duration == 0.25f, "a frame duration survives");
		check(b.clips.size() == 1 && b.clips[0].name == "run" && b.clips[0].fps == 14.f && b.clips[0].pingPong, "clips survive");
		check(b.clips.size() == 1 && b.clips[0].events.size() == 1 && b.clips[0].events[0].name == "step" && b.clips[0].events[0].at == 2, "events survive");

		check(!SpriteSheet2DFromString("{\"pixelsPerUnit\":0}", b, &err), "pixelsPerUnit 0 is refused");
		check(!SpriteSheet2DFromString("{\"frames\":[{\"name\":\"a\",\"rect\":[0,0,0,4]}]}", b, &err), "a frame with no area is refused");
		check(!SpriteSheet2DFromString("{\"frames\":[{\"rect\":[0,0,4,4]}],\"clips\":[{\"name\":\"c\",\"frames\":[3]}]}", b, &err)
			&& err.find("frame 3") != std::string::npos, "a clip using a frame the sheet lacks is refused by name");
		check(!SpriteSheet2DFromString("[1,2]", b, &err), "a non-object is refused");
		check(!SpriteSheet2DFromString("{\"frames\":[{\"rect\":[\"x\",0,4,4]}]}", b, &err), "a wrongly typed field is refused, not thrown");
	}

	// --- quads ------------------------------------------------------------------------
	{
		SpriteSheet2D s;
		s.pixelsPerUnit = 16.f;
		s.SetImageSize(64, 32);
		s.SliceGrid(32, 32);             // two 32x32 frames: 2x2 world units each

		SpriteMeshParams2D p;
		SpriteMesh2D m;
		check(BuildSpriteMesh2D(s, p, m) && m.vertex.size() == 4 && m.index.size() == 6, "a simple sprite is one quad");
		nearly(m.vertex[0].x, -1.f, "centred pivot: left edge at -1");
		nearly(m.vertex[2].x, 1.f, "centred pivot: right edge at +1");
		nearly(m.vertex[0].y, -1.f, "centred pivot: bottom edge at -1");
		nearly(m.texcoord[0].y, 31.5f / 32.f, "the bottom vertices take the frame's bottom v");
		nearly(m.texcoord[2].y, 0.5f / 32.f, "the top vertices take the frame's top v");
		check(m.color.size() == 4 && m.color[0].w == 1.f, "vertices carry the colour");

		// Feet pivot.
		s.frames[0].pivot = Vec2(0.5f, 0.f);
		m.Clear();
		BuildSpriteMesh2D(s, p, m);
		nearly(m.vertex[0].y, 0.f, "bottom pivot: the quad stands on the origin");
		nearly(m.vertex[2].y, 2.f, "bottom pivot: and is 2 units tall");
		const Vec4 rect = SpriteLocalRect2D(s, p);
		nearly(rect.y, 0.f, "the local rect agrees with the quad (bottom)");
		nearly(rect.w, 2.f, "the local rect agrees with the quad (top)");

		// Flip: mirrored about the pivot, picture reversed.
		s.frames[0].pivot = Vec2(0.25f, 0.f);
		m.Clear();
		BuildSpriteMesh2D(s, p, m);
		const f32 unflippedLeft = m.vertex[0].x, unflippedU = m.texcoord[0].x;
		nearly(unflippedLeft, -0.5f, "pivot a quarter in: left edge at -0.5");
		p.flipX = true;
		m.Clear();
		BuildSpriteMesh2D(s, p, m);
		nearly(m.vertex[0].x, 0.5f, "flipped: the same vertex is now at +0.5");
		nearly(m.texcoord[0].x, unflippedU, "and still samples the same texel, so the picture mirrors");
		const Vec4 fr = SpriteLocalRect2D(s, p);
		nearly(fr.x, -1.5f, "flipped local rect reaches -1.5");
		nearly(fr.z, 0.5f, "flipped local rect reaches +0.5");
		p.flipX = false;
		s.frames[0].pivot = Vec2(0.5f, 0.5f);

		// Trim: a 16x16 kept rect inside a 32x32 source, 8 in from the left
		// and 4 down from the top.
		SpriteSheet2D t;
		t.pixelsPerUnit = 16.f;
		t.SetImageSize(64, 64);
		SpriteFrame2D f;
		f.name = "t"; f.x = 0; f.y = 0; f.w = 16; f.h = 16;
		f.sourceW = 32; f.sourceH = 32; f.offsetX = 8; f.offsetY = 4;
		t.frames.push_back(f);
		m.Clear();
		SpriteMeshParams2D tp;
		BuildSpriteMesh2D(t, tp, m);
		nearly(m.vertex[0].x, -0.5f, "trimmed: left edge is the source's left plus the trim");
		nearly(m.vertex[2].x, 0.5f, "trimmed: 16px wide");
		nearly(m.vertex[2].y, 0.75f, "trimmed: top edge is the source's top minus 4px");
		nearly(m.vertex[0].y, -0.25f, "trimmed: 16px tall");
		const Vec4 tr = SpriteLocalRect2D(t, tp);
		nearly(tr.z - tr.x, 2.f, "the local rect is the SOURCE size, so it does not change per frame");

		// Nine-slice: 32x32 frame, 8px borders, drawn 10x4 units.
		s.frames[1].border = Vec4(8.f, 8.f, 8.f, 8.f);
		SpriteMeshParams2D sp;
		sp.frame = 1;
		sp.mode = SpriteDraw2D::Sliced;
		sp.size = Vec2(10.f, 4.f);
		m.Clear();
		check(BuildSpriteMesh2D(s, sp, m) && m.QuadCount() == 9, "a sliced sprite is nine quads");
		nearly(m.vertex[0].x, -5.f, "sliced: left edge");
		nearly(m.vertex[1].x, -4.5f, "sliced: the corner keeps its 8px = 0.5 unit width");
		nearly(m.minBounds.x, -5.f, "sliced bounds (min x)");
		nearly(m.maxBounds.x, 5.f, "sliced bounds (max x)");
		nearly(m.maxBounds.y, 2.f, "sliced bounds (max y)");
		// Smaller than its own corners: they shrink and do not cross.
		sp.size = Vec2(0.5f, 4.f);
		m.Clear();
		BuildSpriteMesh2D(s, sp, m);
		bool crossed = false;
		for (size_t i = 0; i + 3 < m.vertex.size(); i += 4)
			if (m.vertex[i + 1].x < m.vertex[i].x - 1e-6f) crossed = true;
		check(!crossed && m.maxBounds.x <= 0.25f + 1e-5f, "drawn narrower than its corners, they shrink instead of crossing");

		// Tiled: a 2x2 unit frame over 5x2 is two whole and a half.
		SpriteMeshParams2D ti;
		ti.mode = SpriteDraw2D::Tiled;
		ti.size = Vec2(5.f, 2.f);
		m.Clear();
		check(BuildSpriteMesh2D(s, ti, m) && m.QuadCount() == 3, "tiled 5 wide with a 2 wide frame is three quads");
		nearly(m.vertex[8 + 1].x - m.vertex[8].x, 1.f, "the last one is cut to half width");
		const f32 fullU = m.texcoord[1].x - m.texcoord[0].x, cutU = m.texcoord[8 + 1].x - m.texcoord[8].x;
		nearly(cutU, fullU * 0.5f, "and samples half the frame");
		ti.size = Vec2(100000.f, 100000.f);
		m.Clear();
		check(BuildSpriteMesh2D(s, ti, m) && m.QuadCount() == 1, "an absurd tiled size falls back to one quad");

		// Appending with a transform: what a batch does.
		m.Clear();
		Matrix move;
		move.Translate(Vec3(10.f, 0.f, 0.f));
		SpriteMeshParams2D a;
		BuildSpriteMesh2D(s, a, m);
		BuildSpriteMesh2D(s, a, m, &move);
		check(m.QuadCount() == 2 && m.index[6] == 4, "a second sprite appends, indices continuing");
		nearly(m.vertex[4].x, 9.f, "and is placed by its transform");
		nearly(m.maxBounds.x, 11.f, "bounds grow to hold both");

		SpriteMeshParams2D bad;
		bad.frame = 9;
		const size_t before = m.vertex.size();
		check(!BuildSpriteMesh2D(s, bad, m) && m.vertex.size() == before, "a frame out of range appends nothing");
	}

	// --- how far inside a frame its texture coordinates stop -----------------
	{
		SpriteSheet2D es;
		es.SetImageSize(64, 32);
		SpriteFrame2D f; f.x = 16; f.y = 0; f.w = 16; f.h = 16;
		es.frames.push_back(f);
		es.filter = SpriteFilter2D::Linear;
		nearly(es.UVRect(0).x, 16.5f / 64.f, "smoothly filtered, a frame stops half a texel inside itself");
		es.filter = SpriteFilter2D::Nearest;
		nearly(es.UVRect(0).x, 16.02f / 64.f, "nearest filtered, it keeps its edge texels whole");
		nearly(es.UVRect(0).z, 31.98f / 64.f, "on the far side too");
		es.edgeInset = 0.25f;
		nearly(es.UVRect(0).x, 16.25f / 64.f, "a sheet can say its own inset");
		SpriteSheet2D back;
		check(SpriteSheet2DFromString(SpriteSheet2DToString(es), back) && back.edgeInset == 0.25f, "and it survives a save");
		es.edgeInset = -1.f;
		check(SpriteSheet2DToString(es).find("edgeInset") == std::string::npos, "one left to the filter is not written");
		es.frames[0].w = 1; es.edgeInset = 4.f;
		nearly(es.UVRect(0).x, 16.5f / 64.f, "an inset bigger than the frame stops at its middle");
	}

	// --- packing loose images into an atlas ---------------------------------
	{
		// Three images: a solid red 10x6, a 12x12 with a 4x4 green block in
		// its middle (the rest clear), and a fully clear 5x5.
		std::vector<SpriteAtlasImage2D> ims(3);
		ims[0].name = "red"; ims[0].w = 10; ims[0].h = 6; ims[0].rgba.assign(10 * 6 * 4, 0);
		for (int i = 0; i < 10 * 6; i++) { ims[0].rgba[i * 4] = 255; ims[0].rgba[i * 4 + 3] = 255; }
		ims[1].name = "dot"; ims[1].w = 12; ims[1].h = 12; ims[1].rgba.assign(12 * 12 * 4, 0);
		for (int y = 4; y < 8; y++) for (int x = 3; x < 7; x++) { ims[1].rgba[(y * 12 + x) * 4 + 1] = 255; ims[1].rgba[(y * 12 + x) * 4 + 3] = 255; }
		ims[2].name = "empty"; ims[2].w = 5; ims[2].h = 5; ims[2].rgba.assign(5 * 5 * 4, 0);

		SpriteAtlasOptions2D opt;
		std::vector<uchar> out;
		int32 W = 0, H = 0;
		std::vector<SpriteFrame2D> fr;
		std::string err;
		check(PackSpriteAtlas2D(ims, opt, out, W, H, fr, &err) && fr.size() == 3, "three images pack");
		check(W > 0 && H > 0 && (W & (W - 1)) == 0 && (H & (H - 1)) == 0, "into a power-of-two picture");
		check(fr[0].name == "red" && fr[1].name == "dot" && fr[2].name == "empty", "frames come back in the order given");
		check(fr[0].w == 10 && fr[0].h == 6 && fr[0].sourceW == 0, "a solid image is kept whole and is not marked trimmed");
		check(fr[1].w == 4 && fr[1].h == 4 && fr[1].sourceW == 12 && fr[1].sourceH == 12 && fr[1].offsetX == 3 && fr[1].offsetY == 4,
			"a trimmed one remembers its size and where the kept part sat");
		check(fr[2].w == 1 && fr[2].h == 1 && fr[2].sourceW == 5, "an empty image is still a frame, of one pixel");
		bool overlap = false;
		for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++)
			if (fr[a].x < fr[b].x + fr[b].w + opt.padding && fr[b].x < fr[a].x + fr[a].w + opt.padding
				&& fr[a].y < fr[b].y + fr[b].h + opt.padding && fr[b].y < fr[a].y + fr[a].h + opt.padding) overlap = true;
		check(!overlap, "no two frames are within the padding of each other");
		const uchar* r0 = &out[((size_t)fr[0].y * W + fr[0].x) * 4];
		const uchar* g0 = &out[((size_t)(fr[1].y + 3) * W + fr[1].x + 3) * 4];
		check(r0[0] == 255 && r0[3] == 255 && g0[1] == 255 && g0[3] == 255, "the pixels are where the frames say");
		const uchar* gap = &out[((size_t)fr[0].y * W + fr[0].x + fr[0].w) * 4];
		check(gap[3] == 0, "and the padding is clear");

		// The trimmed frame is drawn where the whole image would have been.
		SpriteSheet2D ps;
		ps.pixelsPerUnit = 1.f;
		ps.frames = fr;
		ps.SetImageSize(W, H);
		SpriteMeshParams2D pp;
		pp.frame = 1;
		SpriteMesh2D pm;
		check(BuildSpriteMesh2D(ps, pp, pm), "a packed, trimmed frame builds");
		// 12x12 source, centre pivot: the block spans x 3..7 and y (from the top) 4..8.
		nearly(pm.minBounds.x, -3.f, "its left edge is where the block was in the source");
		nearly(pm.maxBounds.x, 1.f, "its right edge too");
		nearly(pm.maxBounds.y, 2.f, "and its top");

		opt.extrude = 1;
		check(PackSpriteAtlas2D(ims, opt, out, W, H, fr, &err), "packs with extrusion");
		const uchar* ex = &out[((size_t)fr[0].y * W + fr[0].x - 1) * 4];
		const uchar* far2 = &out[((size_t)fr[0].y * W + fr[0].x - 2) * 4];
		check(ex[0] == 255 && ex[3] == 255 && far2[3] == 0, "extrusion repeats the edge one pixel out and no further");

		opt = SpriteAtlasOptions2D();
		opt.trim = false;
		opt.powerOfTwo = false;
		check(PackSpriteAtlas2D(ims, opt, out, W, H, fr, &err) && fr[1].w == 12 && fr[1].sourceW == 0, "untrimmed keeps every image whole");
		check(out.size() == (size_t)W * H * 4, "and a free-size picture is exactly as big as its pixels");

		opt.maxSize = 16;
		check(!PackSpriteAtlas2D(ims, opt, out, W, H, fr, &err) && !err.empty(), "too many pixels for the limit is refused with a message");
		std::vector<SpriteAtlasImage2D> none;
		check(!PackSpriteAtlas2D(none, SpriteAtlasOptions2D(), out, W, H, fr, &err), "nothing to pack is refused");

		// Many images of mixed sizes: all placed, none overlapping, all inside.
		std::vector<SpriteAtlasImage2D> many(60);
		for (int i = 0; i < 60; i++)
		{
			many[i].name = "m" + std::to_string(i);
			many[i].w = 5 + (i * 7) % 23; many[i].h = 4 + (i * 11) % 19;
			many[i].rgba.assign((size_t)many[i].w * many[i].h * 4, 255);
		}
		check(PackSpriteAtlas2D(many, SpriteAtlasOptions2D(), out, W, H, fr, &err) && fr.size() == 60, "sixty mixed images pack");
		bool bad = false;
		for (int a = 0; a < 60 && !bad; a++)
		{
			if (fr[a].x < 0 || fr[a].y < 0 || fr[a].x + fr[a].w > W || fr[a].y + fr[a].h > H) bad = true;
			for (int b = a + 1; b < 60 && !bad; b++)
				if (fr[a].x < fr[b].x + fr[b].w && fr[b].x < fr[a].x + fr[a].w && fr[a].y < fr[b].y + fr[b].h && fr[b].y < fr[a].y + fr[a].h) bad = true;
		}
		check(!bad, "every one inside the picture and none on another");
		long long used = 0;
		for (int a = 0; a < 60; a++) used += (long long)fr[a].w * fr[a].h;
		check(used * 100 / ((long long)W * H) >= 35, "and the picture is not mostly empty");

		// Clips from numbered names.
		SpriteSheet2D cs;
		const char* nm[] = { "walk_1", "walk_0", "walk_2", "idle", "jump-10", "jump-9", "7", "solo_1" };
		for (int i = 0; i < 8; i++) { SpriteFrame2D f; f.name = nm[i]; f.w = f.h = 4; cs.frames.push_back(f); }
		check(SpriteSheet2DClipsFromNames(cs, 10.f) == 2 && cs.clips.size() == 2, "numbered runs become clips; a lone frame and a bare number do not");
		check(cs.clips[0].name == "walk" && cs.clips[0].frames.size() == 3 && cs.clips[0].frames[0] == 1 && cs.clips[0].frames[2] == 2,
			"in number order, whatever order the frames are in");
		check(cs.clips[1].name == "jump" && cs.clips[1].frames[0] == 5 && cs.clips[1].frames[1] == 4, "9 before 10, not as text");
		check(SpriteSheet2DClipsFromNames(cs, 10.f) == 0, "asked again, nothing is added twice");
	}

	// --- masks: quads cut against an outline --------------------------------
	{
		struct A {
			static f32 Area(const SpriteMesh2D &m)
			{
				f32 a = 0.f;
				for (size_t i = 0; i + 2 < m.index.size(); i += 3)
				{
					const Vec3 &u = m.vertex[m.index[i]], &v = m.vertex[m.index[i + 1]], &w = m.vertex[m.index[i + 2]];
					a += fabsf((v.x - u.x) * (w.y - u.y) - (v.y - u.y) * (w.x - u.x)) * 0.5f;
				}
				return a;
			}
		};
		// A 2x2 sprite centred on the origin.
		SpriteSheet2D ms;
		ms.pixelsPerUnit = 8.f;
		ms.SetImageSize(16, 16);
		ms.SliceGrid(16, 16, 0, 0, "m");
		SpriteMeshParams2D mp;
		SpriteMesh2D m;
		BuildSpriteMesh2D(ms, mp, m);
		nearly(A::Area(m), 4.f, "the sprite to be cut is 2 by 2");
		const f32 uLeft = m.texcoord[0].x, uRight = m.texcoord[1].x;

		std::vector<Vec2> rightHalf;
		rightHalf.push_back(Vec2(0.f, -5.f)); rightHalf.push_back(Vec2(5.f, -5.f)); rightHalf.push_back(Vec2(5.f, 5.f)); rightHalf.push_back(Vec2(0.f, 5.f));
		SpriteMesh2D in = m;
		check(ClipSpriteMesh2D(in, 0, rightHalf, true), "a mask over the right half leaves something");
		nearly(A::Area(in), 2.f, "half of it");
		nearly(in.minBounds.x, 0.f, "from the mask's edge");
		nearly(in.maxBounds.x, 1.f, "to the sprite's");
		f32 uAtCut = -1.f;
		for (size_t i = 0; i < in.vertex.size(); i++) if (fabsf(in.vertex[i].x) < 1e-5f) uAtCut = in.texcoord[i].x;
		nearly(uAtCut, (uLeft + uRight) * 0.5f, "and the texture at the cut is the texture that was there");
		check(in.vertex.size() % 4 == 0 && in.index.size() == in.vertex.size() / 4 * 6 && in.tangent.size() == in.vertex.size(),
			"what is left is still quads, every attribute with it");

		SpriteMesh2D out = m;
		check(ClipSpriteMesh2D(out, 0, rightHalf, false), "the same mask, outside");
		nearly(A::Area(out), 2.f, "leaves the other half");
		nearly(out.maxBounds.x, 0.f, "up to the mask's edge");

		std::vector<Vec2> clockwise(rightHalf.rbegin(), rightHalf.rend());
		SpriteMesh2D cw = m;
		ClipSpriteMesh2D(cw, 0, clockwise, true);
		nearly(A::Area(cw), 2.f, "an outline given the other way round cuts the same");

		// A soft edge: the same cut, with a band inside the outline over
		// which what is kept fades. Nothing more or less is kept - only how
		// much of it shows.
		{
			struct W {
				// area weighted by alpha: exact, the fade being straight across each piece
				static f32 Shown(const SpriteMesh2D &g) {
					f32 total = 0.f;
					for (size_t i = 0; i + 2 < g.index.size(); i += 3)
					{
						const Vec3 &p0 = g.vertex[g.index[i]], &p1 = g.vertex[g.index[i + 1]], &p2 = g.vertex[g.index[i + 2]];
						const f32 area = fabsf((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y)) * 0.5f;
						total += area * (g.color[g.index[i]].w + g.color[g.index[i + 1]].w + g.color[g.index[i + 2]].w) / 3.f;
					}
					return total;
				}
			};
			std::vector<Vec2> square;
			square.push_back(Vec2(-0.5f, -0.5f)); square.push_back(Vec2(0.5f, -0.5f)); square.push_back(Vec2(0.5f, 0.5f)); square.push_back(Vec2(-0.5f, 0.5f));
			SpriteMesh2D soft = m;
			check(ClipSpriteMeshFeathered2D(soft, 0, square, true, 0.25f), "a mask with a soft edge leaves something");
			nearly(A::Area(soft), 1.f, "as much as the hard one");
			nearly(W::Shown(soft), 0.25f + 1.f / 3.f, "of which the middle is whole and the band fades");
			f32 atEdge = 1.f, atInner = 0.f;
			for (size_t i = 0; i < soft.vertex.size(); i++)
			{
				const f32 reach = std::max(fabsf(soft.vertex[i].x), fabsf(soft.vertex[i].y));
				if (fabsf(reach - 0.5f) < 1e-4f) atEdge = std::min(atEdge, soft.color[i].w), atEdge = std::max(0.f, atEdge);
				if (reach < 0.25f + 1e-4f) atInner = std::max(atInner, soft.color[i].w);
			}
			nearly(atEdge, 0.f, "clear at the outline");
			nearly(atInner, 1.f, "whole a feather in");
			check(soft.vertex.size() % 4 == 0 && soft.index.size() == soft.vertex.size() / 4 * 6 && soft.tangent.size() == soft.vertex.size(),
				"and it is still quads, every attribute with it");
			SpriteMesh2D softOut = m;
			check(ClipSpriteMeshFeathered2D(softOut, 0, square, false, 0.25f), "the same mask, outside");
			nearly(A::Area(softOut), 3.75f, "keeps the band as well as what is outside");
			nearly(W::Shown(softOut), 3.f + 0.75f - 1.f / 3.f, "fading the other way");
			SpriteMesh2D wide = m;
			check(ClipSpriteMeshFeathered2D(wide, 0, square, true, 5.f), "a feather wider than the shape is narrowed to fit");
			nearly(A::Area(wide), 1.f, "and cuts the same shape");
		}

		std::vector<Vec2> hole;       // a 1x1 square in the middle
		hole.push_back(Vec2(-0.5f, -0.5f)); hole.push_back(Vec2(0.5f, -0.5f)); hole.push_back(Vec2(0.5f, 0.5f)); hole.push_back(Vec2(-0.5f, 0.5f));
		SpriteMesh2D frame = m;
		ClipSpriteMesh2D(frame, 0, hole, false);
		nearly(A::Area(frame), 3.f, "outside a hole in the middle is the sprite less the hole");
		SpriteMesh2D plug = m;
		ClipSpriteMesh2D(plug, 0, hole, true);
		nearly(A::Area(plug), 1.f, "and inside it, the hole");

		std::vector<Vec2> ring;
		for (int i = 0; i < 32; i++) ring.push_back(Vec2(cosf((f32)i / 32.f * 6.2831853f) * 0.8f, sinf((f32)i / 32.f * 6.2831853f) * 0.8f));
		SpriteMesh2D disc = m;
		ClipSpriteMesh2D(disc, 0, ring, true);
		check(fabsf(A::Area(disc) - 3.14159265f * 0.64f) < 0.02f, "a round mask leaves a disc");

		std::vector<Vec2> away;
		away.push_back(Vec2(10.f, 10.f)); away.push_back(Vec2(11.f, 10.f)); away.push_back(Vec2(11.f, 11.f)); away.push_back(Vec2(10.f, 11.f));
		SpriteMesh2D gone = m;
		check(!ClipSpriteMesh2D(gone, 0, away, true) && gone.vertex.empty() && gone.index.empty(), "a mask nowhere near leaves nothing, and says so");
		SpriteMesh2D whole = m;
		ClipSpriteMesh2D(whole, 0, away, false);
		nearly(A::Area(whole), 4.f, "and outside one nowhere near, everything");

		// Several pieces: a shape cut from an image is many rectangles.
		std::vector<std::vector<Vec2> > two2;
		std::vector<Vec2> a1, a2;
		a1.push_back(Vec2(-1.f, -1.f)); a1.push_back(Vec2(-0.5f, -1.f)); a1.push_back(Vec2(-0.5f, 1.f)); a1.push_back(Vec2(-1.f, 1.f));
		a2.push_back(Vec2(0.5f, -1.f)); a2.push_back(Vec2(1.f, -1.f)); a2.push_back(Vec2(1.f, 1.f)); a2.push_back(Vec2(0.5f, 1.f));
		two2.push_back(a1); two2.push_back(a2);
		SpriteMesh2D bars = m;
		check(ClipSpriteMeshPieces2D(bars, 0, two2, true), "a shape of two pieces cuts");
		nearly(A::Area(bars), 2.f, "inside it is both of them");
		check(bars.QuadCount() == 2, "and a four-cornered piece stays one quad");
		SpriteMesh2D gap = m;
		ClipSpriteMeshPieces2D(gap, 0, two2, false);
		nearly(A::Area(gap), 2.f, "outside it is what is between them");
		nearly(gap.minBounds.x, -0.5f, "from one's edge");
		nearly(gap.maxBounds.x, 0.5f, "to the other's");

		// An image's opaque part as rectangles: an L, 4x4 pixels.
		//   X . . .
		//   X . . .
		//   X X X X
		//   . . . .
		std::vector<uchar> img(4 * 4 * 4, 0);
		const int on[][2] = { {0,0}, {0,1}, {0,2}, {1,2}, {2,2}, {3,2} };
		for (int i = 0; i < 6; i++) img[(on[i][1] * 4 + on[i][0]) * 4 + 3] = 255;
		int32 gw = 0, gh = 0;
		std::vector<SpriteRect2D> rects = SpriteAlphaRects2D(&img[0], 4, 4, 127, 128, gw, gh);
		int covered = 0;
		bool right = true;
		for (size_t i = 0; i < rects.size(); i++)
			for (int y = rects[i].y; y < rects[i].y + rects[i].h; y++)
				for (int x = rects[i].x; x < rects[i].x + rects[i].w; x++)
				{ covered++; if (img[(y * 4 + x) * 4 + 3] == 0) right = false; }
		check(gw == 4 && gh == 4 && covered == 6 && right, "an image's opaque texels are covered exactly, once each");
		check(rects.size() == 2, "an L is two rectangles, not six");
		std::vector<uchar> big(300 * 300 * 4, 255);
		rects = SpriteAlphaRects2D(&big[0], 300, 300, 127, 128, gw, gh);
		check(rects.size() == 1 && gw <= 128 && rects[0].w == gw && rects[0].h == gh, "a big solid image is read coarser and is one rectangle");
		std::vector<uchar> clear(8 * 8 * 4, 0);
		check(SpriteAlphaRects2D(&clear[0], 8, 8, 127, 128, gw, gh).empty(), "a clear image has none");

		// In a batch: only the quads from `firstVertex` on are cut.
		SpriteMesh2D two = m;
		Matrix shift;
		shift.Translate(Vec3(10.f, 0.f, 0.f));
		const size_t first = two.vertex.size();
		BuildSpriteMesh2D(ms, mp, two, &shift);
		std::vector<Vec2> farHalf;
		farHalf.push_back(Vec2(10.f, -5.f)); farHalf.push_back(Vec2(15.f, -5.f)); farHalf.push_back(Vec2(15.f, 5.f)); farHalf.push_back(Vec2(10.f, 5.f));
		ClipSpriteMesh2D(two, first, farHalf, true);
		nearly(A::Area(two), 6.f, "cutting the second sprite of a batch leaves the first whole");
		nearly(two.minBounds.x, -1.f, "and the bounds are of both");
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
