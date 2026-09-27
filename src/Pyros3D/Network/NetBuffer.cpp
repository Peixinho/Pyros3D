//============================================================================
// Name        : NetBuffer.cpp
// Author      : Duarte Peixinho
// Description : See NetBuffer.h.
//============================================================================

#include <Pyros3D/Network/NetBuffer.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace p3d {

	namespace {
		const f32 kAxisMax = 16777215.f;	// 2^24 - 1
		const f32 kRotMax = 32767.f;		// 15 bits
		const f32 kInvSqrt2 = 0.70710678f;	// the largest a non-largest component can be

		uint32 QuantizeAxis(const f32 v, const f32 lo, const f32 hi)
		{
			const f32 t = hi > lo ? (v - lo) / (hi - lo) : 0.f;
			return (uint32)std::lround(std::min(std::max(t, 0.f), 1.f) * kAxisMax);
		}
		f32 DequantizeAxis(const uint32 q, const f32 lo, const f32 hi)
		{
			return lo + (hi - lo) * ((f32)q / kAxisMax);
		}
	}

	void NetWriter::F32(const f32 v)
	{
		uint32 u;
		memcpy(&u, &v, 4);
		U32(u);
	}

	void NetWriter::VarU32(uint32 v)
	{
		while (v >= 0x80) { U8((uint8)(v | 0x80)); v >>= 7; }
		U8((uint8)v);
	}

	void NetWriter::String(const std::string &s)
	{
		VarU32((uint32)s.size());
		Bytes(s.data(), s.size());
	}

	void NetWriter::Bytes(const void* p, const size_t n)
	{
		const uchar* b = (const uchar*)p;
		data.insert(data.end(), b, b + n);
	}

	void NetWriter::Position(const Vec3 &v, const NetQuantization &q)
	{
		const uint32 a[3] = { QuantizeAxis(v.x, q.min.x, q.max.x), QuantizeAxis(v.y, q.min.y, q.max.y), QuantizeAxis(v.z, q.min.z, q.max.z) };
		for (int i = 0; i < 3; i++) { U8((uint8)a[i]); U8((uint8)(a[i] >> 8)); U8((uint8)(a[i] >> 16)); }
	}

	void NetWriter::Rotation(const Quaternion &qIn)
	{
		f32 c[4] = { qIn.x, qIn.y, qIn.z, qIn.w };
		const f32 len = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2] + c[3] * c[3]);
		if (len > 0.f) for (int i = 0; i < 4; i++) c[i] /= len;
		// Drop the largest; flip so it is positive (q and -q are one rotation).
		int largest = 0;
		for (int i = 1; i < 4; i++) if (std::fabs(c[i]) > std::fabs(c[largest])) largest = i;
		const f32 sign = c[largest] < 0.f ? -1.f : 1.f;
		uint64 packed = (uint64)largest;
		int shift = 2;
		for (int i = 0; i < 4; i++)
		{
			if (i == largest) continue;
			const f32 t = (c[i] * sign / kInvSqrt2) * 0.5f + 0.5f;	// -1/sqrt2..1/sqrt2 -> 0..1
			const uint64 v = (uint64)std::lround(std::min(std::max(t, 0.f), 1.f) * kRotMax);
			packed |= v << shift;
			shift += 15;
		}
		// 47 bits in 6 bytes.
		for (int b = 0; b < 6; b++) U8((uint8)(packed >> (b * 8)));
	}

	uint8 NetReader::U8()
	{
		if (at >= n) { ok = false; return 0; }
		return p[at++];
	}

	f32 NetReader::F32()
	{
		const uint32 u = U32();
		f32 v;
		memcpy(&v, &u, 4);
		return v;
	}

	uint32 NetReader::VarU32()
	{
		uint32 v = 0;
		for (int shift = 0; shift < 35; shift += 7)
		{
			const uint8 b = U8();
			if (!ok) return 0;
			v |= (uint32)(b & 0x7F) << shift;
			if (!(b & 0x80)) return v;
		}
		ok = false;	// more than 5 bytes: not a 32-bit varint
		return 0;
	}

	std::string NetReader::String()
	{
		const uint32 len = VarU32();
		if (!ok || len > Remaining()) { ok = false; return std::string(); }
		std::string s((const char*)p + at, len);
		at += len;
		return s;
	}

	bool NetReader::Bytes(void* out, const size_t count)
	{
		if (count > Remaining()) { ok = false; return false; }
		memcpy(out, p + at, count);
		at += count;
		return true;
	}

	Vec3 NetReader::Position(const NetQuantization &q)
	{
		uint32 a[3];
		for (int i = 0; i < 3; i++) { const uint32 b0 = U8(), b1 = U8(), b2 = U8(); a[i] = b0 | (b1 << 8) | (b2 << 16); }
		return Vec3(DequantizeAxis(a[0], q.min.x, q.max.x), DequantizeAxis(a[1], q.min.y, q.max.y), DequantizeAxis(a[2], q.min.z, q.max.z));
	}

	Quaternion NetReader::Rotation()
	{
		uint64 packed = 0;
		for (int b = 0; b < 6; b++) packed |= (uint64)U8() << (b * 8);
		const int largest = (int)(packed & 3);
		f32 c[4];
		f32 sum = 0.f;
		int shift = 2;
		for (int i = 0; i < 4; i++)
		{
			if (i == largest) continue;
			const f32 t = (f32)((packed >> shift) & 0x7FFF) / kRotMax;
			c[i] = (t * 2.f - 1.f) * kInvSqrt2;
			sum += c[i] * c[i];
			shift += 15;
		}
		c[largest] = std::sqrt(std::max(0.f, 1.f - sum));
		return Quaternion(c[3], c[0], c[1], c[2]);
	}

}
