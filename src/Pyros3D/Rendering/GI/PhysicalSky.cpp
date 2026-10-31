//============================================================================
// Name        : PhysicalSky.cpp
// Description : The physical air's light as an ambient environment (see the header). The model is
//               SkyEffect.cpp's - Rayleigh, Mie and ozone on a round planet, single scattering marched,
//               with the share scattered more than once - and must stay its twin: a change there is a
//               change here, or the ambient light and the sky in the picture part ways.
//============================================================================

#include <Pyros3D/Rendering/GI/PhysicalSky.h>
#include <cmath>
#include <algorithm>
#include <vector>

namespace p3d {

	namespace {
		const f32 kPi = 3.14159265f;
		const f32 kRg = 6360e3f, kRt = 6460e3f;
		const f32 kHR = 8000.f, kHM = 1200.f;
		const Vec3 kBetaR(5.802e-6f, 13.558e-6f, 33.1e-6f);
		const Vec3 kBetaO(0.650e-6f, 1.881e-6f, 0.085e-6f);

		Vec3 Mul(const Vec3 &a, const Vec3 &b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
		Vec3 Exp(const Vec3 &a) { return Vec3(std::exp(a.x), std::exp(a.y), std::exp(a.z)); }

		// (where a ray from ro along rd meets a sphere of radius r about the planet's centre)
		bool RaySphere(const Vec3 &ro, const Vec3 &rd, const f32 r, f32 &t0, f32 &t1)
		{
			const f32 b = ro.dotProduct(rd), c = ro.dotProduct(ro) - r * r, d = b * b - c;
			if (d < 0.f) return false;
			const f32 s = std::sqrt(d);
			t0 = -b - s; t1 = -b + s;
			return true;
		}
		Vec3 Density(const f32 h) { return Vec3(std::exp(-h / kHR), std::exp(-h / kHM), std::max(0.f, 1.f - std::fabs(h - 25000.f) / 15000.f)); }
		Vec3 Extinction(const Vec3 &dens, const f32 haze)
		{
			const f32 betaM = 3.996e-6f * 1.11f * haze;
			return kBetaR * dens.x + Vec3(betaM, betaM, betaM) * dens.y + kBetaO * dens.z;
		}
		// (SkyEffect.cpp's airSunTransmittance: the column of air above, times the air mass at the sun's angle)
		Vec3 SunTransmittance(const Vec3 &p, const Vec3 &sun, const f32 haze)
		{
			const f32 r = p.magnitude();
			const f32 h = std::max(r - kRg, 0.f);
			const f32 mu = (p * (1.f / r)).dotProduct(sun);
			if (mu < -0.12f) return Vec3(0.f, 0.f, 0.f);
			const f32 zenith = std::acos(std::max(-1.f, std::min(1.f, mu))) * 57.2957795f;
			const f32 airmass = 1.f / (std::max(mu, 0.f) + 0.50572f * std::pow(std::max(96.07995f - zenith, 0.5f), -1.6364f));
			const f32 m = 3.996e-6f * 1.11f * haze * (kHM * std::exp(-h / kHM));
			const Vec3 column = kBetaR * (kHR * std::exp(-h / kHR)) + Vec3(m, m, m) + kBetaO * (15000.f * std::max(0.f, std::min(1.f, (40000.f - h) / 30000.f)));
			return Exp(column * -airmass);
		}
		f32 PhaseR(const f32 mu) { return 3.f / (16.f * kPi) * (1.f + mu * mu); }
		f32 PhaseM(const f32 mu)
		{
			const f32 g = 0.80f;
			return 3.f / (8.f * kPi) * ((1.f - g * g) * (1.f + mu * mu)) / ((2.f + g * g) * std::pow(1.f + g * g - 2.f * g * mu, 1.5f));
		}
		// The sky's light arriving from dir (above the horizon), as SkyEffect.cpp's airScatter with the eye a
		// couple of metres over the ground, its sun's light (sunE) as the effect has it.
		Vec3 SkyRadiance(const Vec3 &dir, const Vec3 &sun, const f32 haze, const Vec3 &sunE)
		{
			const Vec3 ro(0.f, kRg + 2.f, 0.f);
			f32 a, b;
			if (!RaySphere(ro, dir, kRt, a, b)) return Vec3(0.f, 0.f, 0.f);
			const f32 t0 = std::max(a, 0.f);
			f32 t1 = b;
			f32 ga, gb;
			if (RaySphere(ro, dir, kRg, ga, gb) && ga > 0.f) t1 = std::min(t1, ga);
			if (t1 <= t0) return Vec3(0.f, 0.f, 0.f);
			const f32 mu = dir.dotProduct(sun);
			const f32 pR = PhaseR(mu), pM = PhaseM(mu);
			const f32 betaMs = 3.996e-6f * haze;
			const int N = 8;
			const f32 dt = (t1 - t0) / (f32)N;
			Vec3 L(0.f, 0.f, 0.f), od(0.f, 0.f, 0.f);
			for (int i = 0; i < N; i++)
			{
				const f32 t = t0 + dt * ((f32)i + 0.5f);
				const Vec3 p = ro + dir * t;
				const Vec3 dens = Density(p.magnitude() - kRg);
				const Vec3 ext = Extinction(dens, haze);
				const Vec3 Ts = SunTransmittance(p, sun, haze);
				const Vec3 scatR = kBetaR * dens.x, scatM = Vec3(betaMs, betaMs, betaMs) * dens.y;
				const Vec3 S = Mul(scatR * pR + scatM * pM, Ts) + (scatR + scatM) * (0.08f * std::max(sun.y + 0.1f, 0.f));
				const Vec3 Tstep = Exp(ext * -dt);
				const Vec3 grow(ext.x > 1e-9f ? (1.f - Tstep.x) / ext.x : dt, ext.y > 1e-9f ? (1.f - Tstep.y) / ext.y : dt, ext.z > 1e-9f ? (1.f - Tstep.z) / ext.z : dt);
				L = L + Mul(Mul(Exp(od * -1.f), S), grow);
				od = od + ext * dt;
			}
			return Mul(L, sunE);
		}
	}

	namespace { f32 g_sunAirHaze = 2.f; }
	void SetSunAirHaze(const f32 haze) { g_sunAirHaze = haze; }
	f32 GetSunAirHaze() { return g_sunAirHaze; }

	Vec3 PhysicalSunTransmittance(const f32 sunY, const f32 haze)
	{
		const f32 y = std::max(-1.f, std::min(1.f, sunY));
		return SunTransmittance(Vec3(0.f, kRg + 2.f, 0.f), Vec3(std::sqrt(std::max(0.f, 1.f - y * y)), y, 0.f), haze);
	}

	void PhysicalSkySH(const Vec3 &toSun, const f32 sunStrength, const f32 haze, const f32 sunPower,
		const f32 groundAlbedo, SphericalHarmonicsL2 &out)
	{
		out.Clear();
		const Vec3 sun = toSun.magnitude() > 1e-6f ? toSun.normalize() : Vec3(0.f, 1.f, 0.f);
		// (the sun's light as SkyEffect.cpp's airScatter has it - its calibration included)
		const f32 e = sunStrength * kPi * 14.f * sunPower;
		const Vec3 sunE(e, e, e);
		// Directions of equal solid angle: even in azimuth and in the sine of the elevation. The sky above, and
		// what reaches the ground with it (for the ground's light below the horizon).
		const int nPhi = 48, nY = 16;
		const f32 dOmega = 2.f * kPi / (f32)(nPhi * nY);       // (each, over a hemisphere)
		std::vector<Vec3> dirs, radiance;
		dirs.reserve(nPhi * nY); radiance.reserve(nPhi * nY);
		Vec3 skyOnGround(0.f, 0.f, 0.f);
		for (int j = 0; j < nY; j++)
		{
			const f32 y = ((f32)j + 0.5f) / (f32)nY, r = std::sqrt(std::max(0.f, 1.f - y * y));
			for (int i = 0; i < nPhi; i++)
			{
				const f32 phi = 2.f * kPi * ((f32)i + 0.5f) / (f32)nPhi;
				const Vec3 d(r * std::cos(phi), y, r * std::sin(phi));
				const Vec3 L = SkyRadiance(d, sun, haze, sunE);
				dirs.push_back(d); radiance.push_back(L);
				skyOnGround = skyOnGround + L * (y * dOmega);
			}
		}
		// The ground's light: the sun on it (its radiance in the scene's units is sunStrength * PI, a white
		// surface facing it sending back that much) and the sky on it, as much as it reflects.
		const f32 sunOnGround = sunStrength * kPi * std::max(sun.y, 0.f);
		const Vec3 ground = (skyOnGround * (1.f / kPi) + Vec3(sunOnGround, sunOnGround, sunOnGround)) * groundAlbedo;
		f32 basis[9];
		for (size_t k = 0; k < dirs.size(); k++)
		{
			EvaluateSHBasis(dirs[k], basis);
			for (int c = 0; c < 9; c++) out.coefficients[c] = out.coefficients[c] + radiance[k] * (basis[c] * dOmega);
			const Vec3 below(dirs[k].x, -dirs[k].y, dirs[k].z);
			EvaluateSHBasis(below, basis);
			for (int c = 0; c < 9; c++) out.coefficients[c] = out.coefficients[c] + ground * (basis[c] * dOmega);
		}
	}

};
