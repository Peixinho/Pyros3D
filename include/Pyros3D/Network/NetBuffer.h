//============================================================================
// Name        : NetBuffer.h
// Author      : Duarte Peixinho
// Description : Writing and reading network messages, compactly. Bandwidth
//               is the budget a 100-player game lives inside, so the usual
//               tricks are here: variable-length integers, positions
//               quantized to a fixed step inside the map's bounds, and
//               rotations as the three smallest quaternion components.
//
//               Little-endian throughout. A reader that runs off the end
//               stops, returns zeros and reports it through Ok() - a
//               malformed or truncated packet from the network must never
//               read past its buffer.
//============================================================================

#ifndef NETBUFFER_H
#define NETBUFFER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>
#include <vector>

namespace p3d {

	// Where quantized positions may lie: 24 bits per axis across the box,
	// so the default 16 km x 2 km x 16 km resolves ~1 mm horizontally and
	// ~0.1 mm vertically. Anything outside clamps to the box.
	struct NetQuantization
	{
		Vec3 min = Vec3(-8192.f, -1024.f, -8192.f);
		Vec3 max = Vec3(8192.f, 1024.f, 8192.f);
	};

	class PYROS3D_API NetWriter
	{
	public:
		std::vector<uchar> data;

		void U8(const uint8 v) { data.push_back(v); }
		void U16(const uint16 v) { U8((uint8)v); U8((uint8)(v >> 8)); }
		void U32(const uint32 v) { U16((uint16)v); U16((uint16)(v >> 16)); }
		void U64(const uint64 v) { U32((uint32)v); U32((uint32)(v >> 32)); }
		void F32(const f32 v);
		void Bool(const bool v) { U8(v ? 1 : 0); }
		// 7 bits a byte: small numbers (ids, counts, deltas) cost one byte.
		void VarU32(uint32 v);
		void VarI32(const int32 v) { VarU32(((uint32)v << 1) ^ (uint32)(v >> 31)); }
		void String(const std::string &s);
		void Bytes(const void* p, const size_t n);
		void Vec3Raw(const Vec3 &v) { F32(v.x); F32(v.y); F32(v.z); }
		// 3 x 24 bits with the default quantization.
		void Position(const Vec3 &v, const NetQuantization &q = NetQuantization());
		// Smallest three: 2 bits of index + 3 x 15 bits, ~0.00005 rad.
		void Rotation(const Quaternion &q);
		size_t Size() const { return data.size(); }
	};

	class PYROS3D_API NetReader
	{
	public:
		NetReader(const uchar* data, const size_t length) : p(data), n(length), at(0), ok(true) {}
		explicit NetReader(const std::vector<uchar> &v) : p(v.empty() ? NULL : &v[0]), n(v.size()), at(0), ok(true) {}

		uint8 U8();
		uint16 U16() { const uint16 a = U8(); return (uint16)(a | (U8() << 8)); }
		uint32 U32() { const uint32 a = U16(); return a | ((uint32)U16() << 16); }
		uint64 U64() { const uint64 a = U32(); return a | ((uint64)U32() << 32); }
		f32 F32();
		bool Bool() { return U8() != 0; }
		uint32 VarU32();
		int32 VarI32() { const uint32 u = VarU32(); return (int32)((u >> 1) ^ (0u - (u & 1u))); }
		std::string String();
		bool Bytes(void* out, const size_t count);
		Vec3 Vec3Raw() { const f32 x = F32(); const f32 y = F32(); return Vec3(x, y, F32()); }
		Vec3 Position(const NetQuantization &q = NetQuantization());
		Quaternion Rotation();

		bool Ok() const { return ok; }
		bool AtEnd() const { return at >= n; }
		size_t Remaining() const { return at < n ? n - at : 0; }

	private:
		const uchar* p;
		size_t n, at;
		bool ok;
	};

}

#endif /* NETBUFFER_H */
