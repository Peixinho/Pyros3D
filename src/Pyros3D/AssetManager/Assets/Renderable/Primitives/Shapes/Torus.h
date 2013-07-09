//============================================================================
// Name        : Torus
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Torus Geometry
//============================================================================

#ifndef TORUS_H
#define TORUS_H

#include "../Primitive.h"

namespace p3d {
    
    class Torus : public Primitive {
        
        public:
            
            Torus(const f32 &radius, const f32 &tube, const uint32 &segmentsW = 60, const uint32 segmentsH = 6, bool smooth = false)
            {
                Vec3 normal;

                int i, j;
                std::vector <std::vector<Vec3> > aVtc;  

                for (i=0;i<segmentsW;++i) {

                    std::vector<Vec3> aRow;
                    Vec3 oVtx;

                    for (j=0;j<segmentsH;++j) {
                        f32 u = (f32)((f32)i/segmentsW*2*PI);
                        f32 v = (f32)((f32)j/segmentsH*2*PI);
                        f32 x = (radius + tube * cos(v)) * cos(u);
                        f32 y = tube * sin(v);
                        f32 z = (radius + tube * cos(v)) * sin(u);

                        oVtx = Vec3(x,y,z);
                        aRow.push_back(oVtx);

                    }
                    aVtc.push_back(aRow);
                }
                for (i=0;i<segmentsW;++i) {
                    for (j=0;j<segmentsH;++j) {
                        int ip = (i+1)%segmentsW;
                        int jp = (j+1)%segmentsH;
                        Vec3 a = aVtc[i ][j];
                        Vec3 b = aVtc[ip][j];
                        Vec3 c = aVtc[i ][jp];
                        Vec3 d = aVtc[ip][jp];

                        Vec2 aUV = Vec2((f32)i/segmentsW,         -(f32)j/segmentsH);
                        Vec2 bUV = Vec2((f32)(i+1)/segmentsW,     -(f32)j/segmentsH);
                        Vec2 cUV = Vec2((f32)i/segmentsW,         -(f32)(j+1)/segmentsH);
                        Vec2 dUV = Vec2((f32)(i+1)/segmentsW,     -(f32)(j+1)/segmentsH);

                        normal = ((a-b).cross(c-b)).normalize();
                        geometry->tVertex.push_back(c);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(cUV);
                        geometry->tVertex.push_back(b);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(bUV);
                        geometry->tVertex.push_back(a);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aUV);

                        geometry->index.push_back(geometry->tVertex.size()-3);
                        geometry->index.push_back(geometry->tVertex.size()-2);
                        geometry->index.push_back(geometry->tVertex.size()-1);

                        normal = ((d-c).cross(b-c)).normalize();
                        geometry->tVertex.push_back(b);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(bUV);
                        geometry->tVertex.push_back(c);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(cUV);
                        geometry->tVertex.push_back(d);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(dUV);    

                        geometry->index.push_back(geometry->tVertex.size()-3);
                        geometry->index.push_back(geometry->tVertex.size()-2);
                        geometry->index.push_back(geometry->tVertex.size()-1);
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

 #endif /* TORUS_H */