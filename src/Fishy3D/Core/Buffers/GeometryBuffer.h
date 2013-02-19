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
#include <iostream>

namespace Fishy3D {

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
        
        unsigned int GetTypeSize(unsigned type);
        unsigned int GetTypeCount(unsigned type);
        unsigned long GetType(unsigned type);
        
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

            std::vector<unsigned char> GeometryData;

            unsigned long bufferType;
            unsigned long bufferDraw;

        public:    

            // OpenGL ID
            int ID;

            // Data Size
            unsigned long DataLength;

            GeometryBuffer();
            GeometryBuffer(const unsigned &bufferType, const unsigned &bufferDraw);
            ~GeometryBuffer();
            // methods
            void Init(  const void *GeometryData, unsigned long length );            

            void *Map(unsigned MappingType = 1);
            void Unmap();

            void Update( const void *GeometryData );

            const std::vector<unsigned char> &GetGeometryData() const;

    };

}

#endif	/* GEOMETRYBUFFER_H */
