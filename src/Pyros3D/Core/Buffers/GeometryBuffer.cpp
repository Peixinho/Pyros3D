//============================================================================
// Name        : GeometryBuffer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GeometryBuffer
//============================================================================

#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

namespace p3d {

	// Every GeometryBuffer shares whichever backend is currently active
	// (see GetActiveRenderDevice() in IRenderDevice.h) - avoids plumbing
	// an IRenderDevice* through every call site that constructs a
	// GeometryBuffer (there's no IRenderer/IRenderDevice reference
	// available at most of those - asset loading, mesh construction, etc.)
	// while still respecting the actual backend in use (GL vs Vulkan).
	static IRenderDevice& Device()
	{
		return GetActiveRenderDevice();
	}

	GeometryBuffer::GeometryBuffer() : ID(-1), DataLength(0) {}

	GeometryBuffer::GeometryBuffer(const uint32 bufferType, const uint32 bufferDraw) : ID(-1)
	{
		this->bufferType = bufferType;
		this->bufferDraw = bufferDraw;
	}

	GeometryBuffer::~GeometryBuffer()
	{
		if (ID != -1) {
			Device().DestroyBuffer(ID);
		}
	}

	void GeometryBuffer::Init(const void* GeometryData, const uint32 length)
	{
		// Destroy buffer if exists
		if (ID != -1) {
			Device().DestroyBuffer(ID);
			ID = -1;
		}

		// creating buffer
		ID = Device().CreateBuffer(bufferType, bufferDraw, GeometryData, length);

		DataLength = length;
	}

	// Updates Buffer
	void GeometryBuffer::Update(const void* GeometryData, const uint32 length)
	{
		if (length != DataLength)
		{
			DataLength = length;
			Device().ReallocateBuffer(ID, bufferType, bufferDraw, GeometryData, DataLength);
		}
		else {
			Device().UpdateBufferSubData(ID, bufferType, GeometryData, DataLength);
		}
	}

	void *GeometryBuffer::Map(const uint32 MappingType)
	{
		return Device().MapBuffer(ID, bufferType, MappingType);
	}
	void GeometryBuffer::Unmap()
	{
		Device().UnmapBuffer(ID, bufferType);
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
			const uint32 GetType(const uint32 type)
			{
				return Device().TranslateAttributeType(type);
			}
		}
	}
}
