//============================================================================
// Name        : Grid.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ( ͡° ͜ʖ ͡°)
// Description : Grid
//============================================================================

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

#ifndef RENDERINGGRID_H
#define	RENDERINGGRID_H

using namespace p3d;
   
class Grid : public Primitive {
        
    public:

        Grid(const f32 &width, const f32 &depth, const uint32 &unit)
        {
            float w2 = width * unit / 2.f;
            float d2 = depth * unit / 2.f;

            for (float k=-d2;k<=d2;k+=unit)
            {
                geometry->tVertex.push_back(Vec3(-w2,0,k));
                geometry->tVertex.push_back(Vec3(w2,0,k));
                geometry->tNormal.push_back(Vec3(0,0,0));
                geometry->tNormal.push_back(Vec3(0,0,0));
                geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tTexcoord.push_back(Vec2(0,0));
            }
            for (float k=-w2;k<=w2;k+=unit)
            {
                geometry->tVertex.push_back(Vec3(k,0,-d2));
                geometry->tVertex.push_back(Vec3(k,0,d2));
                geometry->tNormal.push_back(Vec3(0,0,0));
                geometry->tNormal.push_back(Vec3(0,0,0));
                geometry->tTexcoord.push_back(Vec2(0,0));
                geometry->tTexcoord.push_back(Vec2(0,0));
            }

            for (unsigned k = 0;k<geometry->tVertex.size(); k++)
                geometry->index.push_back(k);

			Vec3 min = geometry->tVertex[0];
			for (uint32 i = 0; i < geometry->tVertex.size(); i++)
			{
				geometry->index.push_back(i);
				geometry->tTexcoord[i].y = 1 - geometry->tTexcoord[i].y;

				if (geometry->tVertex[i].x < min.x) min.x = geometry->tVertex[i].x;
				if (geometry->tVertex[i].y < min.y) min.y = geometry->tVertex[i].y;
				if (geometry->tVertex[i].z < min.z) min.z = geometry->tVertex[i].z;

			}

            Build();

			// Bounding Box
			minBounds = min;
			maxBounds = min.negate();

			// Bounding Sphere
			BoundingSphereCenter = Vec3(0, 0, 0);
			BoundingSphereRadius = min.distance(Vec3::ZERO);

        }

};

#endif	/* RENDERINGGRID_H */