//============================================================================
// Name        : Plane
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Plane Geometry
//============================================================================

#ifndef PLANE_H
#define PLANE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {
   
    class PYROS3D_API Plane : public Primitive {
        
        public:

            Plane(const f32 width, const f32 height, bool smooth = false, bool flip = false, bool TangentBitangent = false)
            {
                isFlipped = flip;
                isSmooth = smooth;
                calculateTangentBitangent = TangentBitangent;
                
                f32 w2 = width; f32 h2 = height;       

                Vec3 a = Vec3(-w2,-h2,0); Vec3 b = Vec3(w2,-h2,0); Vec3 c = Vec3(w2,h2,0); Vec3 d = Vec3(-w2,h2,0);
                Vec3 normal = ((c-b).cross(a-b)).normalize();    
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));
                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));                
                geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));    

                geometry->index.push_back(0);
                geometry->index.push_back(1);
                geometry->index.push_back(2);
                geometry->index.push_back(2);
                geometry->index.push_back(3);
                geometry->index.push_back(0);

                for (size_t i = 0;i < geometry->tTexcoord.size(); i++) geometry->tTexcoord[i].y = 1-geometry->tTexcoord[i].y;

                // Build and Send Buffers
                Build();

				// Bounding Box
				minBounds = Vec3(-w2, -h2, 0);
				maxBounds = Vec3(w2, h2, 0);

				// Bounding Sphere
				BoundingSphereCenter = Vec3(0, 0, 0);
				BoundingSphereRadius = Max(w2,h2);
			}

			virtual void CalculateBounding()
			{

			}

    };
};

 #endif /* PLANE_H */