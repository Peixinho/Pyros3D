//============================================================================
// Name        : Cylinder
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Cylinder Geometry
//============================================================================

#ifndef CYLINDER_H
#define CYLINDER_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

    class PYROS3D_API Cylinder : public Primitive {

        public:

            f32 segmentsH;
            f32 height;

            Cylinder(const f32 radius, const f32 height, const uint32 segmentsW, const uint32 segmentsH, bool openEnded, bool smooth = false, bool flip = false, bool TangentBitangent = false)
            {
                isFlipped = flip;
                isSmooth = smooth;
                calculateTangentBitangent = TangentBitangent;
                this->segmentsH = segmentsH;
                this->height = height;

                int i,j,jMin,jMax;

                Vec3 normal;
                std::vector <Vec3> aRowT, aRowB;

                std::vector <std::vector<Vec3> > aVtc;

                if(!openEnded) {
                        this->segmentsH += 2;
                        jMin = 1;
                        jMax = this->segmentsH -1;

                        // Bottom
                        Vec3 oVtx = Vec3(0, -this->height, 0);
                        for (i=0;i<segmentsW;++i) {
                            aRowB.push_back(oVtx);
                        }
                        aVtc.push_back(aRowB);

                        //Top
                        oVtx = Vec3(0,this->height,0);          
                        for (i=0;i<segmentsW;i++) {                
                            aRowT.push_back(oVtx);
                        }

                } else {
                    jMin = 0;
                    jMax = this->segmentsH;
                }

                for (j=jMin;j<=jMax;++j) {
                    f32 z = -this->height+2*this->height*(f32)(j-jMin)/(f32)(jMax-jMin);
                    std::vector <Vec3> aRow;

                    for (i=0;i<segmentsW;++i) {
                        f32 verangle = (f32)(2*(f32)i/segmentsW*PI);
                        f32 x = radius * sin(verangle);
                        f32 y = radius * cos(verangle);
                        Vec3 oVtx = Vec3(y,z,x);
                        aRow.push_back(oVtx);
                    }
                    aVtc.push_back(aRow);
                }

                if (!openEnded)
                    aVtc.push_back(aRowT);

                for (j=1;j<=this->segmentsH;++j) {
                    for (i=0;i<segmentsW;++i) {
                        Vec3 a = aVtc[j][i];
                        Vec3 b = aVtc[j][(i-1+segmentsW)%segmentsW];
                        Vec3 c = aVtc[j-1][(i-1+segmentsW)%segmentsW];
                        Vec3 d = aVtc[j-1][i];

                        int i2;
                        (i==0?i2=segmentsW:i2=i);

                        f32 vab = (f32)j/this->segmentsH;
                        f32 vcd = (f32)(j-1)/this->segmentsH;
                        f32 uad = (f32)i2/(f32)segmentsW;
                        f32 ubc = (f32)(i2-1)/(f32)segmentsW;

                        Vec2 aUV = Vec2(uad,-vab);
                        Vec2 bUV = Vec2(ubc,-vab);
                        Vec2 cUV = Vec2(ubc,-vcd);
                        Vec2 dUV = Vec2(uad,-vcd);                

                        normal = ((a-b).cross(c-b)).normalize();
                        geometry->tVertex.push_back(c);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(cUV);
                        geometry->tVertex.push_back(b);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(bUV);
                        geometry->tVertex.push_back(a);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aUV);

                        geometry->index.push_back(geometry->tVertex.size()-3);
                        geometry->index.push_back(geometry->tVertex.size()-2);
                        geometry->index.push_back(geometry->tVertex.size()-1);

                        normal = ((a-c).cross(d-c)).normalize();
                        geometry->tVertex.push_back(d);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(dUV);
                        geometry->tVertex.push_back(c);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(cUV);
                        geometry->tVertex.push_back(a);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aUV);    

                        geometry->index.push_back(geometry->tVertex.size()-3);
                        geometry->index.push_back(geometry->tVertex.size()-2);
                        geometry->index.push_back(geometry->tVertex.size()-1);                

                    }            
                }

                if (!openEnded) 
                {
                    this->segmentsH -=2;
                }

                for (uint32 i=0;i<geometry->tVertex.size();i++)
                {
                    geometry->tTexcoord[i].x = 1-geometry->tTexcoord[i].x;
                }

                Build();
            }

    };
};

 #endif /* CYLINDER_H */