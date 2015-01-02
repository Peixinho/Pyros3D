//============================================================================
// Name        : Primitive.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Interface to Create Primitives Shapes
//============================================================================

#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {
    
    class PYROS3D_API PrimitiveGeometry : public IGeometry {

        public:
            std::vector<Vec3> tVertex, tNormal;
            std::vector<Vec2> tTexcoord;

            void CreateBuffers()
            {
                // Calculate Bounding Sphere Radius
                CalculateBounding();

                // Create and Set Attribute Buffer
                AttributeBuffer* Vertex  = new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Static);
                Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tVertex[0],tVertex.size());
                Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3,&tNormal[0],tNormal.size());
                Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2,&tTexcoord[0],tTexcoord.size());
                // Add Buffer to Attributes Buffer List
                Attributes.push_back(Vertex);
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

class PYROS3D_API Primitive : public Renderable {

    public:

        PrimitiveGeometry* geometry;

        Primitive() { geometry = new PrimitiveGeometry(); isFlipped = false; isSmooth = false; }

        void Build() 
        {
            // Flip Normals
            if (isFlipped)
            {
                for (uint32 i=0;i<geometry->tNormal.size();i++)
                {
                    geometry->tNormal[i].negateSelf();
                }
            }

		if (isSmooth)
		{
			if (geometry->tNormal.size()>0)
			{
				std::vector<Vec3> CopyNormals;
				for (uint32 i=0;i<geometry->tNormal.size();i++) {
					CopyNormals.push_back(geometry->tNormal[i]);
				}
				for (uint32 i=0;i<geometry->tNormal.size();i++) {
					for (uint32 j=0;j<geometry->tNormal.size();j++) {
						if (i!=j && geometry->tVertex[i]==geometry->tVertex[j])
							geometry->tNormal[j]+=CopyNormals[i];            
				   }
				}
				for (uint32 i=0;i<geometry->tNormal.size();i++) {
					geometry->tNormal[i].normalizeSelf();
				}
			}
		}

            // Create Attributes Buffers
            geometry->CreateBuffers();
            // Send Buffers
            geometry->SendBuffers();

            // Material Defaults
            geometry->materialProperties.haveColor = true;
            geometry->materialProperties.haveBones = false;
            geometry->materialProperties.haveSpecular = false;
            geometry->materialProperties.haveColorMap = false;
            geometry->materialProperties.haveSpecularMap = false;
            geometry->materialProperties.haveNormalMap = false;
            geometry->materialProperties.Color = Vec4(1.0,1.0,1.0,1.0);

            // Add To Geometry List
            Geometries.push_back(geometry);

            // Calculate Model's Bounding Box
            CalculateBounding();
            
            // Execute Parent Build
            BuildMaterials();
        }

    protected:

        bool isSmooth;
        bool isFlipped;
    };
};

 #endif /* PRIMITIVE_H */
