//============================================================================
// Name        : Text.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text
//============================================================================

#ifndef TEXT_H
#define	TEXT_H

#include "../Renderables.h"
#include "../../../../Core/Buffers/GeometryBuffer.h"
#include "../../Font/Font.h"
#include "../../../AssetManager.h"
#include <vector>

namespace p3d {

    namespace Renderables {
    
        class TextGeometry : public IGeometry {
        
            public:
                std::vector<Vec3> tVertex, tNormal;
                std::vector<Vec2> tTexcoord;

                void CreateBuffers()
                {
                    // Calculate Bounding Sphere Radius
                    CalculateBounding();
                    if (AttributesBuffer.size()==0)
                    {
                    // Create and Set Attribute Buffer
                    AttributeBuffer* Vertex  = new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Dynamic);
                    Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tVertex[0],tVertex.size());
                    Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3,&tNormal[0],tNormal.size());
                    Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2,&tTexcoord[0],tTexcoord.size());
                    // Add Buffer to Attributes Buffer List
                    AttributesBuffer.push_back(Vertex);
                    }
                }

                virtual std::vector<uint32> &GetIndexData()
                {
                    return index;
                }
                virtual std::vector<Vec3> &GetVertexData()
                {
                    return tVertex;
                }

            protected:

                void CalculateBounding()
                {
                    // Bounding Box
                    for (uint32 i=0;i<tVertex.size();i++)
                    {
                        if (i==0) {
                            minBounds = tVertex[i];
                            maxBounds = tVertex[i];
                        } else {
                            if (tVertex[i].x<minBounds.x) minBounds.x = tVertex[i].x;
                            if (tVertex[i].y<minBounds.y) minBounds.y = tVertex[i].y;
                            if (tVertex[i].z<minBounds.z) minBounds.z = tVertex[i].z;
                            if (tVertex[i].x>maxBounds.x) maxBounds.x = tVertex[i].x;
                            if (tVertex[i].y>maxBounds.y) maxBounds.y = tVertex[i].y;
                            if (tVertex[i].z>maxBounds.z) maxBounds.z = tVertex[i].z;
                        }
                    }
                    // Bounding Sphere
                    BoundingSphereCenter = maxBounds-minBounds;
                    f32 a = maxBounds.distance(BoundingSphereCenter);
                    f32 b = minBounds.distance(BoundingSphereCenter);        
                    BoundingSphereRadius = (a>b?a:b);
                }
        };
        
        class Text : public Renderable {
            public:
                
                TextGeometry* geometry;

                Text(const uint32 &Handle, const std::string& text, const f32 &charWidth, const f32 &charHeight);
                
                virtual ~Text();
            
                void Build() 
                {
                
                    // Create Attributes Buffers
                    geometry->CreateBuffers();
                    // Send Buffers
                    geometry->SendBuffers();

                    // Add To Geometry List
                    Geometries.push_back(geometry);

                    // Calculate Model's Bounding Box
                    CalculateBounding();
                    
                };
            
                void UpdateText(const std::string &text);
                
            private:
                
                // Char Dimensions
                f32 charWidth, charHeight;
                uint32 fontHandle;
        };
    };
};

#endif	/* TEXT_H */