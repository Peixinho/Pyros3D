//============================================================================
// Name        : WritePng.h
// Author      : Duarte Peixinho
// Description : The device layer's captures (PYROS_SHOT): the frame read back, as a PNG
//============================================================================

#ifndef D2WRITEPNG_H
#define D2WRITEPNG_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace p3d { namespace gfx {

	// A PNG of RGBA8 rows: stored (uncompressed) deflate - a test's capture (PYROS_SHOT), not a picture to keep
	inline void WritePng(const char* path, const uint8_t* rgba, uint32_t w, uint32_t h)
	{
		std::vector<uint8_t> raw; raw.reserve((size_t)(w * 4 + 1) * h);
		for (uint32_t y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgba + (size_t)y * w * 4, rgba + (size_t)(y + 1) * w * 4); }
		std::vector<uint8_t> z = { 0x78, 0x01 };
		uint32_t a = 1, b = 0;
		for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
		for (size_t at = 0; at < raw.size(); at += 65535)
		{
			const uint32_t n = (uint32_t)std::min<size_t>(65535, raw.size() - at);
			const bool last = at + n >= raw.size();
			z.push_back(last ? 1 : 0); z.push_back(n & 255); z.push_back((n >> 8) & 255); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
			z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
		}
		const uint32_t adler = (b << 16) | a;
		for (int i = 3; i >= 0; i--) z.push_back((adler >> (i * 8)) & 255);
		static uint32_t table[256]; static bool made = false;
		if (!made) { for (uint32_t n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[n] = c; } made = true; }
		FILE* f = fopen(path, "wb");
		if (!f) return;
		auto chunk = [&](const char* type, const std::vector<uint8_t> &data) {
			const uint32_t len = (uint32_t)data.size();
			const uint8_t lb[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len };
			fwrite(lb, 1, 4, f); fwrite(type, 1, 4, f); if (len) fwrite(data.data(), 1, len, f);
			uint32_t crc = 0xFFFFFFFFu;
			for (int i = 0; i < 4; i++) crc = table[(crc ^ (uint8_t)type[i]) & 255] ^ (crc >> 8);
			for (uint8_t c : data) crc = table[(crc ^ c) & 255] ^ (crc >> 8);
			crc ^= 0xFFFFFFFFu;
			const uint8_t cb[4] = { (uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc };
			fwrite(cb, 1, 4, f);
		};
		static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
		fwrite(sig, 1, 8, f);
		chunk("IHDR", { (uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w, (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 6, 0, 0, 0 });
		chunk("IDAT", z);
		chunk("IEND", {});
		fclose(f);
	}

} }

#endif /* D2WRITEPNG_H */
