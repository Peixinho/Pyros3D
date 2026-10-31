//============================================================================
// OceanSim - see the header. The spectrum, its evolution and the assembly follow Ultra Water's OceanSim.js
// (https://github.com/Humpalumps/Ultra-Water, MIT, (c) 2026 Humpalumps), done on the CPU: k is indexed without
// the centring shift (i < N/2 ? i : i - N), so -k is (N - i) % N and no sign flip is needed after the transform.
//============================================================================

#include <Pyros3D/Rendering/Water/OceanSim.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <chrono>
#include <cmath>

namespace p3d {

	namespace {
		const f32 kLengths[OceanSim::Cascades] = { 600.f, 85.f, 12.f, 2.1f };
		const f64 G = 9.81;
		// surface tension / density (m^3/s^2): w^2 = g k + (sigma/rho) k^3
		const f64 SIGMA_RHO = 7.4e-5;
		// w is quantised to multiples of 2 pi / LOOP and time wraps at LOOP: the sea loops and phase stays exact
		const f64 LOOP = 300.0;
		const f64 kPi = 3.14159265358979323846;

		f64 Omega(const f64 k) { return std::sqrt(G * k + SIGMA_RHO * k * k * k); }


		uint32 Pcg(uint32 &x, uint32 &y)
		{
			x = x * 1664525u + 1013904223u; y = y * 1664525u + 1013904223u;
			x += y * 1664525u; y += x * 1664525u;
			x ^= x >> 16u; y ^= y >> 16u;
			x += y * 1664525u; y += x * 1664525u;
			x ^= x >> 16u; y ^= y >> 16u;
			return x;
		}

		void Gauss(const uint32 i, const uint32 j, const uint32 c, const uint32 seed, f64 &a, f64 &b)
		{
			uint32 x = i + c * 7919u + seed, y = j + c * 104729u + seed;
			Pcg(x, y);
			const f64 u1 = ((f64)x + 1.0) / 4294967296.0, u2 = (f64)y / 4294967296.0;
			const f64 r = std::sqrt(-2.0 * std::log(u1));
			a = r * std::cos(2.0 * kPi * u2); b = r * std::sin(2.0 * kPi * u2);
		}

		struct Spectrum { f64 scale, angle, spreadBlend, swell, alpha, peakOmega, gamma, fade; };

		f64 Jonswap(const f64 w, const Spectrum &s)
		{
			const f64 wp = s.peakOmega, sigma = w <= wp ? 0.07 : 0.09;
			const f64 r = std::exp(-(w - wp) * (w - wp) / (2.0 * sigma * sigma * wp * wp));
			const f64 q = wp / w;
			return s.scale * s.alpha * G * G / std::pow(w, 5.0) * std::exp(-1.25 * q * q * q * q) * std::pow(s.gamma, r);
		}

		f64 Normalisation(const f64 s)
		{
			const f64 s2 = s * s, s3 = s2 * s, s4 = s3 * s;
			if (s < 5.0) return -0.000564 * s4 + 0.00776 * s3 - 0.044 * s2 + 0.192 * s + 0.163;
			return -4.80e-08 * s4 + 1.07e-05 * s3 - 9.53e-04 * s2 + 5.90e-02 * s + 3.93e-01;
		}

		f64 Directional(const f64 theta, const f64 w, const Spectrum &sp)
		{
			const f64 x = w / sp.peakOmega;
			f64 s = x > 1.0 ? 9.77 * std::pow(x, -2.5) : 6.97 * std::pow(x, 5.0);
			s += 16.0 * std::tanh(std::fmin(x, 20.0)) * sp.swell * sp.swell;
			const f64 t = theta - sp.angle;
			const f64 cos2s = Normalisation(s) * std::pow(std::fabs(std::cos(0.5 * t)), 2.0 * s);
			return (1.0 - sp.spreadBlend) * (2.0 / kPi * std::cos(t) * std::cos(t)) + sp.spreadBlend * cos2s;
		}

		void JonswapParams(const f64 wind, const f64 fetchKm, f64 &alpha, f64 &peakOmega)
		{
			const f64 fetch = fetchKm * 1000.0;
			alpha = 0.076 * std::pow(G * fetch / (wind * wind), -0.22);
			peakOmega = 22.0 * std::pow(wind * fetch / (G * G), -0.33);
		}
	}

	OceanSim::OceanSim(const OceanSimParams &p) : params(p)
	{
		const uint32 NN = N * N;
		bitrev.resize(N);
		for (uint32 i = 0; i < N; i++)
		{
			uint32 r = 0;
			for (uint32 b = 0; b < LogN; b++) if (i & (1u << b)) r |= 1u << (LogN - 1 - b);
			bitrev[i] = r;
		}
		twiddle.resize(N / 2);
		for (uint32 i = 0; i < N / 2; i++) twiddle[i] = cf((f32)std::cos(2.0 * kPi * i / N), (f32)std::sin(2.0 * kPi * i / N));

		for (uint32 c = 0; c < Cascades; c++)
		{
			h0[c].resize(NN); h0mc[c].resize(NN);
			for (uint32 f = 0; f < 4; f++) { wre[c][f].resize(NN); wim[c][f].resize(NN); }
			kxs[c].resize(NN); kzs[c].resize(NN); iks[c].resize(NN); quant[c].resize(NN);
			turbulence[c].assign(NN, 1.f);
			dispOut[c].assign(NN * 4, 0.f); slopeOut[c].assign(NN * 4, 0.f);
			for (uint32 i = 0; i < NN; i++) dispOut[c][i * 4 + 3] = 1.f;
			for (int t = 0; t < 2; t++)
			{
				std::shared_ptr<Texture> tex = std::make_shared<Texture>();
				tex->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, N, N, true);
				tex->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
				tex->SetMinMagFilter(TextureFilter::LinearMipmapLinear, TextureFilter::Linear);
				tex->UpdateData(t == 0 ? dispOut[c].data() : slopeOut[c].data());
				(t == 0 ? displacement[c] : slope[c]) = tex;
			}
		}
		BuildSpectrum();
		for (uint32 c = 0; c < Cascades; c++) Step(c);
	}

	OceanSim::~OceanSim()
	{
		if (running) JobSystem::Instance().Wait(counter);
	}

	f32 OceanSim::GetLength(const uint32 cascade) const { return kLengths[cascade % Cascades]; }

	Vec3 OceanSim::Displacement(const f32 x, const f32 z, const Vec3 &weights) const
	{
		Vec3 d;
		const f32 w[3] = { weights.x, weights.y, weights.z };
		for (uint32 c = 0; c < 3; c++)
		{
			if (shown[c].empty() || w[c] == 0.f) continue;
			// bilinear, repeating, texel centres at (i + 0.5) / N - as the GPU reads the texture
			const f32 u = x / kLengths[c] * N - 0.5f, v = z / kLengths[c] * N - 0.5f;
			const f32 fu = std::floor(u), fv = std::floor(v), au = u - fu, av = v - fv;
			const int32 i0 = (int32)fu, j0 = (int32)fv;
			f32 r[3] = { 0.f, 0.f, 0.f };
			for (int32 dj = 0; dj < 2; dj++)
				for (int32 di = 0; di < 2; di++)
				{
					const uint32 i = (uint32)(((i0 + di) % (int32)N + (int32)N) % (int32)N), j = (uint32)(((j0 + dj) % (int32)N + (int32)N) % (int32)N);
					const f32 k = (di ? au : 1.f - au) * (dj ? av : 1.f - av);
					const f32* q = &shown[c][(j * N + i) * 3];
					r[0] += q[0] * k; r[1] += q[1] * k; r[2] += q[2] * k;
				}
			d += Vec3(r[0], r[1], r[2]) * w[c];
		}
		return d;
	}

	f32 OceanSim::HeightAt(const f32 x, const f32 z, const Vec3 &weights) const
	{
		f32 px = x, pz = z;
		Vec3 d;
		for (int i = 0; i < 4; i++) { d = Displacement(px, pz, weights); px = x - d.x; pz = z - d.z; }
		return Displacement(px, pz, weights).y;
	}

	Vec3 OceanSim::GetStats(const uint32 cascade)
	{
		// (the fields the workers are making: waited for, not read half-written)
		if (running) { JobSystem::Instance().Wait(counter); running = false; }
		const uint32 c = cascade % Cascades;
		f64 h2 = 0.0, m = 0.0, fold = 0.0;
		for (uint32 i = 0; i < N * N; i++)
		{
			const f32* o = &dispOut[c][i * 4]; const f32* q = &slopeOut[c][i * 4];
			h2 += o[1] * o[1]; m += q[2] + q[3]; fold += o[3] < 0.6f ? 1.0 : 0.0;
		}
		return Vec3((f32)std::sqrt(h2 / (N * N)), (f32)(m / (N * N)), (f32)(fold / (N * N)));
	}

	void OceanSim::SetParams(const OceanSimParams &p)
	{
		if (running) { JobSystem::Instance().Wait(counter); running = false; }
		params = p;
		BuildSpectrum();
	}

	void OceanSim::BuildSpectrum()
	{
		const OceanSimParams &p = params;
		Spectrum sp[2];
		f64 a, w;
		JonswapParams(p.windSpeed, p.fetchKm, a, w);
		sp[0] = { p.waveScale, p.windDirection * kPi / 180.0, p.spreadBlend, p.windSwell, a, w, p.gamma, p.shortWavesFade };
		JonswapParams(p.swellWindSpeed, p.swellFetchKm, a, w);
		sp[1] = { (f64)p.swellScale * p.waveScale, p.swellDirection * kPi / 180.0, 1.0, p.swellSwell, a, w, p.gamma, p.shortWavesFade };

		// cascade c owns [cut[c-1], cut[c]); each cut sits at 6 fundamentals of the next cascade
		f64 cut[Cascades];
		for (uint32 c = 0; c + 1 < Cascades; c++) cut[c] = 2.0 * kPi / kLengths[c + 1] * 6.0;

		JobSystem::Instance().ParallelFor(Cascades * N, 16, [&](uint32 begin, uint32 end) {
			for (uint32 row = begin; row < end; row++)
			{
				const uint32 c = row / N, j = row % N;
				const f64 L = kLengths[c], dk = 2.0 * kPi / L;
				const f64 lo = c == 0 ? 1e-4 : cut[c - 1], hi = c == Cascades - 1 ? 1e4 : cut[c];
				const f64 fade = c == Cascades - 1 ? p.capillaryFade : -1.0;
				for (uint32 i = 0; i < N; i++)
				{
					const f64 kx = ((int)i < (int)N / 2 ? (int)i : (int)i - (int)N) * dk;
					const f64 kz = ((int)j < (int)N / 2 ? (int)j : (int)j - (int)N) * dk;
					const f64 kl = std::sqrt(kx * kx + kz * kz);
					f64 amp = 0.0;
					if (kl >= lo && kl <= hi)
					{
						const f64 om = Omega(kl), dwdk = (G + 3.0 * SIGMA_RHO * kl * kl) / (2.0 * om), theta = std::atan2(kz, kx);
						f64 S = 0.0;
						for (int s = 0; s < 2; s++)
						{
							if (sp[s].scale <= 0.0) continue;
							const f64 f = fade >= 0.0 ? fade : sp[s].fade;
							S += Jonswap(om, sp[s]) * Directional(theta, om, sp[s]) * std::exp(-f * f * kl * kl);
						}
						// (h(k, t) = h0(k) e + conj(h0(-k)) conj(e) puts two draws in each wave and the field is
						// their sum over k and -k: for the waves' variance to be the spectrum's, E|h0|^2 = E dk^2 / 2.
						// The reference draws sqrt(2 E dk^2) - eight times the energy, a storm for its wind.)
						amp = std::sqrt(S * dwdk / kl * dk * dk * p.energy) * 0.5;
					}
					// the random draw is keyed to the centred index, as the reference's is
					f64 ga, gb;
					Gauss((i + N / 2) % N, (j + N / 2) % N, c, p.seed, ga, gb);
					h0[c][j * N + i] = cf((f32)(ga * amp), (f32)(gb * amp));
					// (what Evolve needs of each wave, made once: its wave vector, 1/k, and w as a whole number of 2 pi / LOOP)
					kxs[c][j * N + i] = (f32)kx; kzs[c][j * N + i] = (f32)kz; iks[c][j * N + i] = kl > 1e-6 ? (f32)(1.0 / kl) : 0.f;
					quant[c][j * N + i] = std::floor(Omega(kl) / (2.0 * kPi / LOOP));
				}
			}
		});
		for (uint32 c = 0; c < Cascades; c++)
			for (uint32 j = 0; j < N; j++)
				for (uint32 i = 0; i < N; i++)
					h0mc[c][j * N + i] = std::conj(h0[c][((N - j) % N) * N + (N - i) % N]);
	}

	void OceanSim::Evolve(const uint32 c)
	{
		const f64 tl = time / LOOP;
		const bool slopesOnly = c == Cascades - 1;
		f32 *r0 = wre[c][0].data(), *i0 = wim[c][0].data(), *r1 = wre[c][1].data(), *i1 = wim[c][1].data();
		f32 *r2 = wre[c][2].data(), *i2 = wim[c][2].data(), *r3 = wre[c][3].data(), *i3 = wim[c][3].data();
		for (uint32 id = 0; id < N * N; id++)
		{
			// w = n 2 pi / LOOP: the phase reduced to one turn in double before the (float) sine and cosine
			const f64 turns = quant[c][id] * tl;
			const f32 ph = (f32)(2.0 * kPi * (turns - std::floor(turns)));
			const f32 ec = std::cos(ph), es = std::sin(ph);
			const cf a = h0[c][id], b = h0mc[c][id];
			// h = h0 e + conj(h0(-k)) conj(e)
			const f32 hr = a.real() * ec - a.imag() * es + b.real() * ec + b.imag() * es;
			const f32 hi = a.real() * es + a.imag() * ec - b.real() * es + b.imag() * ec;
			const f32 kx = kxs[c][id], kz = kzs[c][id], ik = iks[c][id];
			// i h
			const f32 ihr = -hi, ihi = hr;
			// two real fields packed as one complex spectrum: IFFT(A + iB) = a + ib, A + iB = (Ar - Bi, Ai + Br)
			r2[id] = ihr * kx - ihi * kz; i2[id] = ihi * kx + ihr * kz;                         // dDy/dx, dDy/dz
			if (slopesOnly) continue;
			const f32 ax = kx * ik, az = kz * ik;
			r0[id] = ihr * ax - ihi * az; i0[id] = ihi * ax + ihr * az;                         // Dx, Dz
			const f32 cxz = -kx * kz * ik;
			r1[id] = hr - hi * cxz; i1[id] = hi + hr * cxz;                                      // Dy, Dxz
			const f32 cxx = -kx * kx * ik, czz = -kz * kz * ik;
			r3[id] = hr * cxx - hi * czz; i3[id] = hi * cxx + hr * czz;                          // Dxx, Dzz
		}
	}

	void OceanSim::Assemble(const uint32 c)
	{
		const f32 lambda = params.choppiness, dt = lastDt, rec = params.foamRecovery;
		const bool slopesOnly = c == Cascades - 1;
		for (uint32 id = 0; id < N * N; id++)
		{
			const f32 dx = slopesOnly ? 0.f : wre[c][0][id], dz = slopesOnly ? 0.f : wim[c][0][id];
			const f32 dy = slopesOnly ? 0.f : wre[c][1][id], dxzR = slopesOnly ? 0.f : wim[c][1][id];
			const f32 dxxR = slopesOnly ? 0.f : wre[c][3][id], dzzR = slopesOnly ? 0.f : wim[c][3][id];
			const f32 dxx = lambda * dxxR, dzz = lambda * dzzR, dxz = lambda * dxzR;
			const f32 jac = (1.f + dxx) * (1.f + dzz) - dxz * dxz;
			const f32 sx = wre[c][2][id] / std::fmax(1.f + dxx, 0.2f), sz = wim[c][2][id] / std::fmax(1.f + dzz, 0.2f);
			// foam follows the Jacobian down at once and recovers at a dt-based rate
			const f32 turb = std::fmin(jac, turbulence[c][id] + dt * rec / std::fmax(jac, 0.5f));
			turbulence[c][id] = turb;
			f32* o = &dispOut[c][id * 4];
			o[0] = lambda * dx; o[1] = dy; o[2] = lambda * dz; o[3] = turb;
			f32* q = &slopeOut[c][id * 4];
			q[0] = sx; q[1] = sz; q[2] = sx * sx; q[3] = sz * sz;
		}
	}

	namespace {
		// In place, a square N x N of floats: blocks of 16 so both sides stay in the cache.
		void Transpose(f32* m, const uint32 n)
		{
			const uint32 B = 16;
			for (uint32 bj = 0; bj < n; bj += B)
				for (uint32 bi = bj; bi < n; bi += B)
					for (uint32 j = bj; j < bj + B; j++)
						for (uint32 i = (bi == bj ? j + 1 : bi); i < bi + B; i++)
							std::swap(m[j * n + i], m[i * n + j]);
		}
	}

	void OceanSim::FftColumns(f32* re, f32* im)
	{
		// Every column at once: the butterflies run along whole rows, so the inner loop is contiguous floats the
		// compiler does four (NEON) or eight (AVX) at a time. Rows into bit-reversed order first.
		for (uint32 j = 0; j < N; j++)
		{
			const uint32 r = bitrev[j];
			if (r > j)
				for (uint32 i = 0; i < N; i++) { std::swap(re[j * N + i], re[r * N + i]); std::swap(im[j * N + i], im[r * N + i]); }
		}
		for (uint32 size = 2; size <= N; size <<= 1)
		{
			const uint32 half = size >> 1, step = N / size;
			for (uint32 s = 0; s < N; s += size)
				for (uint32 k = 0; k < half; k++)
				{
					const f32 wr = twiddle[k * step].real(), wi = twiddle[k * step].imag();
					f32* __restrict ar = re + (s + k) * N; f32* __restrict ai = im + (s + k) * N;
					f32* __restrict br = re + (s + k + half) * N; f32* __restrict bi = im + (s + k + half) * N;
					for (uint32 i = 0; i < N; i++)
					{
						const f32 tr = wr * br[i] - wi * bi[i], ti = wr * bi[i] + wi * br[i];
						br[i] = ar[i] - tr; bi[i] = ai[i] - ti;
						ar[i] += tr; ai[i] += ti;
					}
				}
		}
	}

	void OceanSim::Fft(f32* re, f32* im)
	{
		// e^{+i} (inverse), unnormalised - the amplitudes already carry dk. Columns, then rows (as columns of the
		// transpose, turned back after).
		FftColumns(re, im);
		Transpose(re, N); Transpose(im, N);
		FftColumns(re, im);
		Transpose(re, N); Transpose(im, N);
	}

	void OceanSim::Step(const uint32 c)
	{
		const auto t0 = std::chrono::steady_clock::now();
		Evolve(c);
		// (the capillary cascade is only ever read for its slopes: one transform, not four)
		if (c == Cascades - 1) Fft(wre[c][2].data(), wim[c][2].data());
		else for (uint32 f = 0; f < 4; f++) Fft(wre[c][f].data(), wim[c][f].data());
		Assemble(c);
		stepMs[c] = std::chrono::duration<f32, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}

	void OceanSim::Update(const f32 dt)
	{
		const auto t0 = std::chrono::steady_clock::now();
		// The fields are made a frame ahead, on the job workers, one cascade a job: this frame shows the ones
		// made during the last, and the next moment's are made while this one draws.
		if (running) { JobSystem::Instance().Wait(counter); running = false; }
		for (uint32 c = 0; c < Cascades; c++)
		{
			displacement[c]->UpdateData(dispOut[c].data());
			slope[c]->UpdateData(slopeOut[c].data());
		}
		for (uint32 c = 0; c < 3; c++)
		{
			shown[c].resize(N * N * 3);
			for (uint32 i = 0; i < N * N; i++)
				for (uint32 k = 0; k < 3; k++) shown[c][i * 3 + k] = dispOut[c][i * 4 + k];
		}
		lastDt = dt;
		time = std::fmod(time + dt, LOOP);
		for (uint32 c = 0; c < Cascades; c++)
			JobSystem::Instance().Run([this, c]() { Step(c); }, counter);
		running = true;
		lastMs = std::chrono::duration<f32, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}

}
