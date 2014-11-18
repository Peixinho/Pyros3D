//============================================================================
// Name        : GeometryBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GeometryBuffer
//============================================================================

#ifndef GEOMETRYBUFFER_H
#define GEOMETRYBUFFER_H

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#include <vector>
#include <string.h>
#include "../Math/Math.h"
#include "../../Other/Export.h"
#include <iostream>

namespace p3d {

    namespace Buffer {
    
        namespace Attribute {
        
            // Data type
            namespace Type {
                enum {
                    Int = 0,
                    Float,
                    Vec2,
                    Vec3,
                    Vec4,
                    Matrix
                };
            };

            // Usage of the Data
            namespace Usage {
                enum {
                    Pos = 0,
                    TexCoord,
                    Color,
                    Normal,
                    Tangent,
                    Bitangent,
                    Other
            };
        };
        
        const uint32 GetTypeSize(const uint32 &type);
        const uint32 GetTypeCount(const uint32 &type);
        const uint32 GetType(const uint32 &type);
        
    };  
        
        // Type of Draw
        namespace Draw {
            enum {
                Static = 0,
                Dynamic,
                Stream
            };
        };

        // Type of Buffer
        namespace Type {
            enum {
                Index = 0,
                Vertex,
                Attribute
            };
        };

        // Type of Mapping
        namespace Mapping {
            enum {
                Write = 0,
                Read,
                ReadAndWrite
            };
        };
    };
    
    class GeometryBuffer {

        private:

            std::vector<uchar> GeometryData;

            uint32 bufferType;
            uint32 bufferDraw;

        public:    

            // OpenGL ID
            uint32 ID;

            // Data Size
            uint32 DataLength;

            GeometryBuffer();
            GeometryBuffer(const uint32 &bufferType, const uint32 &bufferDraw);
            ~GeometryBuffer();
            // methods
            void Init(  const void *GeometryData, const uint32 &length );            

            void *Map(const uint32 &MappingType = 1);
            void Unmap();

            void Update( const void *GeometryData );

            const std::vector<uchar> &GetGeometryData() const;

    };

}

#endif	/* GEOMETRYBUFFER_H */
