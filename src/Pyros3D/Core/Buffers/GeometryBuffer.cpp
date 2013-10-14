//============================================================================
// Name        : GeometryBuffer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GeometryBuffer
//============================================================================

#include "GeometryBuffer.h"
#include "GL/glew.h"

#define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace p3d {

    GeometryBuffer::GeometryBuffer() : ID(-1), DataLength(0) {}

    GeometryBuffer::GeometryBuffer(const uint32& bufferType, const uint32& bufferDraw) : ID(-1) 
    {

        // getting buffer type
        switch (bufferType)
        {

            case Buffer::Type::Index:
                    this->bufferType=GL_ELEMENT_ARRAY_BUFFER;
                    break;
            case Buffer::Type::Vertex:
                    this->bufferType=GL_ARRAY_BUFFER; 
                    break;
            case Buffer::Type::Attribute:
                    this->bufferType=GL_ARRAY_BUFFER; 
                    break;

        }

        // getting buffer draw type            
        switch (bufferDraw)
        {

            case Buffer::Draw::Static:
                    this->bufferDraw=GL_STATIC_DRAW;
                    break;
            case Buffer::Draw::Dynamic:
                    this->bufferDraw=GL_DYNAMIC_DRAW; 
                    break;
            case Buffer::Draw::Stream:
                    this->bufferDraw=GL_STREAM_DRAW; 
                    break;

        }
    }

    GeometryBuffer::~GeometryBuffer() 
    {
        if (ID!=-1) {
            glDeleteBuffers(1, (GLuint*)&ID);
        }
    }

    void GeometryBuffer::Init(const void* GeometryData, uint32 length)
    {
        // Destroy buffer if exists
        if (ID!=-1) {
            glBindBuffer(this->bufferType, ID);
            glDeleteBuffers(1, (GLuint*)&ID);
            glBindBuffer(this->bufferType, 0);
            ID=-1;
        }
        
        // creating buffer
        glGenBuffers(1, (GLuint*)&ID);
        glBindBuffer(this->bufferType, ID); 
        glBufferData(this->bufferType, length, GeometryData, this->bufferDraw);
        glBindBuffer(this->bufferType, 0);
              
        // copy geometry data
        this->GeometryData.resize(length);
        memcpy(&this->GeometryData[0], GeometryData, length);
        DataLength=length;
        
    }    

    // Updates Buffer
    void GeometryBuffer::Update(const void* GeometryData)
    {        
        this->GeometryData.clear();
        
        this->GeometryData.resize(DataLength);
        memcpy(&this->GeometryData[0], GeometryData, DataLength);
        
        // Updating buffer
        glBufferData(this->bufferType, DataLength, GeometryData, this->bufferDraw);

    }

    const std::vector<uchar> &GeometryBuffer::GetGeometryData() const
    {
        return GeometryData;
    }
    
    void *GeometryBuffer::Map(uint32 MappingType)
    {
        glBindBuffer(this->bufferType, ID); 
        
        switch (MappingType)
        {
            case Buffer::Mapping::Read:
                MappingType = GL_READ_ONLY;
                break;
            case Buffer::Mapping::Write:
                MappingType = GL_WRITE_ONLY;
                break;
            case Buffer::Mapping::ReadAndWrite:
                MappingType = GL_READ_WRITE;
                break;
        }
        
        void* vboData = glMapBuffer(bufferType,MappingType);
        if (vboData) 
        {
            glBindBuffer(this->bufferType, 0);
            return vboData;
        } else if (!vboData) 
        {
            glGetBufferPointerv(this->bufferType, GL_BUFFER_MAP_POINTER, &vboData);
            glBindBuffer(this->bufferType, 0);
            if (vboData) return vboData;
        }
        return NULL;
                        
    }
    void GeometryBuffer::Unmap()
    {
        glBindBuffer(this->bufferType, ID);
        glUnmapBuffer(this->bufferType);
        glBindBuffer(this->bufferType, 0);
    }
    
    namespace Buffer {
        
        namespace Attribute {
            
            uint32 GetTypeSize(uint32 type)
            {
                switch(type) {

                    case Buffer::Attribute::Type::Int:
                        return sizeof(int);
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
            uint32 GetTypeCount(uint32 type)
            {
                switch(type) {

                    case Buffer::Attribute::Type::Int:
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
                        return 16;
                        break;
                }
                return 0;
            }
            uint32 GetType(uint32 type)
            {
                switch(type) {

                    case Buffer::Attribute::Type::Int:
                        return GL_INT;
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
