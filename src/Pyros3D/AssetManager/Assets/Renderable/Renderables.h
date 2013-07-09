//============================================================================
// Name        : Rendering Cone Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Cone Component
//============================================================================

#ifndef RENDERABLES_H
#define RENDERABLES_H

#include "../../../Ext/StringIDs/StringID.hpp"
#include "../../../Core/Math/Math.h"
#include "../../../Core/Buffers/GeometryBuffer.h"
#include "../IAsset.h"
#include <map>
#include <vector>
#include <list>

namespace p3d {

    namespace Renderables {

        // Store Each Submesh Material Properties
        struct MaterialProperties
        {
            // Material ID
            uint32 id;
            // Material Name
            std::string Name;
            // Properties
            Vec4 Color;
            Vec4 Specular;
            Vec4 Ambient;
            Vec4 Emissive;
            bool WireFrame;
            bool Twosided;
            f32 Opacity;
            f32 Shininess;
            f32 ShininessStrength;
            // textures        
            std::string colorMap;
            std::string specularMap;
            std::string normalMap;
            // flags
            bool haveColor;
            bool haveSpecular;
            bool haveAmbient;
            bool haveEmissive;        
            bool haveColorMap;
            bool haveSpecularMap;
            bool haveNormalMap;
            bool haveBones;

            MaterialProperties() : haveColor(false), haveSpecular(false), haveAmbient(false),haveColorMap(false),haveEmissive(false),haveNormalMap(false),haveSpecularMap(false), Opacity(1.0f), Shininess(0.0f), ShininessStrength(0.0f) {}
        };
        
        // Vertex Attribute Struct
        struct VertexAttribute {
            // Attribute Name
            std::string Name;
            // Attribute Type
            uint32 Type;
            // Attribute Offset
            uint32 Offset;
            // Type Offset
            uint32 byteSize;
            // Attribute Data
            std::vector<uchar> Data;
            // Data Length
            uint32 DataLength;

            VertexAttribute() {};
            // ID = -2 because 0 is the first and -1 is "not found"         
            VertexAttribute(const std::string &name, uint32 type, void* data, const uint32 &length) : Name(name), Type(type), DataLength(length) 
            {                           
                // Copy Data
                switch(Type)
                {
                    case Buffer::Attribute::Type::Float:         
                            byteSize=sizeof(f32);
                            break;
                    case Buffer::Attribute::Type::Int:
                            byteSize=sizeof(int);
                            break;
                    case Buffer::Attribute::Type::Matrix:                        
                            byteSize=sizeof(Matrix);
                            break;
                    case Buffer::Attribute::Type::Vec2:                        
                            byteSize=sizeof(Vec2);							
                            break;
                    case Buffer::Attribute::Type::Vec3:
                            byteSize=sizeof(Vec3);
                            break;
                    case Buffer::Attribute::Type::Vec4:
                            byteSize=sizeof(Vec4);
                            break;
                };
                Data.resize(DataLength*byteSize);
                memcpy(&Data[0],data,DataLength*byteSize);

            }
        };

        // Attributes Buffer
        struct AttributeBuffer {
            // Buffer
            GeometryBuffer* Buffer;
            uint32 bufferType;
            uint32 bufferDraw;
            // Attributes List
            std::list<VertexAttribute*> Attributes;
            // Data
            std::vector<uchar> Data;
            // Offset
            uint32 BufferOffset;
            // Attributes Size
            uint32 attributeSize;

            AttributeBuffer() : attributeSize(0) {}
            AttributeBuffer(const uint32 &type, const uint32 &draw) : attributeSize(0) { bufferDraw = draw; bufferType = type; }

            void AddAttribute(const std::string &name, const uint32 &type, void* data, const uint32 &length) 
            {
                VertexAttribute* v (new VertexAttribute(name, type, data, length));
                Attributes.push_back(v);
            }

            void SendBuffer()
            {
                uint32 offset=0;
                uint32 count = 0;
                for (std::list<VertexAttribute*>::iterator k = Attributes.begin();k!=Attributes.end();k++)
                {
                    (*k)->Offset = offset;
                    offset += (*k)->byteSize;
                    count = (*k)->DataLength;
                }
                BufferOffset=offset;

                // Resize Data
                Data.resize(BufferOffset*count);
                // Run through attributes data            
                for (uint32 l=0;l<count;l++)
                {                
                    offset = BufferOffset*l;               
                    // Run through all attributes
                    for (std::list<VertexAttribute*>::iterator k = Attributes.begin();k!=Attributes.end();k++)
                    {
                        memcpy(&Data[offset+(*k)->Offset],&(*k)->Data[(l*(*k)->byteSize)],(*k)->byteSize);
                    }
                }
                // Create Buffer
                Buffer = new GeometryBuffer(bufferType,bufferDraw);
                // Send Buffer
                Buffer->Init(&Data[0],Data.size());      
            }
            
            void Dispose()
            {
                // Loop Through each Vertex Attribute and Delete Them
                for (std::list<VertexAttribute*>::iterator k = Attributes.begin();k!=Attributes.end();k++)
                {
                    // Delete Vertex Attribute
                    delete *k;
                }
            }
        };

        // Geometry Interface Keeps Index and Attributes Buffer
        // Creates a Simple Material Properties
        class IGeometry {
            
            public:
                
                // Index Buffer
                GeometryBuffer* IndexBuffer;
                // Index Data
                std::vector<uint32> index;
                // Attributes Buffer
                std::vector<AttributeBuffer*> AttributesBuffer;

                // Material Properties
                MaterialProperties materialProperties;
                
                // Virtual Methods
                virtual std::vector<uint32> &GetIndexData() = 0;
                virtual std::vector<Vec3> &GetVertexData() = 0;
                virtual f32 &GetBoundingSphereRadius() { return BoundingSphereRadius; }
                virtual Vec3 &GetBoundingSphereCenter() { return BoundingSphereCenter; }
                virtual Vec3 &GetBoundingMinValue() { return minBounds; }
                virtual Vec3 &GetBoundingMaxValue() { return maxBounds; }
                
                virtual void CalculateBounding() = 0;
                
                void SendBuffers()
                {
                    // create and send index buffer
                    IndexBuffer = new GeometryBuffer(Buffer::Type::Index, Buffer::Draw::Static);
                    IndexBuffer->Init( &index[0], sizeof(uint32)*index.size());

                    // send attribute buffers
                    for (std::vector<AttributeBuffer*>::iterator i=AttributesBuffer.begin();i!=AttributesBuffer.end();i++)
                    {
                        (*i)->SendBuffer();
                    }
                }

                void Dispose()
                {
                    // Delete Index Buffer
                    delete IndexBuffer;
                    // Loop Through Attributes Buffer and Delete Each One
                    for (std::vector<AttributeBuffer*>::iterator i=AttributesBuffer.begin();i!=AttributesBuffer.end();i++)
                    {
                        // Dipose Vertex Attributes
                        (*i)->Dispose();
                        // Delete Pointer
                        delete *i;
                    }
                }
                
            protected:
                
                f32 BoundingSphereRadius;
                Vec3 BoundingSphereCenter;
                Vec3 maxBounds, minBounds;
                
        };

        // Keeps the Geometry List
        class Renderable : public IAsset {
            
            public:
            
                Renderable() {}
                virtual ~Renderable() {}
            
                // Vector of Buffers
                std::vector<IGeometry*> Geometries;

                virtual void Dispose()
                {
                    for(std::vector<IGeometry*>::iterator i = Geometries.begin();i!=Geometries.end();i++)
                    {
                        // Dispose Buffer
                        (*i)->Dispose();
                        // Delete Pointer
                        delete *i;
                    }
                }
                
                virtual void CalculateBounding() { 
                    // Get Bounding Boxes
                    for(std::vector<IGeometry*>::iterator i = Geometries.begin();i!=Geometries.end();i++)
                    {
                        if ((*i)->GetBoundingMaxValue()>maxBounds) maxBounds = (*i)->GetBoundingMaxValue();
                        if ((*i)->GetBoundingMinValue()>minBounds) minBounds = (*i)->GetBoundingMinValue();
                    }
                    // Set Sphere Radius and Bounds
                    BoundingSphereCenter = maxBounds-minBounds;
                    f32 a = maxBounds.distance(BoundingSphereCenter);
                    f32 b = minBounds.distance(BoundingSphereCenter);        
                    BoundingSphereRadius = (a>b?a:b);
                }
                
                // Bounds of the Whole Model
                virtual f32 &GetBoundingSphereRadius() { return BoundingSphereRadius; }
                virtual Vec3 &GetBoundingSphereCenter() { return BoundingSphereCenter; }
                virtual Vec3 &GetBoundingMinValue() { return minBounds; }
                virtual Vec3 &GetBoundingMaxValue() { return maxBounds; }
                
            protected:
                
                // Bounds of the Whole Model
                f32 BoundingSphereRadius;
                Vec3 BoundingSphereCenter;
                Vec3 maxBounds, minBounds;
                
        };
    
    };
};
 #endif /* RENDERABLES_H */