//============================================================================
// Name        : Cone
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Cone Geometry
//============================================================================

#ifndef CONE_H
#define CONE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

    class PYROS3D_API Cone : public Primitive {

        public:

            f32 segmentsW,segmentsH;

            Cone(const f32 radius, const f32 height, const uint32 segmentsW, const uint32 segmentsH, const bool openEnded, bool smooth = false, bool flip = false, bool TangentBitangent = false)
            {
                isFlipped = flip;
                isSmooth = smooth;
                calculateTangentBitangent = TangentBitangent;
                this->segmentsW = (f32)segmentsW;
                this->segmentsH = (f32)segmentsH;

                Vec3 normal;

				size_t i, j;
                int jMin;

                f32 _height = height / 2;

                std::vector <std::vector <Vec3> >aVtc;

                if (!openEnded) {
                    jMin = 1;
                    this->segmentsH += 1;
                    Vec3 bottom = Vec3(0,-_height*2, 0);
                    std::vector <Vec3> aRowB;
                    for (i=0;i<segmentsW;++i) {
                        aRowB.push_back(bottom);
                    }
                    aVtc.push_back(aRowB);
                } else {
                    jMin = 0;
                }

                for (j=jMin;j<this->segmentsH;++j) {
                    f32 z = -height + 2 * height * (f32)(j-jMin) / (f32)(this->segmentsH-jMin);

                    std::vector <Vec3> aRow;
                    for (i=0;i<segmentsW;++i) {
                        f32 verangle = (f32)(2 * (f32)i / (f32)segmentsW*PI);
                        f32 ringradius = radius * (f32)(this->segmentsH-j)/(f32)(this->segmentsH-jMin);
                        f32 x = ringradius * sin(verangle);
                        f32 y = ringradius * cos(verangle);

                        Vec3 oVtx = Vec3(y,z,x);

                        aRow.push_back(oVtx);
                    }
                    aVtc.push_back(aRow);
                }

                Vec3 top = Vec3(0,height,0);
                std::vector <Vec3> aRowT;

                for (i=0;i<this->segmentsW;++i)
                        aRowT.push_back(top);

                aVtc.push_back(aRowT);

                for (j=1;j<=this->segmentsH;++j) {
                    for (i=0;i<segmentsW;++i) {
                        Vec3 a = aVtc[j][i];
                        Vec3 b = aVtc[j][(i-1+segmentsW)%segmentsW];
                        Vec3 c = aVtc[j-1][(i-1+segmentsW)%segmentsW];
                        Vec3 d = aVtc[j-1][i];

                        int i2 = i;
                        if(i==0) i2 = segmentsW;

                        f32 vab = (f32)j/(f32)segmentsH;
                        f32 vcd = (f32)(j-1)/(f32)segmentsH;
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

                        normal = ((c-d).cross(a-d)).normalize();
                        geometry->tVertex.push_back(a);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aUV);
                        geometry->tVertex.push_back(d);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(dUV);
                        geometry->tVertex.push_back(c);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(cUV);    

                        geometry->index.push_back(geometry->tVertex.size()-3);
                        geometry->index.push_back(geometry->tVertex.size()-2);
                        geometry->index.push_back(geometry->tVertex.size()-1);

                    }
                }

                if (!openEnded) this->segmentsH-=1;

				Vec3 min = geometry->tVertex[0];
				for (uint32 i = 0; i<geometry->tVertex.size(); i++)
				{
					if (geometry->tVertex[i].x < min.x) min.x = geometry->tVertex[i].x;
					if (geometry->tVertex[i].y < min.y) min.y = geometry->tVertex[i].y;
					if (geometry->tVertex[i].z < min.z) min.z = geometry->tVertex[i].z;

					geometry->tTexcoord[i].x = 1 - geometry->tTexcoord[i].x;
				}

                Build();

				// Bounding Box
				minBounds = min;
				maxBounds = min.negate();

				// Bounding Sphere
				BoundingSphereCenter = Vec3(0, 0, 0);
				BoundingSphereRadius = min.distance(Vec3::ZERO);
			}

			virtual void CalculateBounding()
			{

			}

	};
};

 #endif /* CONE_H */