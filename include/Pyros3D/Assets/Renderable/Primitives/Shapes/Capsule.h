//============================================================================
// Name        : Capsule
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Capsule Geometry
//============================================================================

#ifndef CAPSULE_H
#define CAPSULE_H

#include "../Primitive.h"

namespace p3d {

    class PYROS3D_API Capsule : public Primitive {

        public:

            Capsule(const f32 &radius, const f32 &height, const uint32 &numRings, const uint32 &segmentsW, const uint32 &segmentsH, bool smooth = false, bool flip = false)
            {
                isFlipped = flip;
                f32 fDeltaRingAngle = (f32)(PI/2/numRings);
                f32 fDeltaSegAngle = (f32)(PI*2/segmentsW);

                // TOP
                f32 sphereRatio = radius / (2*radius+height);
                f32 cylinderRatio = height / (2*radius+height);
                int offset=0;

                for (uint32 ring = 0;ring<=numRings; ring++) 
                {
                    f32 r0 = radius * sin(ring*fDeltaRingAngle);
                    f32 y0 = radius * cos(ring*fDeltaRingAngle);

                    for (uint32 seg = 0;seg<=segmentsW; seg++)
                    {
                        f32 x0 = r0 * cos(seg * fDeltaSegAngle);
                        f32 z0 = r0 * sin(seg * fDeltaSegAngle);

//            vertexData.push_back(BasicGeometry::VertexData(Vec3(x0,0.5f*height+y0,z0),Vec3(x0,y0,z0),Vec2((f32)seg/(f32)segmentsW, (f32)ring/(f32)numRings*sphereRatio)));
                        geometry->tVertex.push_back(Vec3(x0,0.5f*height+y0,z0));
                        geometry->tNormal.push_back(Vec3(x0,y0,z0));
                        geometry->tTexcoord.push_back(Vec2((f32)seg/(f32)segmentsW, (f32)ring/(f32)numRings*sphereRatio));

                        geometry->index.push_back(offset+segmentsW+1);
                        geometry->index.push_back(offset+segmentsW);
                        geometry->index.push_back(offset);
                        geometry->index.push_back(offset+segmentsW+1);
                        geometry->index.push_back(offset);
                        geometry->index.push_back(offset+1); 

                        offset++;
                    }
                }

                // Cylinder

                f32 deltaAngle = (f32)(PI*2 / segmentsW);
                f32 deltaheight = height/(f32)segmentsH;

                for (uint32 i = 1;i<segmentsH; i++)
                    for (uint32 j=0;j<=segmentsW; j++) 
                    {
                        f32 x0 = radius * cos(j*deltaAngle);
                        f32 z0 = radius * sin(j*deltaAngle);
//            vertexData.push_back(BasicGeometry::VertexData(Vec3(x0,0.5f*height-i*deltaheight,z0),Vec3(x0,0,z0),Vec2((f32)j/(f32)segmentsW, (f32)i/(f32)segmentsH*cylinderRatio+sphereRatio)));
                        geometry->tVertex.push_back(Vec3(x0,0.5f*height-i*deltaheight,z0));         
                        geometry->tNormal.push_back(Vec3(x0,0,z0));
                        geometry->tTexcoord.push_back(Vec2((f32)j/(f32)segmentsW, (f32)i/(f32)segmentsH*cylinderRatio+sphereRatio));

                        geometry->index.push_back(offset+segmentsW+1);
                        geometry->index.push_back(offset+segmentsW);
                        geometry->index.push_back(offset);
                        geometry->index.push_back(offset+segmentsW+1);
                        geometry->index.push_back(offset);
                        geometry->index.push_back(offset+1);

                        offset++;
                    }

                // Bottom
                for (uint32 ring = 0;ring <= numRings; ring++) 
                {
                    f32 r0 = (f32)(radius * sin(PI/2 + ring*fDeltaRingAngle));
                    f32 y0 = (f32)(radius * cos(PI/2 + ring*fDeltaRingAngle));

                    for (uint32 seg = 0;seg<=segmentsW; seg++)
                    {
                        f32 x0 = r0 * cos(seg * fDeltaSegAngle);
                        f32 z0 = r0 * sin(seg * fDeltaSegAngle);            
                        //vertexData.push_back(BasicGeometry::VertexData(Vec3(x0,-0.5f*height+y0,z0),Vec3(x0,y0,z0),Vec2((f32)seg/(f32)segmentsW, (f32)ring/(f32)numRings*sphereRatio+cylinderRatio+sphereRatio)));
                        geometry->tVertex.push_back(Vec3(x0,-0.5f*height+y0,z0));
                        geometry->tNormal.push_back(Vec3(x0,y0,z0));
                        geometry->tTexcoord.push_back(Vec2((f32)seg/(f32)segmentsW, (f32)ring/(f32)numRings*sphereRatio+cylinderRatio+sphereRatio));

                        if (ring != numRings)
                        {            
                            geometry->index.push_back(offset+segmentsW+1);
                            geometry->index.push_back(offset+segmentsW);
                            geometry->index.push_back(offset);
                            geometry->index.push_back(offset+segmentsW+1);
                            geometry->index.push_back(offset);
                            geometry->index.push_back(offset+1);
                        }

                        offset++;
                    }
                }

                for (uint32 i=0;i<geometry->tVertex.size();i++)
                {
                    geometry->tTexcoord[i].x = 1-geometry->tTexcoord[i].x;
                }

                Build();
            }

    };
};

 #endif /* CAPSULE_H */