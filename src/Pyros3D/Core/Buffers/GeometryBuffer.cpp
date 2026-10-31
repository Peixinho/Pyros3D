//============================================================================
// Name        : GeometryBuffer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GeometryBuffer
//============================================================================

#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <cstring>

namespace p3d {

	// A buffer is its bytes, kept on the CPU: the renderer makes its GPU copy, and makes it again when cpuRevision moves
	GeometryBuffer::GeometryBuffer() : DataLength(0) {}

	GeometryBuffer::GeometryBuffer(const uint32 bufferType, const uint32 bufferDraw) : DataLength(0)
	{
		this->bufferType = bufferType;
		this->bufferDraw = bufferDraw;
	}

	GeometryBuffer::~GeometryBuffer() {}

	void GeometryBuffer::Init(const void* GeometryData, const uint32 length)
	{
		const uchar* p = (const uchar*)GeometryData;
		if (p) CpuData.assign(p, p + length); else CpuData.assign(length, 0);
		DataLength = length;
		cpuRevision++;
	}

	void GeometryBuffer::InitOwned(std::vector<uchar> &&GeometryData)
	{
		DataLength = (uint32)GeometryData.size();
		CpuData = std::move(GeometryData);
		cpuRevision++;
	}

	void GeometryBuffer::Update(const void* GeometryData, const uint32 length)
	{
		const uint32 n = length ? length : DataLength;
		if (CpuData.size() != n) CpuData.resize(n);
		if (GeometryData && n) memcpy(CpuData.data(), GeometryData, n);
		DataLength = n;
		cpuRevision++;
	}

	void *GeometryBuffer::Map(const uint32 MappingType)
	{
		return CpuData.empty() ? NULL : CpuData.data();
	}
	void GeometryBuffer::Unmap()
	{
		cpuRevision++;     // (what was written through Map: changed)
	}

	namespace Buffer {

		namespace Attribute {

			const uint32 GetTypeSize(const uint32 type)
			{
				switch (type) {

				case Buffer::Attribute::Type::Int:
					return sizeof(int32);
					break;
				case Buffer::Attribute::Type::Short:
					return sizeof(short);
					break;
				case Buffer::Attribute::Type::Float:
					return sizeof(f32);
					break;
				case Buffer::Attribute::Type::Vec2:
					return sizeof(Vec2);
					break;
				case Buffer::Attribute::Type::Vec3:
					return sizeof(Vec3);
					break;
				case Buffer::Attribute::Type::Vec4:
					return sizeof(Vec4);
					break;
				case Buffer::Attribute::Type::Matrix:
					return sizeof(Matrix);
					break;
				}
				return 0;
			}
			const uint32 GetTypeCount(const uint32 type)
			{
				switch (type) {

				case Buffer::Attribute::Type::Int:
					return 1;
					break;
				case Buffer::Attribute::Type::Short:
					return 1;
					break;
				case Buffer::Attribute::Type::Float:
					return 1;
					break;
				case Buffer::Attribute::Type::Vec2:
					return 2;
					break;
				case Buffer::Attribute::Type::Vec3:
					return 3;
					break;
				case Buffer::Attribute::Type::Vec4:
					return 4;
					break;
				case Buffer::Attribute::Type::Matrix:
					return 4;
					break;
				}
				return 0;
			}
		}
	}
}
