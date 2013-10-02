//============================================================================
// Name        : IRenderingComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Component Interface
//============================================================================

#ifndef IRENDERINGCOMPONENT_H
#define IRENDERINGCOMPONENT_H

#include <list>
#include "../IComponent.h"
#include "../../Core/Buffers/GeometryBuffer.h"
#include "../../Materials/IMaterial.h"
#include "../../Utils/Geometry/Geometry.h"
#include "../../SceneGraph/SceneGraph.h"

namespace Fishy3D {
    
    // Drawing Type
    namespace DrawingType
    {
        enum {
            Triangles = 0,
            Lines,
            Triangles_Fan,
            Triangles_Strip,
            Quads,
            Points,
            Polygons
        };
    }

    // Vertex Attribute Struct
    struct VertexAttribute {
        // OpenGL ID
        long ID;
        // Attribute Name
        std::string Name;
        // Attribute Type
        unsigned Type;
        // Attribute Offset
        unsigned int Offset;
        // Type Offset
        unsigned int byteSize;
        // Attribute Data
        std::vector<unsigned char> Data;
        // Data Length
        unsigned int DataLength;
        // Auxiliar ID for Multiple Materials
        long AuxID;
        
        VertexAttribute() {};
        // ID = -2 because 0 is the first and -1 is "not found"         
        VertexAttribute(const std::string &name, unsigned type, void* data, const unsigned int &length) : ID(-2), AuxID(-2), Name(name), Type(type), DataLength(length) 
        {                           
            // Copy Data
            switch(Type)
            {
                case Buffer::Attribute::Type::Float:         
                        byteSize=sizeof(float);
                        break;
                case Buffer::Attribute::Type::Int:
                        byteSize=sizeof(int);
                        break;
                case Buffer::Attribute::Type::Matrix:                        
                        byteSize=sizeof(Matrix);
                        break;
                case Buffer::Attribute::Type::Vec2:                        
                        byteSize=sizeof(vec2);							
                        break;
                case Buffer::Attribute::Type::Vec3:
                        byteSize=sizeof(vec3);
                        break;
                case Buffer::Attribute::Type::Vec4:
                        byteSize=sizeof(vec4);
                        break;
            };
            Data.resize(DataLength*byteSize);
            memcpy(&Data[0],data,DataLength*byteSize);
            
        }
    };

    // Attributes Buffer
    struct AttributeBuffer {
        // Buffer
        SuperSmartPointer<GeometryBuffer> Buffer;
        unsigned bufferType;
        unsigned bufferDraw;
        // Attributes List
        std::list<SuperSmartPointer <VertexAttribute> > Attributes;
        // Data
        std::vector<unsigned char> Data;
        // Offset
        unsigned int BufferOffset;
        // Attributes Size
        unsigned attributeSize;
        
        AttributeBuffer() : attributeSize(0) {}
        AttributeBuffer(const unsigned &type, const unsigned &draw) : attributeSize(0) { bufferDraw = draw; bufferType = type; }
        
        void AddAttribute(const std::string &name, const unsigned int &type, void* data, const unsigned int &length) 
        {
            SuperSmartPointer<VertexAttribute> v (new VertexAttribute(name, type, data, length));
            Attributes.push_back(v);
        }
        
        void SendBuffer()
        {
            unsigned int offset=0;
            unsigned int count = 0;
            for (std::list<SuperSmartPointer <VertexAttribute> >::iterator k = Attributes.begin();k!=Attributes.end();k++)
            {
                (*k)->Offset = offset;
                offset += (*k)->byteSize;
                count = (*k)->DataLength;
            }
            BufferOffset=offset;

            // Resize Data
            Data.resize(BufferOffset*count);
            // Run through attributes data            
            for (unsigned int l=0;l<count;l++)
            {                
                offset = BufferOffset*l;               
                // Run through all attributes
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator k = Attributes.begin();k!=Attributes.end();k++)
                {
                    memcpy(&Data[offset+(*k)->Offset],&(*k)->Data[(l*(*k)->byteSize)],(*k)->byteSize);
                }
            }
            // Create Buffer
            Buffer = SuperSmartPointer<GeometryBuffer> (new GeometryBuffer(bufferType,bufferDraw));
            // Send Buffer
            Buffer->Init(&Data[0],Data.size());      
        }
    };    

    class IRenderingComponent : public IComponent {
        
        friend class RenderingModelComponent;
        
        public:

            // DEBUG
            #if _DEBUG
            std::vector<unsigned> tIndexNormals, tIndexBounding;
            std::vector<vec3> tPositionNormals,tPositionBoundingBox;
            SuperSmartPointer<GeometryBuffer> IndexNormalsBuffer,IndexBoundingBuffer;
            SuperSmartPointer<AttributeBuffer> NormalsBuffer, BoundingBuffer;
            #endif            
            
            // material
            void SetMaterial(SuperSmartPointer<IMaterial> material);
            IMaterial* GetMaterial();

            IRenderingComponent();
            IRenderingComponent(const std::string &Name, SuperSmartPointer<IMaterial> Material, bool SNormals = true);
            virtual ~IRenderingComponent();    
            
            virtual void Start() = 0;
            virtual void Update() = 0;
            virtual void Shutdown() = 0;
            
            virtual void Register(void* ptr);
            virtual void UnRegister(void* ptr);
            
            void SetDrawingType(const unsigned int &type);
            unsigned int GetDrawingType();                       

            // bounding box values
            const vec3 &GetBoundingMin() const;
            const vec3 &GetBoundingMax() const;
            const vec3 &GetBoundingSphereCenter() const;
            const float &GetBoundingSphereRadius() const;
            
            // buffer objects
            SuperSmartPointer<GeometryBuffer> IndexBuffer;
            // index data
            std::vector <unsigned int> IndexData;
            // Attribute Buffers
            std::vector<SuperSmartPointer <AttributeBuffer> > Buffers;
            // Add Buffer
            void AddBuffer(SuperSmartPointer<AttributeBuffer> Buffer);            
            virtual void Build() = 0;
            void SendBuffers();
            
            // Active
            void Activate();
            void Deactivate();
            bool IsActive();

            // Smooth Normals
            bool HaveSmoothNormals();
            
            // Culling
            virtual void ActivateCulling();
            virtual void DeactivateCulling();
            bool IsCullingActive();
            
            // Cast Shadows
            // By default it does cast shadows
            bool IsCastingShadows();
            void StartCastingShadows();
            void StopCastingShadows();
            
            // Skinning
            bool HaveBones();
            
        private:
            
            // Clones List
            std::list<IRenderingComponent*> Clones;
            
            bool _culling;
            
        protected:
            
            virtual void SmoothNormals() {}
            
            // Skinning
            bool haveBones;
            
            // Cast Shadows
            bool CastShadows;
            
            // Active
            bool Active;
            
            // material
            SuperSmartPointer<IMaterial> material;

            // how to draw
            unsigned int drawType;
            
            // calculate Bounding values
            virtual void CalculateBounding() = 0;
            // bounding box values
            vec3 minBounds, maxBounds;
            // bounding sphere values
            vec3 boundSphereCenter;
            float boundSphereRadius;
            
            // Smooth Normals
            bool smoothNormals;

            // Send Debug VBOS (Normals and Bounding Box)
            void SendDebugVBOS();
            
        };

}

#endif	/* IRENDERINGCOMPONENT_H */
