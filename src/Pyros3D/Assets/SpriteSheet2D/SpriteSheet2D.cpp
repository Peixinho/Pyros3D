//============================================================================
// Name        : SpriteSheet2D.cpp
// Description : Reading, writing, cutting and meshing a .p3ds - see the
//               header for what one is.
//============================================================================

#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Ext/stb/stb_image.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <map>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace p3d {

	namespace {

		using json = nlohmann::json;
		// Order-preserving, for other tools' atlases: Aseprite's "JSON hash"
		// names its frames "walk 0", "walk 1" ... "walk 10" and refers to
		// them from its tags BY POSITION. The default json object sorts its
		// keys, which files "walk 10" before "walk 2" and makes every tag
		// point at the wrong frames with nothing to say so.
		using ojson = nlohmann::ordered_json;

		const int32 kSpriteSheet2DVersion = 1;

		// A sprite repeated to fill a rectangle is one quad per repeat, and a
		// size typed with one zero too many must not become a million of them.
		const int32 kMaxTiledQuads = 4096;

		void Fail(std::string* errorOut, const std::string &msg)
		{
			if (errorOut) *errorOut = msg;
		}

		std::string StripExtension(const std::string &name)
		{
			const size_t slash = name.find_last_of("/\\");
			const size_t dot = name.find_last_of('.');
			if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return name;
			return name.substr(0, dot);
		}

		f32 StepDuration(const SpriteSheet2D &sheet, const SpriteClip2D &clip, const int32 step)
		{
			const int32 fi = clip.frames[(size_t)step];
			if (sheet.ValidFrame(fi) && sheet.frames[(size_t)fi].duration > 0.f)
				return sheet.frames[(size_t)fi].duration;
			return clip.fps > 0.0001f ? 1.f / clip.fps : 0.f;
		}

		// The order a clip's steps are visited in over one pass. A ping-pong
		// goes out and comes back without holding either end frame twice.
		void PassOrder(const SpriteClip2D &clip, std::vector<int32> &order)
		{
			const int32 n = (int32)clip.frames.size();
			order.clear();
			for (int32 i = 0; i < n; i++) order.push_back(i);
			if (clip.pingPong && n > 2)
				for (int32 i = n - 2; i >= 1; i--) order.push_back(i);
		}

		void PushQuad(SpriteMesh2D &out, const f32 x0, const f32 y0, const f32 x1, const f32 y1,
			const f32 u0, const f32 vBottom, const f32 u1, const f32 vTop,
			const Vec4 &color)
		{
			const __INDEX_C_TYPE__ base = (__INDEX_C_TYPE__)out.vertex.size();
			// The winding every 2D quad in the engine uses - see
			// TileMap2D::BuildChunkMesh. The quad's bottom edge takes the
			// rect's bottom v.
			out.vertex.push_back(Vec3(x0, y0, 0.f)); out.texcoord.push_back(Vec2(u0, vBottom));
			out.vertex.push_back(Vec3(x1, y0, 0.f)); out.texcoord.push_back(Vec2(u1, vBottom));
			out.vertex.push_back(Vec3(x1, y1, 0.f)); out.texcoord.push_back(Vec2(u1, vTop));
			out.vertex.push_back(Vec3(x0, y1, 0.f)); out.texcoord.push_back(Vec2(u0, vTop));
			for (int k = 0; k < 4; k++)
			{
				out.normal.push_back(Vec3(0.f, 0.f, 1.f));
				out.color.push_back(color);
			}
			out.index.push_back(base + 0); out.index.push_back(base + 1); out.index.push_back(base + 2);
			out.index.push_back(base + 2); out.index.push_back(base + 3); out.index.push_back(base + 0);
		}

		// The frame's own rectangle in local space, pivot at the origin:
		// what Simple draws, before any flip.
		void SimpleRect(const SpriteFrame2D &f, const f32 ppu,
			f32 &x0, f32 &y0, f32 &x1, f32 &y1)
		{
			const f32 sw = (f32)f.SourceW() / ppu;
			const f32 sh = (f32)f.SourceH() / ppu;
			const f32 left = -f.pivot.x * sw;
			const f32 bottom = -f.pivot.y * sh;
			// The kept rectangle inside the source one. offsetY is measured
			// from the TOP, like everything a packer writes.
			x0 = left + (f32)f.offsetX / ppu;
			x1 = x0 + (f32)f.w / ppu;
			y1 = bottom + sh - (f32)f.offsetY / ppu;
			y0 = y1 - (f32)f.h / ppu;
		}

	}

	// ------------------------------------------------------------------------

	void SpriteSheet2D::SetImageSize(const int32 w, const int32 h)
	{
		imageW = w > 0 ? w : 0;
		imageH = h > 0 ? h : 0;
	}

	int32 SpriteSheet2D::FindFrame(const std::string &name) const
	{
		for (size_t i = 0; i < frames.size(); i++)
			if (frames[i].name == name) return (int32)i;
		return -1;
	}

	int32 SpriteSheet2D::FindClip(const std::string &name) const
	{
		for (size_t i = 0; i < clips.size(); i++)
			if (clips[i].name == name) return (int32)i;
		return -1;
	}

	Vec4 SpriteSheet2D::UVRect(const int32 frame) const
	{
		if (!ValidFrame(frame) || !HaveImageSize()) return Vec4(0.f, 0.f, 0.f, 0.f);
		const SpriteFrame2D &f = frames[(size_t)frame];
		const f32 w = (f32)imageW, h = (f32)imageH;
		// Never past the middle: a one-pixel frame still samples its pixel.
		const f32 ix = std::min(EdgeInset(), (f32)f.w * 0.5f), iy = std::min(EdgeInset(), (f32)f.h * 0.5f);
		return Vec4(
			((f32)f.x + ix) / w,
			((f32)f.y + iy) / h,
			((f32)(f.x + f.w) - ix) / w,
			((f32)(f.y + f.h) - iy) / h);
	}

	int32 SpriteSheet2D::SliceGrid(const int32 cellW, const int32 cellH,
		const int32 margin, const int32 spacing, const std::string &prefix)
	{
		if (cellW < 1 || cellH < 1 || margin < 0 || spacing < 0 || !HaveImageSize()) return 0;

		// n cells occupy margin + n*size + (n-1)*spacing + margin - the same
		// arithmetic a tileset uses, so a sheet cut as a tileset and as a
		// sprite sheet agree about where its cells are.
		const int32 usableW = imageW - 2 * margin + spacing;
		const int32 usableH = imageH - 2 * margin + spacing;
		const int32 cols = usableW > 0 ? usableW / (cellW + spacing) : 0;
		const int32 rows = usableH > 0 ? usableH / (cellH + spacing) : 0;
		if (cols < 1 || rows < 1) return 0;

		frames.clear();
		clips.clear();
		for (int32 r = 0; r < rows; r++)
			for (int32 c = 0; c < cols; c++)
			{
				SpriteFrame2D f;
				f.name = prefix + std::to_string(r * cols + c);
				f.x = margin + c * (cellW + spacing);
				f.y = margin + r * (cellH + spacing);
				f.w = cellW;
				f.h = cellH;
				frames.push_back(f);
			}
		return (int32)frames.size();
	}

	int32 SpriteSheet2D::SliceCount(const int32 columns, const int32 rows,
		const std::string &prefix)
	{
		if (columns < 1 || rows < 1 || !HaveImageSize()) return 0;
		return SliceGrid(imageW / columns, imageH / rows, 0, 0, prefix);
	}

	int32 SpriteSheet2D::AddClipRange(const std::string &name, const int32 first,
		const int32 last, const f32 fps, const bool loop)
	{
		if (!ValidFrame(first) || !ValidFrame(last)) return -1;
		SpriteClip2D c;
		c.name = name;
		c.fps = fps;
		c.loop = loop;
		const int32 dir = last >= first ? 1 : -1;
		for (int32 i = first; i != last + dir; i += dir) c.frames.push_back(i);
		clips.push_back(c);
		return (int32)clips.size() - 1;
	}

	// ------------------------------------------------------------------------

	f32 SpriteClip2DLength(const SpriteSheet2D &sheet, const SpriteClip2D &clip)
	{
		std::vector<int32> order;
		PassOrder(clip, order);
		f32 total = 0.f;
		for (size_t i = 0; i < order.size(); i++) total += StepDuration(sheet, clip, order[i]);
		return total;
	}

	SpriteClipCursor2D SpriteClip2DAt(const SpriteSheet2D &sheet,
		const SpriteClip2D &clip, const f32 seconds)
	{
		SpriteClipCursor2D out;
		if (clip.frames.empty()) { out.finished = true; return out; }

		std::vector<int32> order;
		PassOrder(clip, order);
		const f32 length = SpriteClip2DLength(sheet, clip);

		f32 t = seconds < 0.f ? 0.f : seconds;
		if (length <= 0.f)
		{
			// A clip with no rate holds its first frame: dividing by its
			// length would be the alternative.
			out.step = order[0];
			out.frame = clip.frames[(size_t)out.step];
			return out;
		}
		if (clip.loop) t = fmodf(t, length);
		else if (t >= length)
		{
			out.step = order.back();
			out.frame = clip.frames[(size_t)out.step];
			out.finished = true;
			return out;
		}

		f32 acc = 0.f;
		for (size_t i = 0; i < order.size(); i++)
		{
			acc += StepDuration(sheet, clip, order[i]);
			if (t < acc || i + 1 == order.size())
			{
				out.step = order[i];
				out.frame = clip.frames[(size_t)out.step];
				return out;
			}
		}
		return out;
	}

	// ------------------------------------------------------------------------

	std::vector<SpriteRect2D> SpriteSheet2DAutoSlice(const uchar* rgba,
		const int32 w, const int32 h, const int32 alphaThreshold,
		const int32 merge, const int32 minSize)
	{
		std::vector<SpriteRect2D> out;
		if (!rgba || w < 1 || h < 1) return out;

		// Islands first: a flood fill over opaque pixels, 8-connected so a
		// diagonal line of pixels is one shape and not one per pixel.
		std::vector<uint8> seen((size_t)w * h, 0);
		std::vector<int32> stack;
		struct Box { int32 x0, y0, x1, y1; };
		std::vector<Box> boxes;

		for (int32 sy = 0; sy < h; sy++)
			for (int32 sx = 0; sx < w; sx++)
			{
				const size_t si = (size_t)sy * w + sx;
				if (seen[si] || rgba[si * 4 + 3] <= alphaThreshold) continue;
				Box b = { sx, sy, sx, sy };
				stack.clear();
				stack.push_back((int32)si);
				seen[si] = 1;
				while (!stack.empty())
				{
					const int32 at = stack.back();
					stack.pop_back();
					const int32 px = at % w, py = at / w;
					if (px < b.x0) b.x0 = px;
					if (px > b.x1) b.x1 = px;
					if (py < b.y0) b.y0 = py;
					if (py > b.y1) b.y1 = py;
					for (int32 dy = -1; dy <= 1; dy++)
						for (int32 dx = -1; dx <= 1; dx++)
						{
							if (!dx && !dy) continue;
							const int32 nx = px + dx, ny = py + dy;
							if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
							const size_t ni = (size_t)ny * w + nx;
							if (seen[ni] || rgba[ni * 4 + 3] <= alphaThreshold) continue;
							seen[ni] = 1;
							stack.push_back((int32)ni);
						}
				}
				boxes.push_back(b);
			}

		// Then boxes that touch, overlap or sit within `merge` pixels of each
		// other become one. Repeated until nothing joins, because joining two
		// can bring a third into reach.
		const int32 reach = merge < 0 ? 0 : merge;
		bool joined = true;
		while (joined)
		{
			joined = false;
			for (size_t a = 0; a < boxes.size() && !joined; a++)
				for (size_t b = a + 1; b < boxes.size() && !joined; b++)
				{
					const Box &A = boxes[a];
					const Box &B = boxes[b];
					// The gap between two boxes is the count of empty
					// pixels between them: touching boxes have none.
					if (A.x0 - B.x1 - 1 > reach || B.x0 - A.x1 - 1 > reach
						|| A.y0 - B.y1 - 1 > reach || B.y0 - A.y1 - 1 > reach) continue;
					Box m = { std::min(A.x0, B.x0), std::min(A.y0, B.y0),
						std::max(A.x1, B.x1), std::max(A.y1, B.y1) };
					boxes[a] = m;
					boxes.erase(boxes.begin() + (std::ptrdiff_t)b);
					joined = true;
				}
		}

		std::vector<Box> kept;
		for (size_t i = 0; i < boxes.size(); i++)
		{
			const int32 bw = boxes[i].x1 - boxes[i].x0 + 1;
			const int32 bh = boxes[i].y1 - boxes[i].y0 + 1;
			if (bw < minSize && bh < minSize) continue;
			kept.push_back(boxes[i]);
		}

		// Reading order. Sprites on a hand-drawn sheet are in rows but not on
		// a grid, so "the same row" is "overlaps the row vertically", not
		// "has the same top".
		std::sort(kept.begin(), kept.end(), [](const Box &a, const Box &b) {
			return a.y0 != b.y0 ? a.y0 < b.y0 : a.x0 < b.x0; });
		std::vector<bool> used(kept.size(), false);
		for (size_t i = 0; i < kept.size(); i++)
		{
			if (used[i]) continue;
			std::vector<Box> row;
			int32 rowBottom = kept[i].y1;
			row.push_back(kept[i]);
			used[i] = true;
			for (size_t k = i + 1; k < kept.size(); k++)
			{
				if (used[k]) continue;
				// Its vertical middle falls inside the row so far.
				const int32 mid = (kept[k].y0 + kept[k].y1) / 2;
				if (mid <= rowBottom && kept[k].y0 <= rowBottom)
				{
					row.push_back(kept[k]);
					used[k] = true;
				}
			}
			std::sort(row.begin(), row.end(), [](const Box &a, const Box &b) { return a.x0 < b.x0; });
			for (size_t r = 0; r < row.size(); r++)
			{
				SpriteRect2D sr;
				sr.x = row[r].x0;
				sr.y = row[r].y0;
				sr.w = row[r].x1 - row[r].x0 + 1;
				sr.h = row[r].y1 - row[r].y0 + 1;
				out.push_back(sr);
			}
		}
		return out;
	}

	// ------------------------------------------------------------------------

	bool SpriteSheet2DAutoSliceFile(const std::string &resolvedImage,
		std::vector<SpriteRect2D> &out, const int32 alphaThreshold,
		const int32 merge, const int32 minSize)
	{
		int w = 0, h = 0, comp = 0;
		// Four channels whatever the file has: an image with no alpha comes
		// back opaque, which cuts as one sprite - the right answer for it.
		uchar* px = stbi_load(resolvedImage.c_str(), &w, &h, &comp, 4);
		if (!px) return false;
		out = SpriteSheet2DAutoSlice(px, (int32)w, (int32)h, alphaThreshold, merge, minSize);
		stbi_image_free(px);
		return true;
	}

	// ------------------------------------------------------------------------

	bool PackSpriteAtlas2D(const std::vector<SpriteAtlasImage2D> &images,
		const SpriteAtlasOptions2D &options, std::vector<uchar> &rgbaOut,
		int32 &widthOut, int32 &heightOut, std::vector<SpriteFrame2D> &framesOut,
		std::string* errorOut)
	{
		if (images.empty()) { Fail(errorOut, "no images to pack"); return false; }
		const int32 pad = std::max<int32>(0, options.padding);
		const int32 extrude = std::max<int32>(0, std::min<int32>(options.extrude, pad / 2));
		const int32 maxSize = std::max<int32>(16, options.maxSize);

		// What is kept of each image: all of it, or the box round what can be seen.
		struct Kept { int32 x, y, w, h; };
		std::vector<Kept> kept(images.size());
		long long area = 0;
		int32 widest = 0, tallest = 0;
		for (size_t i = 0; i < images.size(); i++)
		{
			const SpriteAtlasImage2D &im = images[i];
			if (im.w < 1 || im.h < 1 || im.rgba.size() < (size_t)im.w * (size_t)im.h * 4)
			{ Fail(errorOut, "image '" + im.name + "' has no pixels"); return false; }
			Kept k = { 0, 0, im.w, im.h };
			if (options.trim)
			{
				int32 x0 = im.w, y0 = im.h, x1 = -1, y1 = -1;
				for (int32 y = 0; y < im.h; y++)
					for (int32 x = 0; x < im.w; x++)
						if ((int32)im.rgba[((size_t)y * im.w + x) * 4 + 3] > options.alphaThreshold)
						{
							if (x < x0) x0 = x;
							if (x > x1) x1 = x;
							if (y < y0) y0 = y;
							if (y > y1) y1 = y;
						}
				// Nothing to see at all: one pixel, so the frame still exists
				// and a clip that names it still has something to show.
				if (x1 < 0) { k.x = 0; k.y = 0; k.w = 1; k.h = 1; }
				else { k.x = x0; k.y = y0; k.w = x1 - x0 + 1; k.h = y1 - y0 + 1; }
			}
			kept[i] = k;
			area += (long long)(k.w + pad) * (long long)(k.h + pad);
			widest = std::max(widest, k.w);
			tallest = std::max(tallest, k.h);
		}
		if (widest + 2 * pad > maxSize || tallest + 2 * pad > maxSize)
		{ Fail(errorOut, "an image is bigger than the largest atlas allowed (" + std::to_string(maxSize) + " px)"); return false; }

		// Tallest first onto shelves: rows as tall as their first image, so
		// images of like height share one and little is wasted above them.
		std::vector<uint32> order(images.size());
		for (size_t i = 0; i < order.size(); i++) order[i] = (uint32)i;
		std::stable_sort(order.begin(), order.end(), [&kept](const uint32 a, const uint32 b) {
			if (kept[a].h != kept[b].h) return kept[a].h > kept[b].h;
			return kept[a].w > kept[b].w;
		});

		std::vector<int32> px(images.size()), py(images.size());
		struct Shelves {
			static bool Fit(const std::vector<Kept> &kept, const std::vector<uint32> &order, const int32 W, const int32 H,
				const int32 pad, std::vector<int32> &px, std::vector<int32> &py, int32 &usedW, int32 &usedH)
			{
				int32 x = pad, y = pad, shelf = 0;
				usedW = usedH = 0;
				for (size_t n = 0; n < order.size(); n++)
				{
					const Kept &k = kept[order[n]];
					if (x + k.w + pad > W) { x = pad; y += shelf + pad; shelf = 0; }
					if (x + k.w + pad > W || y + k.h + pad > H) return false;
					px[order[n]] = x; py[order[n]] = y;
					x += k.w + pad;
					shelf = std::max(shelf, k.h);
					usedW = std::max(usedW, x);
					usedH = std::max(usedH, y + k.h + pad);
				}
				return true;
			}
		};

		// The smallest square-ish picture they fit: start at the area's
		// square root and widen, then heighten, in turn.
		int32 W = 16, H = 16;
		while ((long long)W * W < area && W < maxSize) W *= 2;
		H = W;
		while (W < widest + 2 * pad) W *= 2;
		// Try the half-height first: a set that fills less than half is common.
		if (H / 2 >= tallest + 2 * pad) H /= 2;
		int32 usedW = 0, usedH = 0;
		bool fits = false;
		for (int guard = 0; guard < 32; guard++)
		{
			if (W > maxSize || H > maxSize) break;
			if (Shelves::Fit(kept, order, W, H, pad, px, py, usedW, usedH)) { fits = true; break; }
			if (H < W) H *= 2; else W *= 2;
		}
		if (!fits)
		{ Fail(errorOut, "the images do not fit in " + std::to_string(maxSize) + " x " + std::to_string(maxSize) + " px"); return false; }
		if (!options.powerOfTwo) { W = usedW; H = usedH; }

		widthOut = W;
		heightOut = H;
		rgbaOut.assign((size_t)W * (size_t)H * 4, 0);
		framesOut.clear();
		framesOut.resize(images.size());
		for (size_t i = 0; i < images.size(); i++)
		{
			const SpriteAtlasImage2D &im = images[i];
			const Kept &k = kept[i];
			// With the edge pixels repeated outward: the clamp below is what
			// repeats them.
			for (int32 y = -extrude; y < k.h + extrude; y++)
			{
				const int32 sy = k.y + std::max<int32>(0, std::min<int32>(y, k.h - 1));
				const int32 dy = py[i] + y;
				if (dy < 0 || dy >= H) continue;
				for (int32 x = -extrude; x < k.w + extrude; x++)
				{
					const int32 sx = k.x + std::max<int32>(0, std::min<int32>(x, k.w - 1));
					const int32 dx = px[i] + x;
					if (dx < 0 || dx >= W) continue;
					const uchar* from = &im.rgba[((size_t)sy * im.w + sx) * 4];
					uchar* to = &rgbaOut[((size_t)dy * W + dx) * 4];
					to[0] = from[0]; to[1] = from[1]; to[2] = from[2]; to[3] = from[3];
				}
			}
			SpriteFrame2D &f = framesOut[i];
			f.name = im.name;
			f.x = px[i]; f.y = py[i]; f.w = k.w; f.h = k.h;
			if (k.w != im.w || k.h != im.h)
			{
				f.sourceW = im.w; f.sourceH = im.h;
				f.offsetX = k.x; f.offsetY = k.y;
			}
		}
		return true;
	}

	bool PackSpriteAtlasFiles2D(const std::vector<std::string> &resolvedImages,
		const SpriteAtlasOptions2D &options, const std::string &resolvedOutputPng,
		SpriteSheet2D &sheetOut, std::string* errorOut)
	{
		std::vector<SpriteAtlasImage2D> images(resolvedImages.size());
		for (size_t i = 0; i < resolvedImages.size(); i++)
		{
			int w = 0, h = 0, comp = 0;
			uchar* px = stbi_load(resolvedImages[i].c_str(), &w, &h, &comp, 4);
			if (!px) { Fail(errorOut, "could not read " + resolvedImages[i]); return false; }
			{
				// The file's own name, without its folder or its extension.
				std::string n = resolvedImages[i];
				const size_t slash = n.find_last_of("/\\");
				if (slash != std::string::npos) n = n.substr(slash + 1);
				const size_t dot = n.find_last_of('.');
				if (dot != std::string::npos && dot > 0) n = n.substr(0, dot);
				images[i].name = n;
			}
			images[i].w = (int32)w;
			images[i].h = (int32)h;
			images[i].rgba.assign(px, px + (size_t)w * (size_t)h * 4);
			stbi_image_free(px);
		}
		std::vector<uchar> rgba;
		int32 W = 0, H = 0;
		std::vector<SpriteFrame2D> frames;
		if (!PackSpriteAtlas2D(images, options, rgba, W, H, frames, errorOut)) return false;
		if (!PaintableImage::WritePNG(resolvedOutputPng, W, H, 4, &rgba[0]))
		{ Fail(errorOut, "could not write " + resolvedOutputPng); return false; }
		sheetOut.frames = frames;
		sheetOut.SetImageSize(W, H);
		// Padded frames have nothing beside them to keep out - see EdgeInset.
		sheetOut.edgeInset = options.padding >= 1 ? 0.02f : -1.f;
		return true;
	}

	int32 SpriteSheet2DClipsFromNames(SpriteSheet2D &sheet, const f32 fps)
	{
		// name -> (number, frame index), kept in the order names first appear.
		std::vector<std::string> names;
		std::map<std::string, std::vector<std::pair<long, int32> > > runs;
		for (size_t i = 0; i < sheet.frames.size(); i++)
		{
			const std::string &n = sheet.frames[i].name;
			size_t d = n.size();
			while (d > 0 && n[d - 1] >= '0' && n[d - 1] <= '9') d--;
			if (d == n.size() || d == 0) continue;          // no number, or nothing but one
			std::string base = n.substr(0, d);
			while (!base.empty() && (base[base.size() - 1] == '_' || base[base.size() - 1] == '-' || base[base.size() - 1] == ' '))
				base.erase(base.size() - 1);
			if (base.empty()) continue;
			if (!runs.count(base)) names.push_back(base);
			runs[base].push_back(std::make_pair(atol(n.substr(d).c_str()), (int32)i));
		}
		int32 made = 0;
		for (size_t k = 0; k < names.size(); k++)
		{
			std::vector<std::pair<long, int32> > &r = runs[names[k]];
			if (r.size() < 2) continue;
			bool exists = false;
			for (size_t c = 0; c < sheet.clips.size(); c++) if (sheet.clips[c].name == names[k]) exists = true;
			if (exists) continue;
			std::stable_sort(r.begin(), r.end(), [](const std::pair<long, int32> &a, const std::pair<long, int32> &b) { return a.first < b.first; });
			SpriteClip2D clip;
			clip.name = names[k];
			clip.fps = fps > 0.f ? fps : 12.f;
			for (size_t i = 0; i < r.size(); i++) clip.frames.push_back(r[i].second);
			sheet.clips.push_back(clip);
			made++;
		}
		return made;
	}

	bool ImportSpriteAtlasJson(const std::string &text, SpriteSheet2D &out,
		std::string *errorOut)
	{
		ojson j;
		try {
			j = ojson::parse(text);
		}
		catch (const std::exception &e) {
			Fail(errorOut, std::string("malformed atlas JSON: ") + e.what());
			return false;
		}
		if (!j.is_object() || !j.contains("frames"))
		{
			Fail(errorOut, "not a sprite atlas: no \"frames\"");
			return false;
		}

		SpriteSheet2D s;
		s.pixelsPerUnit = out.pixelsPerUnit;
		s.filter = out.filter;

		// Both shapes the packers write: an object keyed by name, or an array
		// whose entries carry "filename".
		std::vector<std::pair<std::string, const ojson*> > entries;
		const ojson &fr = j["frames"];
		if (fr.is_object())
			for (ojson::const_iterator it = fr.begin(); it != fr.end(); ++it)
				entries.push_back(std::make_pair(it.key(), &it.value()));
		else if (fr.is_array())
			for (size_t i = 0; i < fr.size(); i++)
				entries.push_back(std::make_pair(
					fr[i].is_object() ? fr[i].value("filename", std::string()) : std::string(), &fr[i]));
		else
		{
			Fail(errorOut, "\"frames\" is neither an object nor an array");
			return false;
		}

		for (size_t i = 0; i < entries.size(); i++)
		{
			const ojson &e = *entries[i].second;
			if (!e.is_object() || !e.contains("frame") || !e["frame"].is_object())
			{
				Fail(errorOut, "frame '" + entries[i].first + "' has no rectangle");
				return false;
			}
			if (e.value("rotated", false))
			{
				Fail(errorOut, "frame '" + entries[i].first
					+ "' is rotated in the atlas - re-export with rotation off");
				return false;
			}
			SpriteFrame2D f;
			f.name = StripExtension(entries[i].first);
			if (f.name.empty()) f.name = "frame" + std::to_string(i);
			const ojson &r = e["frame"];
			f.x = r.value("x", 0);
			f.y = r.value("y", 0);
			f.w = r.value("w", 0);
			f.h = r.value("h", 0);
			if (f.w < 1 || f.h < 1)
			{
				Fail(errorOut, "frame '" + entries[i].first + "' has no area");
				return false;
			}
			if (e.contains("sourceSize") && e["sourceSize"].is_object())
			{
				f.sourceW = e["sourceSize"].value("w", 0);
				f.sourceH = e["sourceSize"].value("h", 0);
			}
			if (e.contains("spriteSourceSize") && e["spriteSourceSize"].is_object())
			{
				f.offsetX = e["spriteSourceSize"].value("x", 0);
				f.offsetY = e["spriteSourceSize"].value("y", 0);
			}
			// Untrimmed: say so the plain way, so the file does not carry a
			// "trim" that trims nothing.
			if (f.sourceW == f.w && f.sourceH == f.h && f.offsetX == 0 && f.offsetY == 0)
				f.sourceW = f.sourceH = 0;
			if (e.contains("pivot") && e["pivot"].is_object())
			{
				// Packers measure the pivot from the top-left; a sprite's is
				// y-up.
				f.pivot.x = e["pivot"].value("x", 0.5f);
				f.pivot.y = 1.f - e["pivot"].value("y", 0.5f);
			}
			// Milliseconds in Aseprite's export.
			if (e.contains("duration") && e["duration"].is_number())
				f.duration = e["duration"].get<f32>() / 1000.f;
			s.frames.push_back(f);
		}

		if (j.contains("meta") && j["meta"].is_object())
		{
			const ojson &meta = j["meta"];
			s.image = meta.value("image", std::string());
			if (meta.contains("size") && meta["size"].is_object())
				s.SetImageSize(meta["size"].value("w", 0), meta["size"].value("h", 0));
			if (meta.contains("frameTags") && meta["frameTags"].is_array())
			{
				const ojson &tags = meta["frameTags"];
				for (size_t i = 0; i < tags.size(); i++)
				{
					if (!tags[i].is_object()) continue;
					const int32 from = tags[i].value("from", 0);
					const int32 to = tags[i].value("to", 0);
					if (!s.ValidFrame(from) || !s.ValidFrame(to)) continue;
					const std::string dir = tags[i].value("direction", std::string("forward"));
					SpriteClip2D c;
					c.name = tags[i].value("name", std::string("clip") + std::to_string(i));
					if (dir == "reverse")
						for (int32 k = to; k >= from; k--) c.frames.push_back(k);
					else
						for (int32 k = from; k <= to; k++) c.frames.push_back(k);
					c.pingPong = (dir == "pingpong" || dir == "pingpong_reverse");
					// Frames carry their own durations; this is the rate for
					// any that do not.
					f32 total = 0.f;
					for (size_t k = 0; k < c.frames.size(); k++)
						total += s.frames[(size_t)c.frames[k]].duration;
					c.fps = (total > 0.f) ? (f32)c.frames.size() / total : 12.f;
					s.clips.push_back(c);
				}
			}
			// Aseprite's slices carry nine-slice centres and pivots for the
			// whole sprite; applied to every frame, which is what a slice
			// without keys means.
			if (meta.contains("slices") && meta["slices"].is_array())
			{
				const ojson &slices = meta["slices"];
				for (size_t i = 0; i < slices.size(); i++)
				{
					if (!slices[i].is_object() || !slices[i].contains("keys")
						|| !slices[i]["keys"].is_array() || slices[i]["keys"].empty()) continue;
					const ojson &key = slices[i]["keys"][0];
					if (!key.is_object() || !key.contains("bounds") || !key.contains("center")) continue;
					const ojson &b = key["bounds"];
					const ojson &c = key["center"];
					const f32 bw = b.value("w", 0.f), bh = b.value("h", 0.f);
					const f32 cx = c.value("x", 0.f), cy = c.value("y", 0.f);
					const f32 cw = c.value("w", 0.f), ch = c.value("h", 0.f);
					if (bw <= 0.f || bh <= 0.f) continue;
					for (size_t k = 0; k < s.frames.size(); k++)
						s.frames[k].border = Vec4(cx, bh - cy - ch, bw - cx - cw, cy);
					break;
				}
			}
		}

		if (s.frames.empty())
		{
			Fail(errorOut, "the atlas holds no frames");
			return false;
		}
		out = s;
		return true;
	}

	// ------------------------------------------------------------------------

	std::string SpriteSheet2DToString(const SpriteSheet2D &sheet)
	{
		json j;
		j["version"] = kSpriteSheet2DVersion;
		j["image"] = sheet.image;
		if (!sheet.normalImage.empty()) j["normalImage"] = sheet.normalImage;
		if (sheet.edgeInset >= 0.f) j["edgeInset"] = sheet.edgeInset;
		j["pixelsPerUnit"] = sheet.pixelsPerUnit;
		j["filter"] = sheet.filter == SpriteFilter2D::Nearest ? "nearest" : "linear";

		json frames = json::array();
		for (size_t i = 0; i < sheet.frames.size(); i++)
		{
			const SpriteFrame2D &f = sheet.frames[i];
			json e;
			e["name"] = f.name;
			e["rect"] = { f.x, f.y, f.w, f.h };
			// Only what differs from the default is written, so a plain grid
			// sheet stays one short line a frame.
			if (fabsf(f.pivot.x - 0.5f) > 1e-6f || fabsf(f.pivot.y - 0.5f) > 1e-6f)
				e["pivot"] = { f.pivot.x, f.pivot.y };
			if (f.HasBorder())
				e["border"] = { f.border.x, f.border.y, f.border.z, f.border.w };
			if (f.sourceW > 0 || f.sourceH > 0 || f.offsetX != 0 || f.offsetY != 0)
				e["source"] = { f.sourceW, f.sourceH, f.offsetX, f.offsetY };
			if (f.duration > 0.f) e["duration"] = f.duration;
			frames.push_back(e);
		}
		j["frames"] = frames;

		json clips = json::array();
		for (size_t i = 0; i < sheet.clips.size(); i++)
		{
			const SpriteClip2D &c = sheet.clips[i];
			json e;
			e["name"] = c.name;
			e["frames"] = c.frames;
			e["fps"] = c.fps;
			e["loop"] = c.loop;
			if (c.pingPong) e["pingPong"] = true;
			if (!c.events.empty())
			{
				json ev = json::array();
				for (size_t k = 0; k < c.events.size(); k++)
					ev.push_back({ { "at", c.events[k].at }, { "name", c.events[k].name } });
				e["events"] = ev;
			}
			clips.push_back(e);
		}
		j["clips"] = clips;
		return j.dump(1, '\t');
	}

	bool SpriteSheet2DFromString(const std::string &text, SpriteSheet2D &out,
		std::string *errorOut)
	{
		json j;
		try {
			j = json::parse(text);
		}
		catch (const std::exception &e) {
			Fail(errorOut, std::string("malformed sprite sheet JSON: ") + e.what());
			return false;
		}
		if (!j.is_object()) { Fail(errorOut, "sprite sheet is not a JSON object"); return false; }

		SpriteSheet2D s;
		try {
			s.image = j.value("image", std::string());
			s.normalImage = j.value("normalImage", std::string());
			s.edgeInset = j.value("edgeInset", -1.f);
			s.pixelsPerUnit = j.value("pixelsPerUnit", 100.f);
			// Checked, not clamped: a zero here divides every sprite's size
			// by nothing, and the file is the place it can still be fixed.
			if (!(s.pixelsPerUnit > 0.f))
			{
				Fail(errorOut, "pixelsPerUnit must be greater than 0");
				return false;
			}
			s.filter = j.value("filter", std::string("linear")) == "nearest"
				? SpriteFilter2D::Nearest : SpriteFilter2D::Linear;

			if (j.contains("frames") && j["frames"].is_array())
			{
				const json &frames = j["frames"];
				for (size_t i = 0; i < frames.size(); i++)
				{
					const json &e = frames[i];
					if (!e.is_object() || !e.contains("rect") || !e["rect"].is_array()
						|| e["rect"].size() != 4)
					{
						Fail(errorOut, "frame " + std::to_string(i) + " has no [x, y, w, h] rect");
						return false;
					}
					SpriteFrame2D f;
					f.name = e.value("name", std::string("frame") + std::to_string(i));
					f.x = e["rect"][0].get<int32>();
					f.y = e["rect"][1].get<int32>();
					f.w = e["rect"][2].get<int32>();
					f.h = e["rect"][3].get<int32>();
					if (f.w < 1 || f.h < 1 || f.x < 0 || f.y < 0)
					{
						Fail(errorOut, "frame '" + f.name + "' has a rect with no area or a negative origin");
						return false;
					}
					if (e.contains("pivot") && e["pivot"].is_array() && e["pivot"].size() == 2)
						f.pivot = Vec2(e["pivot"][0].get<f32>(), e["pivot"][1].get<f32>());
					if (e.contains("border") && e["border"].is_array() && e["border"].size() == 4)
						f.border = Vec4(e["border"][0].get<f32>(), e["border"][1].get<f32>(),
							e["border"][2].get<f32>(), e["border"][3].get<f32>());
					if (e.contains("source") && e["source"].is_array() && e["source"].size() == 4)
					{
						f.sourceW = e["source"][0].get<int32>();
						f.sourceH = e["source"][1].get<int32>();
						f.offsetX = e["source"][2].get<int32>();
						f.offsetY = e["source"][3].get<int32>();
					}
					f.duration = e.value("duration", 0.f);
					s.frames.push_back(f);
				}
			}

			if (j.contains("clips") && j["clips"].is_array())
			{
				const json &clips = j["clips"];
				for (size_t i = 0; i < clips.size(); i++)
				{
					const json &e = clips[i];
					if (!e.is_object()) continue;
					SpriteClip2D c;
					c.name = e.value("name", std::string("clip") + std::to_string(i));
					c.fps = e.value("fps", 12.f);
					c.loop = e.value("loop", true);
					c.pingPong = e.value("pingPong", false);
					if (e.contains("frames") && e["frames"].is_array())
						for (size_t k = 0; k < e["frames"].size(); k++)
						{
							const int32 fi = e["frames"][k].get<int32>();
							// A clip pointing outside the sheet would draw
							// nothing for that step, silently - which reads
							// as a flicker in the animation.
							if (fi < 0 || fi >= (int32)s.frames.size())
							{
								Fail(errorOut, "clip '" + c.name + "' uses frame "
									+ std::to_string(fi) + ", and the sheet has "
									+ std::to_string(s.frames.size()));
								return false;
							}
							c.frames.push_back(fi);
						}
					if (e.contains("events") && e["events"].is_array())
						for (size_t k = 0; k < e["events"].size(); k++)
						{
							if (!e["events"][k].is_object()) continue;
							SpriteEvent2D ev;
							ev.at = e["events"][k].value("at", 0);
							ev.name = e["events"][k].value("name", std::string());
							c.events.push_back(ev);
						}
					s.clips.push_back(c);
				}
			}
		}
		catch (const std::exception &e) {
			Fail(errorOut, std::string("sprite sheet has a field of the wrong type: ") + e.what());
			return false;
		}

		out = s;
		return true;
	}

	bool LoadSpriteSheet2D(const std::string &filename, SpriteSheet2D &out,
		std::string *errorOut)
	{
		std::ifstream in(filename.c_str());
		if (!in.is_open())
		{
			Fail(errorOut, "could not open " + filename);
			return false;
		}
		std::stringstream ss;
		ss << in.rdbuf();
		std::string err;
		if (!SpriteSheet2DFromString(ss.str(), out, &err))
		{
			Fail(errorOut, filename + ": " + err);
			return false;
		}
		return true;
	}

	bool SaveSpriteSheet2D(const std::string &filename, const SpriteSheet2D &sheet,
		std::string *errorOut)
	{
		std::ofstream o(filename.c_str());
		if (!o.is_open())
		{
			Fail(errorOut, "could not write " + filename);
			return false;
		}
		o << SpriteSheet2DToString(sheet);
		return true;
	}

	// ------------------------------------------------------------------------

	namespace {
		struct ClipVert { Vec3 p; Vec2 uv; Vec4 c; };
		typedef std::vector<ClipVert> ClipPoly;

		ClipVert ClipLerp(const ClipVert &a, const ClipVert &b, const f32 t)
		{
			ClipVert o;
			o.p = Vec3(a.p.x + (b.p.x - a.p.x) * t, a.p.y + (b.p.y - a.p.y) * t, a.p.z + (b.p.z - a.p.z) * t);
			o.uv = Vec2(a.uv.x + (b.uv.x - a.uv.x) * t, a.uv.y + (b.uv.y - a.uv.y) * t);
			o.c = Vec4(a.c.x + (b.c.x - a.c.x) * t, a.c.y + (b.c.y - a.c.y) * t, a.c.z + (b.c.z - a.c.z) * t, a.c.w + (b.c.w - a.c.w) * t);
			return o;
		}
		// The part of `in` on one side of the line through a and b: its left
		// (the inside of a counter-clockwise outline) when keepLeft.
		void ClipHalf(const ClipPoly &in, const Vec2 &a, const Vec2 &b, const bool keepLeft, ClipPoly &out)
		{
			out.clear();
			const size_t n = in.size();
			for (size_t i = 0; i < n; i++)
			{
				const ClipVert &u = in[i], &v = in[(i + 1) % n];
				f32 du = (b.x - a.x) * (u.p.y - a.y) - (b.y - a.y) * (u.p.x - a.x);
				f32 dv = (b.x - a.x) * (v.p.y - a.y) - (b.y - a.y) * (v.p.x - a.x);
				if (!keepLeft) { du = -du; dv = -dv; }
				const bool inU = du >= 0.f, inV = dv >= 0.f;
				if (inU) out.push_back(u);
				if (inU != inV) out.push_back(ClipLerp(u, v, du / (du - dv)));
			}
		}
		f32 ClipArea(const ClipPoly &p)
		{
			f32 a = 0.f;
			for (size_t i = 0; i < p.size(); i++)
			{
				const Vec3 &u = p[i].p, &v = p[(i + 1) % p.size()].p;
				a += u.x * v.y - v.x * u.y;
			}
			return fabsf(a) * 0.5f;
		}
	}

	bool ClipSpriteMesh2D(SpriteMesh2D &mesh, const size_t firstVertex,
		const std::vector<Vec2> &outline, const bool inside)
	{
		if (outline.size() < 3 || firstVertex >= mesh.vertex.size()) return mesh.vertex.size() > firstVertex;
		// Counter-clockwise, whichever way it was handed over.
		std::vector<Vec2> mask = outline;
		{
			f32 a = 0.f;
			for (size_t i = 0; i < mask.size(); i++)
			{ const Vec2 &u = mask[i], &v = mask[(i + 1) % mask.size()]; a += u.x * v.y - v.x * u.y; }
			if (a < 0.f) std::reverse(mask.begin(), mask.end());
		}

		// The quads, taken out; what is left of each goes back in.
		const size_t quads = (mesh.vertex.size() - firstVertex) / 4;
		std::vector<ClipPoly> kept;
		std::vector<Vec3> keptT, keptB, keptN;
		for (size_t q = 0; q < quads; q++)
		{
			const size_t at = firstVertex + q * 4;
			ClipPoly poly(4);
			for (int k = 0; k < 4; k++)
			{
				poly[k].p = mesh.vertex[at + k];
				poly[k].uv = mesh.texcoord[at + k];
				poly[k].c = mesh.color[at + k];
			}
			const Vec3 T = at < mesh.tangent.size() ? mesh.tangent[at] : Vec3(1.f, 0.f, 0.f);
			const Vec3 B = at < mesh.bitangent.size() ? mesh.bitangent[at] : Vec3(0.f, 1.f, 0.f);
			const Vec3 N = at < mesh.normal.size() ? mesh.normal[at] : Vec3(0.f, 0.f, 1.f);
			ClipPoly work = poly, next;
			if (inside)
			{
				for (size_t e = 0; e < mask.size() && work.size() >= 3; e++)
				{
					ClipHalf(work, mask[e], mask[(e + 1) % mask.size()], true, next);
					work.swap(next);
				}
				if (work.size() >= 3 && ClipArea(work) > 1e-9f) { kept.push_back(work); keptT.push_back(T); keptB.push_back(B); keptN.push_back(N); }
			}
			else
			{
				// Outside a convex shape is not convex, so it is taken a
				// side at a time: what lies beyond the first edge, then of
				// what is left what lies beyond the second, and so on. The
				// pieces do not overlap, and what remains at the end is the
				// inside, which is thrown away.
				for (size_t e = 0; e < mask.size() && work.size() >= 3; e++)
				{
					ClipPoly beyond;
					ClipHalf(work, mask[e], mask[(e + 1) % mask.size()], false, beyond);
					if (beyond.size() >= 3 && ClipArea(beyond) > 1e-9f) { kept.push_back(beyond); keptT.push_back(T); keptB.push_back(B); keptN.push_back(N); }
					ClipHalf(work, mask[e], mask[(e + 1) % mask.size()], true, next);
					work.swap(next);
				}
			}
		}

		mesh.vertex.resize(firstVertex);
		mesh.normal.resize(std::min(mesh.normal.size(), firstVertex));
		mesh.tangent.resize(std::min(mesh.tangent.size(), firstVertex));
		mesh.bitangent.resize(std::min(mesh.bitangent.size(), firstVertex));
		mesh.texcoord.resize(firstVertex);
		mesh.color.resize(firstVertex);
		mesh.index.resize(firstVertex / 4 * 6);
		mesh.normal.resize(firstVertex, Vec3(0.f, 0.f, 1.f));
		mesh.tangent.resize(firstVertex, Vec3(1.f, 0.f, 0.f));
		mesh.bitangent.resize(firstVertex, Vec3(0.f, 1.f, 0.f));

		// Back as quads, because that is what the geometry is made of: a
		// polygon is a fan of triangles from its first corner, and a
		// triangle is a quad with its last two corners in the same place.
		for (size_t k = 0; k < kept.size(); k++)
		{
			const ClipPoly &p = kept[k];
			for (size_t i = 1; i + 1 < p.size(); i++)
			{
				const __INDEX_C_TYPE__ base = (__INDEX_C_TYPE__)mesh.vertex.size();
				const ClipVert* v[4] = { &p[0], &p[i], &p[i + 1], &p[i + 1] };
				for (int c = 0; c < 4; c++)
				{
					mesh.vertex.push_back(v[c]->p);
					mesh.texcoord.push_back(v[c]->uv);
					mesh.color.push_back(v[c]->c);
					mesh.normal.push_back(keptN[k]);
					mesh.tangent.push_back(keptT[k]);
					mesh.bitangent.push_back(keptB[k]);
				}
				mesh.index.push_back(base + 0); mesh.index.push_back(base + 1); mesh.index.push_back(base + 2);
				mesh.index.push_back(base + 2); mesh.index.push_back(base + 3); mesh.index.push_back(base + 0);
			}
		}

		// Bounds again, over everything: what was cut may have been the far edge.
		if (!mesh.vertex.empty())
		{
			mesh.minBounds = mesh.maxBounds = mesh.vertex[0];
			for (size_t i = 1; i < mesh.vertex.size(); i++)
			{
				const Vec3 &v = mesh.vertex[i];
				if (v.x < mesh.minBounds.x) mesh.minBounds.x = v.x;
				if (v.y < mesh.minBounds.y) mesh.minBounds.y = v.y;
				if (v.z < mesh.minBounds.z) mesh.minBounds.z = v.z;
				if (v.x > mesh.maxBounds.x) mesh.maxBounds.x = v.x;
				if (v.y > mesh.maxBounds.y) mesh.maxBounds.y = v.y;
				if (v.z > mesh.maxBounds.z) mesh.maxBounds.z = v.z;
			}
		}
		else mesh.minBounds = mesh.maxBounds = Vec3(0.f, 0.f, 0.f);
		return mesh.vertex.size() > firstVertex;
	}

	void SpriteMesh2D::Clear()
	{
		vertex.clear();
		normal.clear();
		tangent.clear();
		bitangent.clear();
		texcoord.clear();
		color.clear();
		index.clear();
		minBounds = maxBounds = Vec3(0.f, 0.f, 0.f);
	}

	Vec4 SpriteLocalRect2D(const SpriteSheet2D &sheet, const SpriteMeshParams2D &params)
	{
		if (!sheet.ValidFrame(params.frame)) return Vec4(0.f, 0.f, 0.f, 0.f);
		SpriteFrame2D f = sheet.frames[(size_t)params.frame];
		if (params.overridePivot) f.pivot = params.pivot;
		const f32 ppu = params.pixelsPerUnit > 0.f ? params.pixelsPerUnit : sheet.pixelsPerUnit;
		if (!(ppu > 0.f)) return Vec4(0.f, 0.f, 0.f, 0.f);

		f32 x0, y0, x1, y1;
		if (params.mode == SpriteDraw2D::Simple)
		{
			// The SOURCE rectangle, not the trimmed one: what a sprite covers
			// must not change from frame to frame because a packer cut a
			// different margin off each.
			const f32 sw = (f32)f.SourceW() / ppu, sh = (f32)f.SourceH() / ppu;
			x0 = -f.pivot.x * sw; x1 = x0 + sw;
			y0 = -f.pivot.y * sh; y1 = y0 + sh;
		}
		else
		{
			x0 = -f.pivot.x * params.size.x; x1 = x0 + params.size.x;
			y0 = -f.pivot.y * params.size.y; y1 = y0 + params.size.y;
		}
		if (params.flipX) { const f32 a = -x1, b = -x0; x0 = a; x1 = b; }
		if (params.flipY) { const f32 a = -y1, b = -y0; y0 = a; y1 = b; }
		return Vec4(x0, y0, x1, y1);
	}

	bool BuildSpriteMesh2D(const SpriteSheet2D &sheet, const SpriteMeshParams2D &params,
		SpriteMesh2D &out, const Matrix* transform)
	{
		if (!sheet.ValidFrame(params.frame) || !sheet.HaveImageSize()) return false;
		SpriteFrame2D f = sheet.frames[(size_t)params.frame];
		if (params.overridePivot) f.pivot = params.pivot;
		if (f.w < 1 || f.h < 1) return false;
		const f32 ppu = params.pixelsPerUnit > 0.f ? params.pixelsPerUnit : sheet.pixelsPerUnit;
		if (!(ppu > 0.f)) return false;

		const size_t firstVertex = out.vertex.size();
		const Vec4 uv = sheet.UVRect(params.frame);   // left, top, right, bottom
		const f32 iw = (f32)sheet.GetImageWidth(), ih = (f32)sheet.GetImageHeight();

		if (params.mode == SpriteDraw2D::Sliced && f.HasBorder())
		{
			const f32 W = params.size.x, H = params.size.y;
			if (!(W > 0.f) || !(H > 0.f)) return false;
			f32 bl = f.border.x / ppu, bb = f.border.y / ppu;
			f32 br = f.border.z / ppu, bt = f.border.w / ppu;
			// Drawn smaller than its own corners: shrink them together
			// rather than let them cross and turn the panel inside out.
			if (bl + br > W) { const f32 k = W / (bl + br); bl *= k; br *= k; }
			if (bb + bt > H) { const f32 k = H / (bb + bt); bb *= k; bt *= k; }

			const f32 left = -f.pivot.x * W, bottom = -f.pivot.y * H;
			const f32 xs[4] = { left, left + bl, left + W - br, left + W };
			const f32 ys[4] = { bottom, bottom + bb, bottom + H - bt, bottom + H };
			// Inner cuts sit exactly on the texel boundary: both sides of one
			// belong to this frame, so there is nothing to bleed in from.
			const f32 us[4] = { uv.x, ((f32)f.x + f.border.x) / iw,
				((f32)(f.x + f.w) - f.border.z) / iw, uv.z };
			const f32 vs[4] = { uv.w, ((f32)(f.y + f.h) - f.border.y) / ih,
				((f32)f.y + f.border.w) / ih, uv.y };
			for (int r = 0; r < 3; r++)
				for (int c = 0; c < 3; c++)
				{
					if (xs[c + 1] - xs[c] <= 0.f || ys[r + 1] - ys[r] <= 0.f) continue;
					PushQuad(out, xs[c], ys[r], xs[c + 1], ys[r + 1],
						us[c], vs[r], us[c + 1], vs[r + 1], params.color);
				}
		}
		else if (params.mode == SpriteDraw2D::Tiled)
		{
			const f32 W = params.size.x, H = params.size.y;
			if (!(W > 0.f) || !(H > 0.f)) return false;
			const f32 tw = (f32)f.w / ppu, th = (f32)f.h / ppu;
			const f32 left = -f.pivot.x * W, bottom = -f.pivot.y * H;
			const int32 nx = (int32)ceilf(W / tw - 1e-4f);
			const int32 ny = (int32)ceilf(H / th - 1e-4f);
			if (nx < 1 || ny < 1 || (int64)nx * ny > kMaxTiledQuads)
			{
				// Too many repeats to be meant: one stretched quad, which is
				// visibly wrong rather than invisibly expensive.
				PushQuad(out, left, bottom, left + W, bottom + H, uv.x, uv.w, uv.z, uv.y, params.color);
			}
			else
			{
				for (int32 ty = 0; ty < ny; ty++)
					for (int32 tx = 0; tx < nx; tx++)
					{
						const f32 x0 = left + (f32)tx * tw, y0 = bottom + (f32)ty * th;
						// The last column and row are cut short, and sample
						// only the part of the frame they show.
						const f32 fx = std::min(1.f, (W - (f32)tx * tw) / tw);
						const f32 fy = std::min(1.f, (H - (f32)ty * th) / th);
						const f32 u1 = uv.x + (uv.z - uv.x) * fx;
						const f32 vTop = uv.w + (uv.y - uv.w) * fy;
						PushQuad(out, x0, y0, x0 + tw * fx, y0 + th * fy,
							uv.x, uv.w, u1, vTop, params.color);
					}
			}
		}
		else if (params.mode == SpriteDraw2D::Sliced)
		{
			// Sliced with no borders set: there is nothing to keep unstretched,
			// so it is the frame stretched to the size.
			const f32 left = -f.pivot.x * params.size.x, bottom = -f.pivot.y * params.size.y;
			if (!(params.size.x > 0.f) || !(params.size.y > 0.f)) return false;
			PushQuad(out, left, bottom, left + params.size.x, bottom + params.size.y,
				uv.x, uv.w, uv.z, uv.y, params.color);
		}
		else
		{
			f32 x0, y0, x1, y1;
			SimpleRect(f, ppu, x0, y0, x1, y1);
			PushQuad(out, x0, y0, x1, y1, uv.x, uv.w, uv.z, uv.y, params.color);
		}

		// Flipping mirrors the finished quads about the pivot. The texture
		// coordinates travel with their vertices, so the picture mirrors and
		// a character facing left keeps its feet where they were.
		if (params.flipX || params.flipY)
			for (size_t i = firstVertex; i < out.vertex.size(); i++)
			{
				if (params.flipX) out.vertex[i].x = -out.vertex[i].x;
				if (params.flipY) out.vertex[i].y = -out.vertex[i].y;
			}

		if (transform)
			for (size_t i = firstVertex; i < out.vertex.size(); i++)
				out.vertex[i] = (*transform) * out.vertex[i];
		{
			// A flip mirrors the quad, and the texture with it: u then runs
			// the other way across the sprite, and a normal map's x with it.
			Vec3 T(params.flipX ? -1.f : 1.f, 0.f, 0.f), B(0.f, params.flipY ? -1.f : 1.f, 0.f);
			if (transform)
			{
				const Matrix &m = *transform;
				T = Vec3(m.m[0] * T.x + m.m[4] * T.y, m.m[1] * T.x + m.m[5] * T.y, m.m[2] * T.x + m.m[6] * T.y);
				B = Vec3(m.m[0] * B.x + m.m[4] * B.y, m.m[1] * B.x + m.m[5] * B.y, m.m[2] * B.x + m.m[6] * B.y);
				const f32 tl = T.magnitude(), bl = B.magnitude();
				if (tl > 1e-6f) T = T * (1.f / tl);
				if (bl > 1e-6f) B = B * (1.f / bl);
			}
			out.tangent.resize(out.vertex.size(), T);
			out.bitangent.resize(out.vertex.size(), B);
		}

		for (size_t i = firstVertex; i < out.vertex.size(); i++)
		{
			const Vec3 &v = out.vertex[i];
			if (i == 0) { out.minBounds = out.maxBounds = v; continue; }
			if (v.x < out.minBounds.x) out.minBounds.x = v.x;
			if (v.y < out.minBounds.y) out.minBounds.y = v.y;
			if (v.z < out.minBounds.z) out.minBounds.z = v.z;
			if (v.x > out.maxBounds.x) out.maxBounds.x = v.x;
			if (v.y > out.maxBounds.y) out.maxBounds.y = v.y;
			if (v.z > out.maxBounds.z) out.maxBounds.z = v.z;
		}
		return out.vertex.size() > firstVertex;
	}

}
