//============================================================================
// Name        : Sphere
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sphere Geometry
//============================================================================

#ifndef SPHERE_H
#define SPHERE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

    class PYROS3D_API Sphere : public Primitive {

        public:

            Sphere(const f32 radius, const uint32 segmentsW, const uint32 segmentsH, bool smooth = false, bool HalfSphere = false, bool flip = false, bool TangentBitangent = false)
            {
                isFlipped = flip;
                isSmooth = smooth;
                calculateTangentBitangent = TangentBitangent;
                
                int i,j;
                f32 iHor=(f32)segmentsW;
                f32 iVer=(f32)segmentsH;

                std::vector <std::vector<Vec3> > aVtc;    

                for (j=0;j<(iVer+1);++j) {
                    f32 fRad1 = j / iVer;        
                    f32 fZ = (f32)(-radius * cos(fRad1 * PI));
                    f32 fRds = (f32)(radius * sin(fRad1 * PI));

                    std::vector<Vec3> aRow;
                    Vec3 oVtx;

                    for (i=0;i< iHor;++i) {            
                        f32 fRad2 = (2 * i / iHor);
                        f32 fX = (f32)(fRds * sin(fRad2 * PI));            
                        f32 fY = (f32)(fRds * cos(fRad2 * PI));
                        if (!(((j == 0 || j == (int)iVer) & i) > 0)) {
                            oVtx=Vec3(fY,fZ,fX);
                        }
                        aRow.push_back(oVtx);
                    }
                    aVtc.push_back(aRow);
                }

                int iVerNum = aVtc.size();

                for (j=0;j<iVerNum;++j) {
                    int iHorNum = aVtc[j].size();
                    if (j > 0) {
                        for (i=0;i<iHorNum;++i) {
                            bool bEnd = i == (iHorNum - 1);                
                            Vec3 aP1 = aVtc[j][bEnd?0:(i + 1)];
                            Vec3 aP2 = aVtc[j][(bEnd?(iHorNum-1):i)];
                            Vec3 aP3 = aVtc[j - 1][(bEnd?(iHorNum - 1):i)];
                            Vec3 aP4 = aVtc[j - 1][bEnd?0:(i + 1)];

                            f32 iVerNumf=(f32)iVerNum;f32 iHorNumf = (f32)iHorNum;                

                            f32 fJ0 = j/(iVerNumf-1.f);
                            f32 fJ1 = (j-1.f) / (iVerNumf-1.f);
                            f32 fI0 = (i+1.f) / iHorNumf;
                            f32 fI1 = i / iHorNumf;

                            Vec2 aP1uv = Vec2(fI0,fJ0);
                            Vec2 aP2uv = Vec2(fI1,fJ0);
                            Vec2 aP3uv = Vec2(fI1,fJ1);
                            Vec2 aP4uv = Vec2(fI0,fJ1);

                            if ((size_t)j < (aVtc.size() - 1)) {                    
                                Vec3 normal = ((aP1-aP2).cross(aP3-aP2)).normalize();     

                                geometry->tVertex.push_back(aP3); geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aP3uv);
                                geometry->tVertex.push_back(aP2); geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aP2uv);
                                geometry->tVertex.push_back(aP1); geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aP1uv);

                                geometry->index.push_back(geometry->tVertex.size()-3);
                                geometry->index.push_back(geometry->tVertex.size()-2);
                                geometry->index.push_back(geometry->tVertex.size()-1);

                            }
                            if (j > 1) {
                                Vec3 normal = ((aP1-aP3).cross(aP4-aP3)).normalize();  

                                geometry->tVertex.push_back(aP4); geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aP4uv);
                                geometry->tVertex.push_back(aP3); geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aP3uv);
                                geometry->tVertex.push_back(aP1); geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aP1uv);                        

                                geometry->index.push_back(geometry->tVertex.size()-3);
                                geometry->index.push_back(geometry->tVertex.size()-2);
                                geometry->index.push_back(geometry->tVertex.size()-1);

                            }
                        }
                    }
                }                

                for (size_t i = 0;i < geometry->tTexcoord.size(); i++) {
                    geometry->tTexcoord[i].x = 1-geometry->tTexcoord[i].x;
                    geometry->tTexcoord[i].y = 1-geometry->tTexcoord[i].y;
                }

                if (HalfSphere==true)
                {
                    geometry->index.resize(geometry->index.size()/2);
                    geometry->tVertex.resize(geometry->tVertex.size()/2);
                    geometry->tNormal.resize(geometry->tNormal.size()/2);
                    geometry->tTexcoord.resize(geometry->tTexcoord.size()/2);
                }

            // Build and Send Buffers
            Build();

			// Bounding Box
			minBounds = Vec3(-radius, -radius, -radius);
			maxBounds = Vec3(radius, radius, radius);

			// Bounding Sphere
			BoundingSphereCenter = Vec3(0, 0, 0);
			BoundingSphereRadius = radius;
        }

		virtual void CalculateBounding()
		{
			
		}

    };
};

 #endif /* SPHERE_H */