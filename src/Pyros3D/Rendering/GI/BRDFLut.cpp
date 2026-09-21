//============================================================================
// Name        : BRDFLut.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See BRDFLut.h.
//============================================================================

#include <Pyros3D/Rendering/GI/BRDFLut.h>
#include <algorithm>
#include <cmath>

namespace p3d {

	namespace {

		const f32 kPi = 3.14159265358979323846f;

		// Van der Corput radical inverse - the low-discrepancy sequence
		// half of Hammersley. Deterministic, so the table is identical
		// on every machine and a test can assert exact-ish values.
		inline f32 RadicalInverseVdC(uint32 bits)
		{
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return (f32)bits * 2.3283064365386963e-10f;
		}

		// GGX importance sampling around +Z.
		inline Vec3 ImportanceSampleGGX(const f32 u1, const f32 u2, const f32 roughness)
		{
			const f32 a = roughness * roughness;
			const f32 phi = 2.f * kPi * u1;
			const f32 cosTheta = sqrtf((1.f - u2) / (1.f + (a * a - 1.f) * u2));
			const f32 sinTheta = sqrtf(std::max(0.f, 1.f - cosTheta * cosTheta));
			return Vec3(cosf(phi) * sinTheta, sinf(phi) * sinTheta, cosTheta);
		}

		// Smith geometry with the IBL k, which is roughness^2/2 - NOT
		// the direct-lighting (roughness+1)^2/8. Using the direct one
		// here is a classic error: it darkens rough metals noticeably
		// and looks like the prefilter being wrong rather than the LUT.
		inline f32 GeometrySmithIBL(const f32 nDotV, const f32 nDotL, const f32 roughness)
		{
			const f32 k = (roughness * roughness) * 0.5f;
			const f32 gv = nDotV / (nDotV * (1.f - k) + k);
			const f32 gl = nDotL / (nDotL * (1.f - k) + k);
			return gv * gl;
		}

	} // namespace

	bool BRDFLut::Generate(const uint32 resolution, const uint32 samples)
	{
		if (resolution < 2 || samples == 0)
			return false;
		size = resolution;
		data.assign((size_t)size * size * 2, 0.f);

		for (uint32 y = 0; y < size; y++)
		{
			// Roughness on Y, N.V on X. Texel centres, so the table
			// never evaluates exactly at N.V = 0 where the integrand is
			// degenerate.
			const f32 roughness = ((f32)y + 0.5f) / (f32)size;
			for (uint32 x = 0; x < size; x++)
			{
				const f32 nDotV = ((f32)x + 0.5f) / (f32)size;
				const Vec3 v(sqrtf(std::max(0.f, 1.f - nDotV * nDotV)), 0.f, nDotV);

				f32 scale = 0.f, bias = 0.f;
				for (uint32 i = 0; i < samples; i++)
				{
					const f32 u1 = (f32)i / (f32)samples;
					const f32 u2 = RadicalInverseVdC(i);
					const Vec3 h = ImportanceSampleGGX(u1, u2, roughness);
					const f32 vDotH = v.dotProduct(h);
					const Vec3 l = h * (2.f * vDotH) - v;

					const f32 nDotL = std::max(l.z, 0.f);
					if (nDotL <= 0.f)
						continue;
					const f32 nDotH = std::max(h.z, 0.f);
					const f32 vDotHc = std::max(vDotH, 0.f);

					const f32 g = GeometrySmithIBL(nDotV, nDotL, roughness);
					const f32 gVis = (g * vDotHc) / std::max(nDotH * nDotV, 1e-6f);
					const f32 fc = powf(1.f - vDotHc, 5.f);

					// The Fresnel term factors out into these two
					// numbers - that factoring is the entire trick, and
					// why one table serves every F0.
					scale += (1.f - fc) * gVis;
					bias += fc * gVis;
				}
				const size_t o = ((size_t)y * size + x) * 2;
				data[o + 0] = scale / (f32)samples;
				data[o + 1] = bias / (f32)samples;
			}
		}
		return true;
	}

	void BRDFLut::Sample(const f32 nDotV, const f32 roughness, f32 &outScale, f32 &outBias) const
	{
		outScale = 0.f; outBias = 0.f;
		if (size == 0)
			return;
		const f32 fx = std::min(std::max(nDotV, 0.f), 1.f) * (f32)size - 0.5f;
		const f32 fy = std::min(std::max(roughness, 0.f), 1.f) * (f32)size - 0.5f;
		const int32 x0 = (int32)floorf(std::max(fx, 0.f));
		const int32 y0 = (int32)floorf(std::max(fy, 0.f));
		const int32 x1 = std::min(x0 + 1, (int32)size - 1);
		const int32 y1 = std::min(y0 + 1, (int32)size - 1);
		const f32 tx = std::min(std::max(fx - (f32)x0, 0.f), 1.f);
		const f32 ty = std::min(std::max(fy - (f32)y0, 0.f), 1.f);

		for (uint32 c = 0; c < 2; c++)
		{
			const f32 a = data[((size_t)y0 * size + x0) * 2 + c];
			const f32 b = data[((size_t)y0 * size + x1) * 2 + c];
			const f32 cc = data[((size_t)y1 * size + x0) * 2 + c];
			const f32 d = data[((size_t)y1 * size + x1) * 2 + c];
			const f32 v = (a * (1.f - tx) + b * tx) * (1.f - ty) + (cc * (1.f - tx) + d * tx) * ty;
			if (c == 0) outScale = v; else outBias = v;
		}
	}

};
