//============================================================================
// Name        : DisplayHdr.cpp
// Description : HDR output, every backend alike (DisplayHdr.mm adds macOS's
//               EDR headroom)
//============================================================================

#include <Pyros3D/Rendering/Device/DisplayHdr.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace p3d { namespace gfx {

	bool WantsHdrOutput()
	{
		const char* e = getenv("PYROS_HDR");
		return e && atoi(e) != 0;
	}

#if !defined(__APPLE__)
	float DisplayHeadroom()
	{
		const char* e = getenv("PYROS_HDR_HEADROOM");
		return e ? std::max(1.f, (float)atof(e)) : 3.f;
	}
#endif

	namespace {
		float HalfToFloat(const uint16_t h)
		{
			const uint32_t sign = (uint32_t)(h & 0x8000) << 16, exponent = (h >> 10) & 0x1F, mantissa = h & 0x3FF;
			uint32_t bits;
			if (exponent == 0)
			{
				if (mantissa == 0) bits = sign;
				else { float f = std::ldexp((float)mantissa, -24); return sign ? -f : f; }
			}
			else if (exponent == 31) bits = sign | 0x7F800000u | (mantissa << 13);
			else bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
			float f; memcpy(&f, &bits, 4); return f;
		}
	}

	void HalfToSrgb8(const uint16_t* half, uint8_t* out, size_t pixels)
	{
		for (size_t i = 0; i < pixels * 4; i++)
		{
			float v = std::min(std::max(HalfToFloat(half[i]), 0.f), 1.f);
			if ((i & 3) != 3) v = v <= 0.0031308f ? v * 12.92f : 1.055f * powf(v, 1.f / 2.4f) - 0.055f;
			out[i] = (uint8_t)(v * 255.f + 0.5f);
		}
	}

} }
