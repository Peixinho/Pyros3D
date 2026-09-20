//============================================================================
// Name        : Octahedral.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Octahedral direction encoding, and the bordered probe
//               atlas layout DDGI stores irradiance and visibility in.
//============================================================================

#ifndef OCTAHEDRAL_H
#define OCTAHEDRAL_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	// Maps a unit direction to a square, and back.
	//
	// Why octahedral rather than a cubemap or SH, now that probes store
	// more than nine numbers: a cubemap needs six render targets and a
	// samplerCube per probe, and SH above order 2 costs coefficients
	// faster than it gains detail. An octahedral map is ONE square per
	// probe with near-uniform solid angle per texel, which means a whole
	// grid of probes packs into a single 2D atlas and samples with one
	// texture fetch and hardware bilinear filtering.
	//
	// Encoded coordinates are in [-1, 1].
	PYROS3D_API Vec2 OctEncode(const Vec3 &direction);
	PYROS3D_API Vec3 OctDecode(const Vec2 &oct);

	// A grid of probes packed into one 2D atlas, each occupying a tile of
	// (resolution + 2) squared texels.
	//
	// The +2 is a one-texel border, and it is not padding - it carries a
	// copy of the opposite interior edge, mirrored. An octahedral map
	// wraps in a way no texture addressing mode implements: the right
	// edge continues into the right edge REVERSED, not into the left.
	// Without the border, hardware bilinear filtering at a tile boundary
	// blends across the seam into directions that are nowhere near each
	// other, and the result is a visible cross of wrong lighting through
	// the middle of every probe's contribution. It is the single most
	// common way a DDGI implementation looks subtly broken.
	class PYROS3D_API ProbeAtlas
	{
	public:

		ProbeAtlas() : resolution(0), channels(0), probesX(0), probesY(0) {}

		// `resolution` is the INTERIOR edge; tiles end up resolution+2.
		// DDGI uses 8 for irradiance (smooth, low frequency) and 16 for
		// visibility (needs the extra detail to resolve a wall edge).
		bool Allocate(const uint32 probeCount, const uint32 resolution, const uint32 channels);

		uint32 GetResolution() const { return resolution; }
		uint32 GetChannels() const { return channels; }
		uint32 GetTileSize() const { return resolution + 2; }
		// Tiles per row. Needed by anything computing a probe's tile
		// origin - the sampling shader above all, which has to turn a
		// probe index into atlas coordinates exactly as this class does.
		// Its absence is why the first version of the border test
		// guessed the layout and failed against a correct atlas.
		uint32 GetProbesPerRow() const { return probesX; }
		uint32 GetWidth() const { return probesX * GetTileSize(); }
		uint32 GetHeight() const { return probesY * GetTileSize(); }
		const std::vector<f32> &GetData() const { return data; }
		std::vector<f32> &GetData() { return data; }

		// Interior texel (tx, ty) of probe `probe`, in [0, resolution).
		f32 *At(const uint32 probe, const uint32 tx, const uint32 ty);
		const f32 *At(const uint32 probe, const uint32 tx, const uint32 ty) const;

		// The direction interior texel (tx, ty) represents - texel
		// CENTRES, which is why the +0.5. Sampling corners instead
		// silently biases every probe by half a texel.
		static Vec3 TexelDirection(const uint32 tx, const uint32 ty, const uint32 resolution);

		// Fills every tile's border from its own interior, with the
		// mirrored wrap an octahedral map needs. Call after writing
		// interiors and before uploading, or filtering at tile edges
		// reads whatever was there before.
		void FillBorders();

		void Clear();

	private:
		uint32 resolution, channels, probesX, probesY;
		std::vector<f32> data;
		f32 *TexelAt(const uint32 probe, const int32 x, const int32 y);
	};

};

#endif /* OCTAHEDRAL_H */
