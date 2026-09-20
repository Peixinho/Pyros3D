//============================================================================
// Name        : Octahedral.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See Octahedral.h.
//============================================================================

#include <Pyros3D/Rendering/GI/Octahedral.h>
#include <cmath>

namespace p3d {

	namespace {
		inline f32 SignNotZero(const f32 v) { return (v >= 0.f) ? 1.f : -1.f; }
	}

	Vec2 OctEncode(const Vec3 &direction)
	{
		// Project onto the octahedron |x|+|y|+|z| = 1, then unfold the
		// lower hemisphere outwards into the corners of the square.
		const f32 l1 = fabsf(direction.x) + fabsf(direction.y) + fabsf(direction.z);
		if (l1 < 1e-20f)
			return Vec2(0.f, 0.f);
		const f32 invL1 = 1.f / l1;
		f32 x = direction.x * invL1;
		f32 y = direction.y * invL1;
		const f32 z = direction.z * invL1;
		if (z < 0.f)
		{
			const f32 oldX = x;
			x = (1.f - fabsf(y)) * SignNotZero(oldX);
			y = (1.f - fabsf(oldX)) * SignNotZero(y);
		}
		return Vec2(x, y);
	}

	Vec3 OctDecode(const Vec2 &oct)
	{
		Vec3 v(oct.x, oct.y, 1.f - fabsf(oct.x) - fabsf(oct.y));
		if (v.z < 0.f)
		{
			const f32 oldX = v.x;
			v.x = (1.f - fabsf(v.y)) * SignNotZero(oldX);
			v.y = (1.f - fabsf(oldX)) * SignNotZero(v.y);
		}
		return v.normalize();
	}

	bool ProbeAtlas::Allocate(const uint32 probeCount, const uint32 res, const uint32 chans)
	{
		Clear();
		if (probeCount == 0 || res == 0 || chans == 0 || chans > 4)
			return false;

		resolution = res;
		channels = chans;
		// Roughly square atlas. Keeping it near-square rather than one
		// long strip matters because GPUs cap texture dimensions
		// (commonly 16384), and a thousand probes in a row would exceed
		// that long before the pixel count did.
		probesX = (uint32)ceilf(sqrtf((f32)probeCount));
		if (probesX == 0) probesX = 1;
		probesY = (probeCount + probesX - 1) / probesX;

		data.assign((size_t)GetWidth() * GetHeight() * channels, 0.f);
		return true;
	}

	void ProbeAtlas::Clear()
	{
		resolution = channels = probesX = probesY = 0;
		data.clear();
	}

	f32 *ProbeAtlas::TexelAt(const uint32 probe, const int32 x, const int32 y)
	{
		// x and y include the border, so they run [0, tile).
		const uint32 tile = GetTileSize();
		const uint32 px = probe % probesX;
		const uint32 py = probe / probesX;
		if (py >= probesY) return NULL;
		const uint32 ax = px * tile + (uint32)x;
		const uint32 ay = py * tile + (uint32)y;
		return &data[((size_t)ay * GetWidth() + ax) * channels];
	}

	f32 *ProbeAtlas::At(const uint32 probe, const uint32 tx, const uint32 ty)
	{
		return TexelAt(probe, (int32)tx + 1, (int32)ty + 1);
	}

	const f32 *ProbeAtlas::At(const uint32 probe, const uint32 tx, const uint32 ty) const
	{
		return const_cast<ProbeAtlas*>(this)->At(probe, tx, ty);
	}

	Vec3 ProbeAtlas::TexelDirection(const uint32 tx, const uint32 ty, const uint32 res)
	{
		// Texel centre, mapped to [-1, 1]. The +0.5 is what makes this
		// the centre rather than the corner; without it every probe is
		// biased by half a texel, which is small, systematic, and looks
		// like a slightly rotated environment.
		const f32 u = ((f32)tx + 0.5f) / (f32)res * 2.f - 1.f;
		const f32 v = ((f32)ty + 0.5f) / (f32)res * 2.f - 1.f;
		return OctDecode(Vec2(u, v));
	}

	void ProbeAtlas::FillBorders()
	{
		if (resolution == 0)
			return;
		const int32 R = (int32)resolution;
		const uint32 probeCount = probesX * probesY;

		for (uint32 p = 0; p < probeCount; p++)
		{
			// Edges. An octahedral map's left edge continues into the
			// left edge REVERSED - not into the right edge - because
			// crossing x = -1 folds back across the same hemisphere
			// boundary. Same for the other three. This is the part that
			// no texture wrap mode can do and that everyone gets wrong
			// once.
			for (int32 i = 0; i < R; i++)
			{
				const int32 mirror = R - 1 - i;
				// left border column (x = 0) <- interior column 0, flipped in y
				const f32 *src = At(p, 0, (uint32)mirror);
				f32 *dst = TexelAt(p, 0, i + 1);
				for (uint32 c = 0; c < channels; c++) dst[c] = src[c];
				// right border column (x = R+1) <- interior column R-1, flipped in y
				src = At(p, (uint32)(R - 1), (uint32)mirror);
				dst = TexelAt(p, R + 1, i + 1);
				for (uint32 c = 0; c < channels; c++) dst[c] = src[c];
				// top border row (y = 0) <- interior row 0, flipped in x
				src = At(p, (uint32)mirror, 0);
				dst = TexelAt(p, i + 1, 0);
				for (uint32 c = 0; c < channels; c++) dst[c] = src[c];
				// bottom border row (y = R+1) <- interior row R-1, flipped in x
				src = At(p, (uint32)mirror, (uint32)(R - 1));
				dst = TexelAt(p, i + 1, R + 1);
				for (uint32 c = 0; c < channels; c++) dst[c] = src[c];
			}

			// The four corners wrap to the diagonally OPPOSITE interior
			// corner, which follows from applying both edge rules at
			// once. Only one texel each, and only reached by filtering
			// exactly at a tile corner - but leaving them zero puts a
			// dark pixel in a place bilinear filtering will find.
			const struct { int32 bx, by, ix, iy; } corners[4] = {
				{ 0,     0,     R - 1, R - 1 },
				{ R + 1, 0,     0,     R - 1 },
				{ 0,     R + 1, R - 1, 0     },
				{ R + 1, R + 1, 0,     0     }
			};
			for (uint32 k = 0; k < 4; k++)
			{
				const f32 *src = At(p, (uint32)corners[k].ix, (uint32)corners[k].iy);
				f32 *dst = TexelAt(p, corners[k].bx, corners[k].by);
				for (uint32 c = 0; c < channels; c++) dst[c] = src[c];
			}
		}
	}

};
