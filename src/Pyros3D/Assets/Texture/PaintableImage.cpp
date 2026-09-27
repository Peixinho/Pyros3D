//============================================================================
// Name        : PaintableImage.cpp
// Author      : Duarte Peixinho
// Description : See PaintableImage.h.
//============================================================================

#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/File/File.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/stb/stb_image.h>

// Private to this file: the editor links its own copy of stb_image_write,
// and STATIC keeps the two from colliding.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <Pyros3D/Ext/stb/stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace p3d {

	void PaintableImage::Create(const int32 w, const int32 h, const uint32 ch, const uchar fill)
	{
		width = w; height = h; channels = ch;
		pixels.assign((size_t)w * h * ch, fill);
	}

	bool PaintableImage::Load(const std::string &path, const uint32 ch)
	{
		File file;
		if (!file.Open(path) || file.Size() == 0) return false;
		int w = 0, h = 0, c = 0;
		stbi_uc* px = stbi_load_from_memory(&file.GetData()[0], (int)file.Size(), &w, &h, &c, (int)ch);
		file.Close();
		if (!px) { echo("ERROR: PaintableImage - could not decode " + path); return false; }
		width = w; height = h; channels = ch;
		pixels.assign(px, px + (size_t)w * h * ch);
		stbi_image_free(px);
		return true;
	}

	bool PaintableImage::Save(const std::string &path) const
	{
		return WritePNG(path, width, height, channels, pixels.empty() ? NULL : &pixels[0]);
	}

	f32 PaintableImage::Sample(const f32 u, const f32 v, const uint32 channel) const
	{
		if (width <= 0 || height <= 0 || channel >= channels) return 0.f;
		const f32 fx = std::min(std::max(u, 0.f), 1.f) * (width - 1);
		const f32 fy = std::min(std::max(v, 0.f), 1.f) * (height - 1);
		const int32 x0 = std::min((int32)fx, std::max(width - 2, 0)), y0 = std::min((int32)fy, std::max(height - 2, 0));
		const int32 x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
		const f32 a = fx - x0, b = fy - y0;
		const uchar* p = &pixels[0];
		const f32 top = p[((size_t)y0 * width + x0) * channels + channel] * (1 - a) + p[((size_t)y0 * width + x1) * channels + channel] * a;
		const f32 bot = p[((size_t)y1 * width + x0) * channels + channel] * (1 - a) + p[((size_t)y1 * width + x1) * channels + channel] * a;
		return (top * (1 - b) + bot * b) / 255.f;
	}

	f32 PaintableImage::Falloff(const f32 d, const f32 hardness)
	{
		if (d >= 1.f) return 0.f;
		const f32 h = std::min(std::max(hardness, 0.f), 1.f);
		if (d <= h) return 1.f;
		const f32 t = (d - h) / (1.f - h);
		return 1.f - t * t * (3.f - 2.f * t);	// smoothstep down to the rim
	}

	bool PaintableImage::Paint(const f32 u, const f32 v, const f32 radiusU, const f32 radiusV, const uint32 channel,
		const f32 target, const f32 strength, const f32 hardness, const bool normalize)
	{
		if (width <= 0 || channel >= channels || radiusU <= 0.f || radiusV <= 0.f) return false;
		// Pixel x sits at u = x / (width - 1): the edge pixels lie ON the
		// tile's edges, as heightmap samples do, so a stroke across a border
		// paints both tiles' shared pixels alike.
		const int32 x0 = std::max(0, (int32)std::floor((u - radiusU) * (width - 1)));
		const int32 x1 = std::min(width - 1, (int32)std::ceil((u + radiusU) * (width - 1)));
		const int32 y0 = std::max(0, (int32)std::floor((v - radiusV) * (height - 1)));
		const int32 y1 = std::min(height - 1, (int32)std::ceil((v + radiusV) * (height - 1)));
		bool changed = false;
		for (int32 y = y0; y <= y1; y++)
			for (int32 x = x0; x <= x1; x++)
			{
				const f32 du = ((f32)x / std::max(width - 1, 1) - u) / radiusU;
				const f32 dv = ((f32)y / std::max(height - 1, 1) - v) / radiusV;
				const f32 w = Falloff(std::sqrt(du * du + dv * dv), hardness) * strength;
				if (w <= 0.f) continue;
				uchar* px = &pixels[((size_t)y * width + x) * channels];
				const f32 old = px[channel] / 255.f;
				const f32 now = old + (target - old) * std::min(w, 1.f);
				const uchar q = (uchar)std::lround(std::min(std::max(now, 0.f), 1.f) * 255.f);
				if (q == px[channel]) continue;
				px[channel] = q;
				changed = true;
				if (normalize && channels == 4)
				{
					// The rest share what is left, keeping their ratios.
					const int32 rest = 255 - q;
					int32 others = 0;
					for (uint32 c = 0; c < 4; c++) if (c != channel) others += px[c];
					int32 given = 0, last = -1;
					for (uint32 c = 0; c < 4; c++)
					{
						if (c == channel) continue;
						const int32 share = others > 0 ? (px[c] * rest) / others : rest / 3;
						px[c] = (uchar)share; given += share; last = (int32)c;
					}
					if (last >= 0) px[last] = (uchar)std::min(255, px[last] + (rest - given));	// rounding
				}
			}
		return changed;
	}

	void PaintableImage::Upload()
	{
		if (texture && channels == 4 && !pixels.empty()) texture->UpdateData(&pixels[0]);
	}

	bool PaintableImage::WritePNG(const std::string &path, const int32 w, const int32 h, const uint32 ch, const uchar* data)
	{
		if (!data || w <= 0 || h <= 0 || ch < 1 || ch > 4) return false;
		if (!stbi_write_png(path.c_str(), w, h, (int)ch, data, w * (int)ch))
		{
			echo("ERROR: PaintableImage - could not write " + path);
			return false;
		}
		return true;
	}

	namespace {
		uint32 Crc32(const uchar* d, size_t n, uint32 crc = 0)
		{
			crc = ~crc;
			for (size_t i = 0; i < n; i++)
			{
				crc ^= d[i];
				for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
			return ~crc;
		}
		void Be32(std::vector<uchar> &o, const uint32 v)
		{
			o.push_back((uchar)(v >> 24)); o.push_back((uchar)(v >> 16)); o.push_back((uchar)(v >> 8)); o.push_back((uchar)v);
		}
		void Chunk(std::vector<uchar> &o, const char* type, const uchar* data, const size_t n)
		{
			Be32(o, (uint32)n);
			const size_t start = o.size();
			o.insert(o.end(), type, type + 4);
			if (n) o.insert(o.end(), data, data + n);
			Be32(o, Crc32(&o[start], n + 4));
		}
	}

	bool PaintableImage::WritePNG16(const std::string &path, const int32 w, const int32 h, const uint16* data)
	{
		if (!data || w <= 0 || h <= 0) return false;
		// Filter byte 0 per row, samples big-endian - PNG's own layout.
		std::vector<uchar> raw((size_t)h * (1 + (size_t)w * 2));
		for (int32 y = 0; y < h; y++)
		{
			uchar* row = &raw[(size_t)y * (1 + (size_t)w * 2)];
			row[0] = 0;
			for (int32 x = 0; x < w; x++)
			{
				const uint16 s = data[(size_t)y * w + x];
				row[1 + x * 2] = (uchar)(s >> 8);
				row[2 + x * 2] = (uchar)(s & 0xFF);
			}
		}
		int zlen = 0;
		uchar* z = stbi_zlib_compress(&raw[0], (int)raw.size(), &zlen, 8);
		if (!z) return false;
		std::vector<uchar> png;
		const uchar sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
		png.insert(png.end(), sig, sig + 8);
		std::vector<uchar> ihdr;
		Be32(ihdr, (uint32)w); Be32(ihdr, (uint32)h);
		ihdr.push_back(16); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
		Chunk(png, "IHDR", &ihdr[0], ihdr.size());
		Chunk(png, "IDAT", z, (size_t)zlen);
		Chunk(png, "IEND", NULL, 0);
		STBIW_FREE(z);
		FILE* f = fopen(path.c_str(), "wb");
		if (!f) { echo("ERROR: PaintableImage - could not write " + path); return false; }
		const bool ok = fwrite(&png[0], 1, png.size(), f) == png.size();
		fclose(f);
		return ok;
	}

}
