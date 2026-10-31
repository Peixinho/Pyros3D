//============================================================================
// Name        : BlockCompress.cpp
// Description : See BlockCompress.h.
//============================================================================

#include "BlockCompress.h"
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace p3d { namespace BlockCompress {

	namespace {
		inline uint16_t To565(const float r, const float g, const float b)
		{
			const int R = std::max(0, std::min(31, (int)std::lround(r * 31.f / 255.f)));
			const int G = std::max(0, std::min(63, (int)std::lround(g * 63.f / 255.f)));
			const int B = std::max(0, std::min(31, (int)std::lround(b * 31.f / 255.f)));
			return (uint16_t)((R << 11) | (G << 5) | B);
		}
		// (as the hardware expands it: the top bits repeated into the low ones)
		inline void From565(const uint16_t c, int out[3])
		{
			const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
			out[0] = (r << 3) | (r >> 2); out[1] = (g << 2) | (g >> 4); out[2] = (b << 3) | (b >> 2);
		}
		void Palette(const uint16_t c0, const uint16_t c1, int pal[4][3])
		{
			From565(c0, pal[0]); From565(c1, pal[1]);
			for (int k = 0; k < 3; k++)
			{
				if (c0 > c1)
				{
					pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3;
					pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3;
				}
				else
				{
					pal[2][k] = (pal[0][k] + pal[1][k]) / 2;
					pal[3][k] = 0;
				}
			}
		}
		// Nearest of the four palette colours for each texel; the total squared error.
		int Assign(const uint8_t rgba[64], const int pal[4][3], const int count, uint32_t &indices)
		{
			int err = 0; indices = 0;
			for (int i = 0; i < 16; i++)
			{
				int best = 0, bestD = 1 << 30;
				for (int p = 0; p < count; p++)
				{
					const int dr = rgba[i * 4] - pal[p][0], dg = rgba[i * 4 + 1] - pal[p][1], db = rgba[i * 4 + 2] - pal[p][2];
					const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;      // (green weighs most, as the eye does)
					if (d < bestD) { bestD = d; best = p; }
				}
				indices |= (uint32_t)best << (i * 2);
				err += bestD;
			}
			return err;
		}
		// Endpoints c0 > c1 (four-colour mode) from two colours; swapped indices if needed.
		void Pack(float a[3], float b[3], const uint8_t rgba[64], uint8_t out[8], int &errOut)
		{
			uint16_t c0 = To565(a[0], a[1], a[2]), c1 = To565(b[0], b[1], b[2]);
			if (c0 < c1) std::swap(c0, c1);
			uint32_t idx = 0;
			int err;
			if (c0 == c1)
			{
				// one colour: every texel index 0 (c0 > c1 needed for 4 colours; with equal ones
				// the 3-colour mode's index 0 is still c0)
				int pal[4][3]; Palette(c0, c1, pal);
				err = 0;
				for (int i = 0; i < 16; i++)
				{
					const int dr = rgba[i * 4] - pal[0][0], dg = rgba[i * 4 + 1] - pal[0][1], db = rgba[i * 4 + 2] - pal[0][2];
					err += dr * dr * 3 + dg * dg * 4 + db * db * 2;
				}
			}
			else
			{
				int pal[4][3]; Palette(c0, c1, pal);
				err = Assign(rgba, pal, 4, idx);
			}
			out[0] = (uint8_t)(c0 & 0xFF); out[1] = (uint8_t)(c0 >> 8);
			out[2] = (uint8_t)(c1 & 0xFF); out[3] = (uint8_t)(c1 >> 8);
			out[4] = (uint8_t)(idx & 0xFF); out[5] = (uint8_t)((idx >> 8) & 0xFF); out[6] = (uint8_t)((idx >> 16) & 0xFF); out[7] = (uint8_t)(idx >> 24);
			errOut = err;
		}

		void EncodeChannelBlock(const uint8_t rgba[64], const int ch, uint8_t out[8])
		{
			int lo = 255, hi = 0;
			for (int i = 0; i < 16; i++) { lo = std::min(lo, (int)rgba[i * 4 + ch]); hi = std::max(hi, (int)rgba[i * 4 + ch]); }
			out[0] = (uint8_t)hi; out[1] = (uint8_t)lo;              // a0 > a1: eight values between them
			int pal[8];
			pal[0] = hi; pal[1] = lo;
			for (int k = 1; k < 7; k++) pal[k + 1] = ((7 - k) * hi + k * lo) / 7;
			uint64_t bits = 0;
			for (int i = 0; i < 16; i++)
			{
				const int a = rgba[i * 4 + ch];
				int best = 0, bestD = 1 << 30;
				for (int p = 0; p < 8; p++) { const int d = std::abs(a - pal[p]); if (d < bestD) { bestD = d; best = p; } }
				if (hi == lo) best = 0;
				bits |= (uint64_t)best << (i * 3);
			}
			for (int b = 0; b < 6; b++) out[2 + b] = (uint8_t)((bits >> (b * 8)) & 0xFF);
		}
	}

	void EncodeBlockBC1(const uint8_t rgba[64], uint8_t out[8])
	{
		// the principal axis of the block's colours (power iteration on their covariance)
		float mean[3] = { 0, 0, 0 };
		for (int i = 0; i < 16; i++) for (int k = 0; k < 3; k++) mean[k] += rgba[i * 4 + k];
		for (int k = 0; k < 3; k++) mean[k] /= 16.f;
		float cov[6] = { 0, 0, 0, 0, 0, 0 };
		for (int i = 0; i < 16; i++)
		{
			const float r = rgba[i * 4] - mean[0], g = rgba[i * 4 + 1] - mean[1], b = rgba[i * 4 + 2] - mean[2];
			cov[0] += r * r; cov[1] += r * g; cov[2] += r * b; cov[3] += g * g; cov[4] += g * b; cov[5] += b * b;
		}
		float axis[3] = { 0.97f, 0.98f, 0.99f };
		for (int it = 0; it < 6; it++)
		{
			const float x = cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2];
			const float y = cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2];
			const float z = cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2];
			const float m = std::max(std::fabs(x), std::max(std::fabs(y), std::fabs(z)));
			if (m < 1e-6f) break;
			axis[0] = x / m; axis[1] = y / m; axis[2] = z / m;
		}
		float tmin = 1e30f, tmax = -1e30f;
		for (int i = 0; i < 16; i++)
		{
			const float t = (rgba[i * 4] - mean[0]) * axis[0] + (rgba[i * 4 + 1] - mean[1]) * axis[1] + (rgba[i * 4 + 2] - mean[2]) * axis[2];
			tmin = std::min(tmin, t); tmax = std::max(tmax, t);
		}
		const float l2 = axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2];
		float a[3], b[3];
		for (int k = 0; k < 3; k++)
		{
			a[k] = std::max(0.f, std::min(255.f, mean[k] + axis[k] * tmax / (l2 > 1e-8f ? l2 : 1.f)));
			b[k] = std::max(0.f, std::min(255.f, mean[k] + axis[k] * tmin / (l2 > 1e-8f ? l2 : 1.f)));
		}
		int err = 0;
		Pack(a, b, rgba, out, err);
		if (err == 0) return;

		// one least-squares refit: the endpoints that best explain the indices chosen
		const uint32_t idx = (uint32_t)out[4] | ((uint32_t)out[5] << 8) | ((uint32_t)out[6] << 16) | ((uint32_t)out[7] << 24);
		const uint16_t c0 = (uint16_t)(out[0] | (out[1] << 8)), c1 = (uint16_t)(out[2] | (out[3] << 8));
		if (c0 <= c1) return;
		static const float w0[4] = { 1.f, 0.f, 2.f / 3.f, 1.f / 3.f };
		float aa = 0, bb = 0, ab = 0, ax[3] = { 0, 0, 0 }, bx[3] = { 0, 0, 0 };
		for (int i = 0; i < 16; i++)
		{
			const int s = (idx >> (i * 2)) & 3;
			const float p = w0[s], q = 1.f - p;
			aa += p * p; bb += q * q; ab += p * q;
			for (int k = 0; k < 3; k++) { ax[k] += p * rgba[i * 4 + k]; bx[k] += q * rgba[i * 4 + k]; }
		}
		const float det = aa * bb - ab * ab;
		if (std::fabs(det) < 1e-6f) return;
		float na[3], nb[3];
		for (int k = 0; k < 3; k++)
		{
			na[k] = std::max(0.f, std::min(255.f, (ax[k] * bb - bx[k] * ab) / det));
			nb[k] = std::max(0.f, std::min(255.f, (bx[k] * aa - ax[k] * ab) / det));
		}
		uint8_t trial[8]; int err2 = 0;
		Pack(na, nb, rgba, trial, err2);
		if (err2 < err) std::memcpy(out, trial, 8);
	}

	void EncodeBlockBC3(const uint8_t rgba[64], uint8_t out[16])
	{
		EncodeChannelBlock(rgba, 3, out);
		EncodeBlockBC1(rgba, out + 8);
		// (BC3's colour block is always read as four colours: keep c0 > c1 - Pack did - and
		// a block of one colour, with its equal endpoints, reads every index as c0 still)
	}

	void EncodeBlockBC5(const uint8_t rgba[64], uint8_t out[16])
	{
		EncodeChannelBlock(rgba, 0, out);
		EncodeChannelBlock(rgba, 1, out + 8);
	}

	void DecodeBlockBC5(const uint8_t in[16], uint8_t rgba[64])
	{
		for (int c = 0; c < 2; c++)
		{
			const uint8_t* b = in + c * 8;
			const int a0 = b[0], a1 = b[1];
			int ap[8]; ap[0] = a0; ap[1] = a1;
			if (a0 > a1) for (int k = 1; k < 7; k++) ap[k + 1] = ((7 - k) * a0 + k * a1) / 7;
			else { for (int k = 1; k < 5; k++) ap[k + 1] = ((5 - k) * a0 + k * a1) / 5; ap[6] = 0; ap[7] = 255; }
			uint64_t bits = 0;
			for (int k = 0; k < 6; k++) bits |= (uint64_t)b[2 + k] << (k * 8);
			for (int i = 0; i < 16; i++) rgba[i * 4 + c] = (uint8_t)ap[(bits >> (i * 3)) & 7];
		}
		for (int i = 0; i < 16; i++) { rgba[i * 4 + 2] = 0; rgba[i * 4 + 3] = 255; }
	}

	void DecodeBlockBC1(const uint8_t in[8], uint8_t rgba[64])
	{
		const uint16_t c0 = (uint16_t)(in[0] | (in[1] << 8)), c1 = (uint16_t)(in[2] | (in[3] << 8));
		int pal[4][3]; Palette(c0, c1, pal);
		const uint32_t idx = (uint32_t)in[4] | ((uint32_t)in[5] << 8) | ((uint32_t)in[6] << 16) | ((uint32_t)in[7] << 24);
		for (int i = 0; i < 16; i++)
		{
			const int s = (idx >> (i * 2)) & 3;
			rgba[i * 4] = (uint8_t)pal[s][0]; rgba[i * 4 + 1] = (uint8_t)pal[s][1]; rgba[i * 4 + 2] = (uint8_t)pal[s][2];
			rgba[i * 4 + 3] = (c0 <= c1 && s == 3) ? 0 : 255;
		}
	}

	void DecodeBlockBC3(const uint8_t in[16], uint8_t rgba[64])
	{
		const uint8_t* c = in + 8;
		const uint16_t c0 = (uint16_t)(c[0] | (c[1] << 8)), c1 = (uint16_t)(c[2] | (c[3] << 8));
		int pal[4][3]; From565(c0, pal[0]); From565(c1, pal[1]);
		for (int k = 0; k < 3; k++) { pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3; pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3; }
		const uint32_t idx = (uint32_t)c[4] | ((uint32_t)c[5] << 8) | ((uint32_t)c[6] << 16) | ((uint32_t)c[7] << 24);
		const int a0 = in[0], a1 = in[1];
		int ap[8]; ap[0] = a0; ap[1] = a1;
		if (a0 > a1) for (int k = 1; k < 7; k++) ap[k + 1] = ((7 - k) * a0 + k * a1) / 7;
		else { for (int k = 1; k < 5; k++) ap[k + 1] = ((5 - k) * a0 + k * a1) / 5; ap[6] = 0; ap[7] = 255; }
		uint64_t bits = 0;
		for (int b = 0; b < 6; b++) bits |= (uint64_t)in[2 + b] << (b * 8);
		for (int i = 0; i < 16; i++)
		{
			const int s = (idx >> (i * 2)) & 3;
			rgba[i * 4] = (uint8_t)pal[s][0]; rgba[i * 4 + 1] = (uint8_t)pal[s][1]; rgba[i * 4 + 2] = (uint8_t)pal[s][2];
			rgba[i * 4 + 3] = (uint8_t)ap[(bits >> (i * 3)) & 7];
		}
	}

	void EncodeImage(const uint32_t format, const uint8_t* rgba, const uint32_t w, const uint32_t h, std::vector<uint8_t> &out)
	{
		const uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4, bytes = BlockBytes(format);
		out.assign((size_t)bw * bh * bytes, 0);
		uint8_t* dst = out.data();
		JobSystem::Instance().ParallelFor(bh, 8, [&](uint32_t begin, uint32_t end) {
			uint8_t block[64];
			for (uint32_t by = begin; by < end; by++)
				for (uint32_t bx = 0; bx < bw; bx++)
				{
					for (uint32_t y = 0; y < 4; y++)
						for (uint32_t x = 0; x < 4; x++)
						{
							const uint32_t sx = std::min(w - 1, bx * 4 + x), sy = std::min(h - 1, by * 4 + y);
							std::memcpy(&block[(y * 4 + x) * 4], &rgba[((size_t)sy * w + sx) * 4], 4);
						}
					uint8_t* o = dst + ((size_t)by * bw + bx) * bytes;
					if (format == BC1) EncodeBlockBC1(block, o); else if (format == BC5) EncodeBlockBC5(block, o); else EncodeBlockBC3(block, o);
				}
		});
	}

	namespace {
		// A cut-out: alpha that is nearly all fully clear or fully solid - leaves, grass, needles,
		// a fence - drawn with an alpha test, not blended
		bool IsCutout(const std::vector<uint8_t> &rgba)
		{
			const size_t texels = rgba.size() / 4;
			if (!texels) return false;
			const size_t step = std::max<size_t>(1, texels / 65536);
			size_t looked = 0, hard = 0, clear = 0;
			for (size_t t = 0; t < texels; t += step, looked++)
			{
				const uint8_t a = rgba[t * 4 + 3];
				if (a <= 8 || a >= 247) hard++;
				if (a <= 8) clear++;
			}
			// (four fifths hard, not nine tenths: a bough painted with soft edges - the spruces' - is still a
			// cut-out, and averaged plainly its needles faded out of the far levels and the coarse shadow maps)
			return clear > looked / 20 && hard >= looked * 4 / 5;
		}
		// The share of texels at or past the cut
		float Coverage(const std::vector<uint8_t> &rgba, const float scale, const int cut)
		{
			const size_t texels = rgba.size() / 4;
			size_t in = 0;
			for (size_t t = 0; t < texels; t++) if (rgba[t * 4 + 3] * scale >= (float)cut) in++;
			return texels ? (float)in / (float)texels : 0.f;
		}
	}

	bool IsColourPicture(const uint32_t format, const char* name)
	{
		if (format == BC5) return false;
		if (!name) return true;
		std::string n(name);
		std::transform(n.begin(), n.end(), n.begin(), ::tolower);
		const size_t slash = n.find_last_of("/\\");
		if (slash != std::string::npos) n = n.substr(slash + 1);
		static const char* kData[] = { "nor", "nrm", "normal", "rough", "metal", "orm", "arm", "_ao", "occlusion", "mask", "height", "disp", "bump", "spec", "splat", "density", "macro" };
		for (const char* k : kData) if (n.find(k) != std::string::npos) return false;
		return true;
	}

	namespace {
		// sRGB <-> linear for the colour mips (ColourSpace.h's curve): a table one way, a search the other
		struct SrgbTables
		{
			float toLinear[256];
			SrgbTables()
			{
				for (int i = 0; i < 256; i++)
				{
					const float c = i / 255.f;
					toLinear[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
				}
			}
			uint8_t Encode(const float l) const
			{
				// the nearest code: binary search on the monotonic table, then the closer neighbour
				int lo = 0, hi = 255;
				while (lo < hi) { const int mid = (lo + hi) / 2; if (toLinear[mid] < l) lo = mid + 1; else hi = mid; }
				if (lo > 0 && (l - toLinear[lo - 1]) < (toLinear[lo] - l)) lo--;
				return (uint8_t)lo;
			}
		};
		const SrgbTables &Srgb() { static const SrgbTables t; return t; }
	}

	void EncodeMipChain(const uint32_t format, const std::vector<uint8_t> &rgba, const uint32_t w, const uint32_t h,
		std::vector<std::vector<uint8_t> > &levels, const bool srgb)
	{
		const SrgbTables &st = Srgb();
		levels.clear();
		std::vector<uint8_t> cur = rgba, next;
		uint32_t cw = w, ch = h;
		// A cut-out's smaller levels: each texel's colour the average of its four by how solid each is
		// (the colour under a clear texel is not what is seen), and the alpha scaled so as many texels
		// pass the cut as in the full picture. Averaged plainly, a grass blade or a needle half a texel
		// wide went to an alpha of a quarter - under the cut, gone - or, where they were many, every
		// texel of the clump passed at once in the dark of the gaps, and a tree far off was a black blob.
		const bool cutout = IsCutout(rgba);
		const int cut = 115;                  // (about the 0.4 - 0.45 the leaf materials cut at)
		const float want = cutout ? Coverage(rgba, 1.f, cut) : 0.f;
		while (true)
		{
			levels.push_back(std::vector<uint8_t>());
			EncodeImage(format, cur.data(), cw, ch, levels.back());
			if (cw == 1 && ch == 1) break;
			const uint32_t nw = std::max(1u, cw / 2), nh = std::max(1u, ch / 2);
			next.assign((size_t)nw * nh * 4, 0);
			for (uint32_t y = 0; y < nh; y++)
				for (uint32_t x = 0; x < nw; x++)
				{
					const uint32_t x0 = std::min(cw - 1, x * 2), x1 = std::min(cw - 1, x * 2 + 1);
					const uint32_t y0 = std::min(ch - 1, y * 2), y1 = std::min(ch - 1, y * 2 + 1);
					const size_t q[4] = { ((size_t)y0 * cw + x0) * 4, ((size_t)y0 * cw + x1) * 4, ((size_t)y1 * cw + x0) * 4, ((size_t)y1 * cw + x1) * 4 };
					const size_t o = ((size_t)y * nw + x) * 4;
					const int as = cur[q[0] + 3] + cur[q[1] + 3] + cur[q[2] + 3] + cur[q[3] + 3];
					for (int k = 0; k < 3; k++)
					{
						if (srgb)
						{
							// as light: decoded, averaged (by how solid each is, for a cut-out), encoded
							float sum = 0.f, wsum = 0.f;
							for (int t = 0; t < 4; t++)
							{
								const float wt = (cutout && as > 0) ? (float)cur[q[t] + 3] : 1.f;
								sum += st.toLinear[cur[q[t] + k]] * wt; wsum += wt;
							}
							next[o + k] = st.Encode(wsum > 0.f ? sum / wsum : 0.f);
						}
						else if (cutout && as > 0)
						{
							const int s = cur[q[0] + k] * cur[q[0] + 3] + cur[q[1] + k] * cur[q[1] + 3] + cur[q[2] + k] * cur[q[2] + 3] + cur[q[3] + k] * cur[q[3] + 3];
							next[o + k] = (uint8_t)std::min(255, (s + as / 2) / as);
						}
						else
						{
							const int s = cur[q[0] + k] + cur[q[1] + k] + cur[q[2] + k] + cur[q[3] + k];
							next[o + k] = (uint8_t)((s + 2) / 4);
						}
					}
					next[o + 3] = (uint8_t)((as + 2) / 4);
				}
			if (cutout && want > 0.f)
			{
				// the scale that gives the full picture's coverage (it only grows: a smaller level
				// averages solid with clear, never the other way)
				float lo = 1.f, hi = 8.f;
				for (int it = 0; it < 12; it++)
				{
					const float mid = 0.5f * (lo + hi);
					if (Coverage(next, mid, cut) < want) lo = mid; else hi = mid;
				}
				for (size_t t = 3; t < next.size(); t += 4) next[t] = (uint8_t)std::min(255.f, next[t] * hi + 0.5f);
			}
			cur.swap(next); cw = nw; ch = nh;
		}
	}

	uint32_t ChooseFormat(const std::vector<uint8_t> &rgba, const char* name)
	{
		bool opaque = true;
		for (size_t i = 3; i < rgba.size(); i += 4) if (rgba[i] != 255) { opaque = false; break; }
		if (!opaque) return BC3;
		// a normal map?
		const size_t texels = rgba.size() / 4;
		const size_t step = std::max<size_t>(1, texels / 8192);
		size_t looked = 0, unit = 0;
		for (size_t t = 0; t < texels; t += step, looked++)
		{
			const float x = rgba[t * 4] / 127.5f - 1.f, y = rgba[t * 4 + 1] / 127.5f - 1.f, z = rgba[t * 4 + 2] / 127.5f - 1.f;
			const float l = std::sqrt(x * x + y * y + z * z);
			if (z > 0.f && l > 0.88f && l < 1.12f) unit++;
		}
		bool named = false;
		if (name)
		{
			std::string n(name);
			std::transform(n.begin(), n.end(), n.begin(), ::tolower);
			const size_t slash = n.find_last_of("/\\");
			if (slash != std::string::npos) n = n.substr(slash + 1);
			named = n.find("nor") != std::string::npos || n.find("nrm") != std::string::npos;
		}
		const float share = looked ? (float)unit / (float)looked : 0.f;
		if (share >= 0.995f || (named && share >= 0.97f)) return BC5;
		return BC1;
	}

} }
