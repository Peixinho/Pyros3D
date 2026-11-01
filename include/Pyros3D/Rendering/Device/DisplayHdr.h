//============================================================================
// Name        : DisplayHdr.h
// Description : HDR output, every backend alike: whether it was asked for,
//               how far past SDR white the screen goes, and a capture of a
//               half-float backbuffer as SDR
//============================================================================

#ifndef DISPLAYHDR_H
#define DISPLAYHDR_H

#include <cstddef>
#include <cstdint>

namespace p3d { namespace gfx {

	// PYROS_HDR=1: the backbuffer half floats in extended linear sRGB (1 is SDR white, more brighter) where the surface
	// offers it - Metal's EDR layer, Vulkan's EXTENDED_SRGB_LINEAR, WebGPU's extended tone mapping
	bool WantsHdrOutput();
	// How far past SDR white the screen shows now: macOS's EDR headroom (it opens a frame or two after HDR content is
	// shown, and moves with the brightness); elsewhere PYROS_HDR_HEADROOM, else 3 (scRGB on a common HDR monitor)
	float DisplayHeadroom();
	// A half-float RGBA backbuffer's pixels as 8-bit sRGB RGBA, clamped to SDR: what a test's capture writes
	void HalfToSrgb8(const uint16_t* half, uint8_t* out, size_t pixels);

} }

#endif /* DISPLAYHDR_H */
