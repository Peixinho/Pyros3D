//============================================================================
// Name        : Cube
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Cube Geometry
//============================================================================

#ifndef CUBE_H
#define CUBE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

    class PYROS3D_API Cube : public Primitive {

        public:

            Cube(const f32 width, const f32 height, const f32 depth, bool smooth = false, bool flip = false, bool TangentBitangent = false)
            {
                isFlipped = flip;
                isSmooth = smooth;
                calculateTangentBitangent = TangentBitangent;
                
                f32 w2 = width;
                f32 h2 = height;
                f32 d2 = depth;

                // Front Face
                Vec3 a = Vec3(-w2,-h2,d2); Vec3 b = Vec3(w2,-h2,d2); Vec3 c = Vec3(w2,h2,d2); Vec3 d = Vec3(-w2,h2,d2);
                Vec3 normal = ((c-b).cross(a-b)).normalize();
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));
                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));                

                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));
                geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0)); 

                // Right Face
                a = Vec3(w2,-h2,-d2); b = Vec3(w2,h2,-d2); c = Vec3(w2,h2,d2); d = Vec3(w2,-h2,d2);
                normal = ((c-b).cross(a-b)).normalize();        
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));
                geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));
                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));                

                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));
                geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));               


                // Top Face
                a = Vec3(-w2,h2,-d2); b = Vec3(-w2,h2,d2); c = Vec3(w2,h2,d2); d = Vec3(w2,h2,-d2);
                normal = ((c-b).cross(a-b)).normalize();
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));
                geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));                

                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));
                geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));        

                // Left Face
                a = Vec3(-w2,-h2,-d2); b = Vec3(-w2,-h2,d2); c = Vec3(-w2,h2,d2); d = Vec3(-w2,h2,-d2);
                normal = ((c-b).cross(a-b)).normalize();
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));
                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));                

                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));
                geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));           

                // Bottom Face
                a = Vec3(-w2,-h2,-d2); b = Vec3(w2, -h2, -d2); c = Vec3(w2,-h2,d2); d = Vec3(-w2,-h2,d2);
                normal = ((c-b).cross(a-b)).normalize();
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));
                geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));
                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));                

                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));                  

                // Back Face
                a = Vec3(-w2,-h2,-d2); b = Vec3(-w2,h2,-d2); c = Vec3(w2,h2,-d2); d = Vec3(w2,-h2,-d2);
                normal = ((c-b).cross(a-b)).normalize();
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));
                geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,1));
                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));                

                geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,1));
                geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1,0));

                for (uint32 i=0;i<geometry->tVertex.size();i++)
                {
                    geometry->index.push_back(i);
                    geometry->tTexcoord[i].y = 1-geometry->tTexcoord[i].y;
                }

                // Build and Send Buffers
                Build();
            }

    };
};

 #endif /* CUBE_H */