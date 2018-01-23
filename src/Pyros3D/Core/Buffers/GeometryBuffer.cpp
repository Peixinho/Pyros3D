//============================================================================
// Name        : GeometryBuffer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GeometryBuffer
//============================================================================

#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	GeometryBuffer::GeometryBuffer() : ID(-1), DataLength(0) {}

	GeometryBuffer::GeometryBuffer(const uint32 bufferType, const uint32 bufferDraw) : ID(-1)
	{

		// getting buffer type
		switch (bufferType)
		{

		case Buffer::Type::Index:
			this->bufferType = GL_ELEMENT_ARRAY_BUFFER;
			break;
		case Buffer::Type::Vertex:
			this->bufferType = GL_ARRAY_BUFFER;
			break;
		case Buffer::Type::Attribute:
			this->bufferType = GL_ARRAY_BUFFER;
			break;

		}

		// getting buffer draw type            
		switch (bufferDraw)
		{

		case Buffer::Draw::Static:
			this->bufferDraw = GL_STATIC_DRAW;
			break;
		case Buffer::Draw::Dynamic:
			this->bufferDraw = GL_DYNAMIC_DRAW;
			break;
		case Buffer::Draw::Stream:
			this->bufferDraw = GL_STREAM_DRAW;
			break;

		}
	}

	GeometryBuffer::~GeometryBuffer()
	{
		if (ID != -1) {
			GLCHECKER(glDeleteBuffers(1, (GLuint*)&ID));
		}
	}

	void GeometryBuffer::Init(const void* GeometryData, const uint32 length)
	{
		// Destroy buffer if exists
		if (ID != -1) {
			GLCHECKER(glBindBuffer(this->bufferType, ID));
			GLCHECKER(glDeleteBuffers(1, (GLuint*)&ID));
			GLCHECKER(glBindBuffer(this->bufferType, 0));
			ID = -1;
		}

		// creating buffer
		GLCHECKER(glGenBuffers(1, (GLuint*)&ID));
		GLCHECKER(glBindBuffer(this->bufferType, ID));
		GLCHECKER(glBufferData(this->bufferType, length, GeometryData, this->bufferDraw));
		GLCHECKER(glBindBuffer(this->bufferType, 0));

		// copy geometry data
		if (length > 0)
		{
			this->GeometryData.resize(length);
			memcpy(&this->GeometryData[0], GeometryData, length);
			DataLength = length;
		}
		else DataLength = 0;
	}

	// Updates Buffer
	void GeometryBuffer::Update(const void* GeometryData, const uint32 length)
	{
		if (length != DataLength)
		{
			DataLength = length;

			this->GeometryData.clear();
			this->GeometryData.resize(DataLength);
			memcpy(&this->GeometryData[0], GeometryData, DataLength);

			GLCHECKER(glBindBuffer(this->bufferType, ID));
			#if !defined(GLES2) && !defined(GLES3) && !defined(GLLEGACY)
				GLCHECKER(glInvalidateBufferData(ID));
			#endif
			GLCHECKER(glBufferData(this->bufferType, DataLength, GeometryData, this->bufferDraw));
			GLCHECKER(glBindBuffer(this->bufferType, 0));
		}
		else {

			this->GeometryData.clear();
			this->GeometryData.resize(DataLength);
			memcpy(&this->GeometryData[0], GeometryData, DataLength);

			// Updating buffer
			GLCHECKER(glBindBuffer(this->bufferType, ID));
			#if !defined(GLES2) && !defined(GLES3) && !defined(GLLEGACY)
				GLCHECKER(glInvalidateBufferData(ID));
			#endif
			glBufferSubData(this->bufferType, 0, DataLength, GeometryData);
			GLCHECKER(glBindBuffer(this->bufferType, 0));
		}
	}

	const std::vector<uchar> &GeometryBuffer::GetGeometryData() const
	{
		return GeometryData;
	}

	void *GeometryBuffer::Map(const uint32 MappingType)
	{
#if !defined(GLES2) && !defined(GLES3)
		GLCHECKER(glBindBuffer(this->bufferType, ID));
		uint32 MP;
		switch (MappingType)
		{
		case Buffer::Mapping::Read:
			MP = GL_READ_ONLY;
			break;
		case Buffer::Mapping::Write:
			MP = GL_WRITE_ONLY;
			break;
		case Buffer::Mapping::ReadAndWrite:
			MP = GL_READ_WRITE;
			break;
		}

		void* vboData = glMapBuffer(bufferType, MP);
		if (vboData)
		{
			GLCHECKER(glBindBuffer(this->bufferType, 0));
			return vboData;
		}
		else if (!vboData)
		{
			GLCHECKER(glGetBufferPointerv(this->bufferType, GL_BUFFER_MAP_POINTER, &vboData));
			GLCHECKER(glBindBuffer(this->bufferType, 0));
			if (vboData) return vboData;
		}
#endif
		return NULL;

	}
	void GeometryBuffer::Unmap()
	{
#if !defined(GLES2)
		GLCHECKER(glBindBuffer(this->bufferType, ID));
		GLCHECKER(glUnmapBuffer(this->bufferType));
		GLCHECKER(glBindBuffer(this->bufferType, 0));
#endif
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
				switch (type) {

				case Buffer::Attribute::Type::Int:
					return GL_INT;
					break;
				case Buffer::Attribute::Type::Short:
					return GL_SHORT;
					break;
				case Buffer::Attribute::Type::Float:
				case Buffer::Attribute::Type::Vec2:
				case Buffer::Attribute::Type::Vec3:
				case Buffer::Attribute::Type::Vec4:
				case Buffer::Attribute::Type::Matrix:
					return GL_FLOAT;
					break;
				}
				return 0;
			}
		}
	}
}
