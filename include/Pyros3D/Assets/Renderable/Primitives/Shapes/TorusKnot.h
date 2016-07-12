//============================================================================
// Name        : Torus Knot
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Torus Knot Geometry
//============================================================================

#ifndef TORUSKNOT_H
#define TORUSKNOT_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

    class PYROS3D_API TorusKnot : public Primitive {

        public:

        f32 p, q, radius, heightScale;

        TorusKnot(const f32 radius, const f32 tube, const uint32 segmentsW = 60, const uint32 segmentsH = 6, const f32 p = 2, const f32 q = 3, const uint32 heightscale = 1, bool smooth = false, bool flip = false, bool TangentBitangent = false)
        {
            isFlipped = flip;
            isSmooth = smooth;
            calculateTangentBitangent = TangentBitangent;
            this->radius=radius;
            this->p=p;
            this->q=q;
            this->heightScale = heightscale;

            Vec3 normal;    

            // counters
            int i,j;      

            // vars aux
            Vec3 tang,n,bitan;

            std::vector< std::vector <Vec3> > aVtc;

            for (i=0;i<segmentsW;++i) {

                std::vector<Vec3> aRow;

                for (j=0;j<segmentsH;++j) {
                    f32 u = (f32)((f32)i/segmentsW*2*p*PI);
                    f32 v  = (f32)((f32)j/segmentsH*2*PI);
                    Vec3 p = GetPos(u,v);
                    Vec3 p2 = GetPos((f32)(u+.01),(f32)(v));
                    f32 cx, cy;
                    tang.x = p2.x - p.x; tang.y = p2.y-p.y; tang.z = p2.z-p.z;
                    n.x = p2.x + p.x; n.y = p2.y + p.y; n.z = p2.z + p.z;
                    bitan = tang.cross(n);
                    n = bitan.cross(tang);
                    bitan.normalizeSelf();
                    n.normalizeSelf();

                    cx = tube*cos(v); cy = tube*sin(v);
                    p.x += cx * n.x + cy * bitan.x;
                    p.y += cx * n.y + cy * bitan.y;
                    p.z += cx * n.z + cy * bitan.z;

                    Vec3 oVtx = Vec3(p.x, p.z, p.y);
                    aRow.push_back(oVtx);                                        
                }
                aVtc.push_back(aRow);
            }

            for (i=0;i<segmentsW;++i) {
                for(j=0;j<segmentsH;++j) {
                    int ip = (i+1)%segmentsW;
                    int jp = (j+1)%segmentsH;
                    Vec3 a = aVtc[i ][j];
                    Vec3 b = aVtc[ip][j];
                    Vec3 c = aVtc[i ][jp];
                    Vec3 d = aVtc[ip][jp];

                    Vec2 aUV = Vec2((f32)i      /segmentsW, (f32)j      /segmentsH);
                    Vec2 bUV = Vec2((f32)(i+1)  /segmentsW, (f32)j      /segmentsH);
                    Vec2 cUV = Vec2((f32)i      /segmentsW, (f32)(j+1)    /segmentsH);
                    Vec2 dUV = Vec2((f32)(i+1)  /segmentsW, (f32)(j+1)    /segmentsH);                    

                    normal = ((c-b).cross(a-b)).normalize();
                    geometry->tVertex.push_back(a);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(aUV);
                    geometry->tVertex.push_back(b);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(bUV);
                    geometry->tVertex.push_back(c);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(cUV);

                    geometry->index.push_back(geometry->tVertex.size()-3);
                    geometry->index.push_back(geometry->tVertex.size()-2);
                    geometry->index.push_back(geometry->tVertex.size()-1);

                    normal = ((b-c).cross(d-c)).normalize();
                    geometry->tVertex.push_back(d);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(dUV);
                    geometry->tVertex.push_back(c);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(cUV);
                    geometry->tVertex.push_back(b);       geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(bUV);                        

                    geometry->index.push_back(geometry->tVertex.size()-3);
                    geometry->index.push_back(geometry->tVertex.size()-2);
                    geometry->index.push_back(geometry->tVertex.size()-1);
                }
            }                     

			Vec3 min = geometry->tVertex[0];
            for (uint32 i=0;i<geometry->tVertex.size();i++)
            {
				if (geometry->tVertex[i].x < min.x) min.x = geometry->tVertex[i].x;
				if (geometry->tVertex[i].y < min.y) min.y = geometry->tVertex[i].y;
				if (geometry->tVertex[i].z < min.z) min.z = geometry->tVertex[i].z;

                geometry->tTexcoord[i].x = 1-geometry->tTexcoord[i].x;
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

        Vec3 GetPos(const f32 u, const f32 v) const
        {
            f32 cu = cos(u);
            f32 su = sin(u);
            f32 quOverP = this->q/this->p*u;
            f32 cs = cos(quOverP);
            Vec3 pos;
            pos.x = (f32)(this->radius*(2+cs)*.5*cu);
            pos.y = (f32)(this->radius*(2+cs)*su*.5);
            pos.z = (f32)(heightScale*radius*sin(quOverP)*.5);

            return pos;
        }
    };
};

 #endif /* TORUSKNOT_H */