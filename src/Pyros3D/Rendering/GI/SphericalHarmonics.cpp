//============================================================================
// Name        : SphericalHarmonics.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See SphericalHarmonics.h.
//============================================================================

#include <Pyros3D/Rendering/GI/SphericalHarmonics.h>
#include <cmath>

namespace p3d {

	namespace {

		const f32 kPi = 3.14159265358979323846f;

		// Real SH basis constants for l = 0..2, in the (l, m) index order
		// documented on SphericalHarmonicsL2. These are the normalisation
		// factors of the real spherical harmonics, not arbitrary weights -
		// getting one wrong produces a projection that still reconstructs
		// something smooth and plausible, just not the environment.
		const f32 kY00  = 0.282095f; // 1/2 * sqrt(1/pi)
		const f32 kY1   = 0.488603f; // 1/2 * sqrt(3/pi)
		const f32 kY2a  = 1.092548f; // 1/2 * sqrt(15/pi)
		const f32 kY20  = 0.315392f; // 1/4 * sqrt(5/pi)
		const f32 kY22  = 0.546274f; // 1/4 * sqrt(15/pi)

		// Ramamoorthi & Hanrahan's cosine-lobe convolution constants. The
		// cosine lobe's own SH coefficients, divided through by the basis
		// normalisation, collapse to these five numbers - which is the
		// entire reason irradiance from an order-2 projection is a closed
		// form rather than an integral.
		const f32 kC1 = 0.429043f;
		const f32 kC2 = 0.511664f;
		const f32 kC3 = 0.743125f;
		const f32 kC4 = 0.886227f;
		const f32 kC5 = 0.247708f;

		// Direction through the centre of texel (u, v) on one cubemap
		// face, where u and v are already in [-1, 1].
		//
		// The face orientations follow the cubemap convention (which is
		// left-handed and has +Y/-Y's V axis running the opposite way
		// from the other four) rather than anything this engine chose.
		// They have to match how Texture uploads its faces or the
		// projection is a rotated version of the real environment - an
		// error that looks entirely correct until something else in the
		// scene disagrees with it.
		Vec3 CubemapTexelDirection(const uint32 face, const f32 u, const f32 v)
		{
			switch (face)
			{
			case 0: return Vec3( 1.f,   -v,   -u); // +X
			case 1: return Vec3(-1.f,   -v,    u); // -X
			case 2: return Vec3(   u,  1.f,    v); // +Y
			case 3: return Vec3(   u, -1.f,   -v); // -Y
			case 4: return Vec3(   u,   -v,  1.f); // +Z
			default:return Vec3(  -u,   -v, -1.f); // -Z
			}
		}

		// Solid angle subtended by one texel of a cubemap face.
		//
		// Closed form rather than an approximation: the solid angle of the
		// spherical rectangle a texel projects to is the sum of four
		// atan2 terms at its corners. The naive alternative - treating
		// every texel as equal, or scaling by a 1/(1+u^2+v^2)^1.5
		// falloff - is close enough at the centre of a face and wrong by
		// ~20% at the corners, which biases the whole projection toward
		// the cube's diagonals.
		f32 TexelSolidAngle(const f32 u0, const f32 v0, const f32 u1, const f32 v1)
		{
			// Area of the spherical rectangle [u0,u1] x [v0,v1] on the
			// unit cube's face, via the standard corner formula.
			const f32 a = atanf((u1 * v1) / sqrtf(u1 * u1 + v1 * v1 + 1.f));
			const f32 b = atanf((u0 * v1) / sqrtf(u0 * u0 + v1 * v1 + 1.f));
			const f32 c = atanf((u1 * v0) / sqrtf(u1 * u1 + v0 * v0 + 1.f));
			const f32 d = atanf((u0 * v0) / sqrtf(u0 * u0 + v0 * v0 + 1.f));
			return a - b - c + d;
		}

	} // namespace

	void SphericalHarmonicsL2::Clear()
	{
		for (uint32 i = 0; i < kCoefficientCount; i++)
			coefficients[i] = Vec3(0.f, 0.f, 0.f);
	}

	bool SphericalHarmonicsL2::IsZero() const
	{
		for (uint32 i = 0; i < kCoefficientCount; i++)
		{
			const Vec3 &c = coefficients[i];
			if (c.x != 0.f || c.y != 0.f || c.z != 0.f)
				return false;
		}
		return true;
	}

	void SphericalHarmonicsL2::Scale(const f32 factor)
	{
		for (uint32 i = 0; i < kCoefficientCount; i++)
			coefficients[i] = coefficients[i] * factor;
	}

	void EvaluateSHBasis(const Vec3 &d, f32 outBasis[9])
	{
		outBasis[0] = kY00;
		outBasis[1] = kY1 * d.y;
		outBasis[2] = kY1 * d.z;
		outBasis[3] = kY1 * d.x;
		outBasis[4] = kY2a * d.x * d.y;
		outBasis[5] = kY2a * d.y * d.z;
		outBasis[6] = kY20 * (3.f * d.z * d.z - 1.f);
		outBasis[7] = kY2a * d.x * d.z;
		outBasis[8] = kY22 * (d.x * d.x - d.y * d.y);
	}

	Vec3 SphericalHarmonicsL2::EvaluateRadiance(const Vec3 &direction) const
	{
		f32 basis[9];
		EvaluateSHBasis(direction, basis);
		Vec3 result(0.f, 0.f, 0.f);
		for (uint32 i = 0; i < kCoefficientCount; i++)
			result += coefficients[i] * basis[i];
		return result;
	}

	Vec3 SphericalHarmonicsL2::Irradiance(const Vec3 &normal) const
	{
		const f32 x = normal.x, y = normal.y, z = normal.z;
		// Mirrors the shader's SHIrradiance() in PyrosShader.glsl exactly.
		// If one changes, the other must - a CPU/GPU divergence here shows
		// up as ambient that shifts when a material switches between the
		// forward and deferred paths, which is a miserable thing to chase.
		Vec3 result(0.f, 0.f, 0.f);
		result += coefficients[8] * (kC1 * (x * x - y * y));
		result += coefficients[6] * (kC3 * z * z);
		result += coefficients[0] * kC4;
		result += coefficients[6] * (-kC5);
		result += coefficients[4] * (2.f * kC1 * x * y);
		result += coefficients[7] * (2.f * kC1 * x * z);
		result += coefficients[5] * (2.f * kC1 * y * z);
		result += coefficients[3] * (2.f * kC2 * x);
		result += coefficients[1] * (2.f * kC2 * y);
		result += coefficients[2] * (2.f * kC2 * z);
		return result;
	}

	Vec3 SphericalHarmonicsL2::AmbientIrradiance(const Vec3 &normal) const
	{
		return Irradiance(normal) * (1.f / kPi);
	}

	bool ProjectCubemapToSH(const std::vector<CubemapFacePixels> &faces, SphericalHarmonicsL2 &outSH)
	{
		outSH.Clear();
		if (faces.size() != 6)
			return false;
		const uint32 size = faces[0].size;
		if (size == 0)
			return false;
		for (size_t f = 0; f < faces.size(); f++)
		{
			if (faces[f].pixels == NULL || faces[f].size != size)
				return false;
		}

		const f32 invSize = 1.f / (f32)size;
		f32 totalSolidAngle = 0.f;

		for (uint32 face = 0; face < 6; face++)
		{
			const f32 *pixels = faces[face].pixels;
			for (uint32 py = 0; py < size; py++)
			{
				// Texel EDGES in [-1, 1], not just the centre: the solid
				// angle below is the area of the patch between them.
				const f32 v0 = 2.f * ((f32)py * invSize) - 1.f;
				const f32 v1 = 2.f * ((f32)(py + 1) * invSize) - 1.f;
				const f32 vc = 0.5f * (v0 + v1);
				for (uint32 px = 0; px < size; px++)
				{
					const f32 u0 = 2.f * ((f32)px * invSize) - 1.f;
					const f32 u1 = 2.f * ((f32)(px + 1) * invSize) - 1.f;
					const f32 uc = 0.5f * (u0 + u1);

					const f32 solidAngle = TexelSolidAngle(u0, v0, u1, v1);
					totalSolidAngle += solidAngle;

					Vec3 direction = CubemapTexelDirection(face, uc, vc);
					direction.normalizeSelf();

					f32 basis[9];
					EvaluateSHBasis(direction, basis);

					const uint32 idx = (py * size + px) * 3;
					const Vec3 radiance(pixels[idx + 0], pixels[idx + 1], pixels[idx + 2]);

					for (uint32 i = 0; i < SphericalHarmonicsL2::kCoefficientCount; i++)
						outSH.coefficients[i] += radiance * (basis[i] * solidAngle);
				}
			}
		}

		// Sanity, not normalisation: the six faces' texels must between
		// them cover the whole sphere, 4*PI steradians. A total that is
		// off by more than a rounding error means the direction mapping
		// or the solid-angle formula is wrong, and every coefficient is
		// scaled by that same error - which is invisible in the output
		// because it looks exactly like a dimmer or brighter environment.
		const f32 expected = 4.f * kPi;
		if (fabsf(totalSolidAngle - expected) > 0.01f * expected)
		{
			outSH.Clear();
			return false;
		}
		return true;
	}

};
