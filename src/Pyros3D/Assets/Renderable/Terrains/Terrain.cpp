//============================================================================
// Name        : Terrain
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Terrain
//============================================================================

#include <Pyros3D/Assets/Renderable/Terrains/Terrain.h>

namespace p3d {

	Terrain::Terrain(Texture* heightmap, const f32 dimensions, const uint32 segments, const f32 height, bool smooth, bool repeatUV)
	{
		std::vector<uchar> texels = heightmap->GetTextureData();
		imgWidth = heightmap->GetWidth();
		imgHeight = heightmap->GetHeight();
		unit = dimensions / segments;
		seg = segments;

		f32 u = 0, v = 0;
		uint32 vert = 0;
		f32 widthU = segments * unit;
		f32 depthU = segments * unit;

		for (f32 i = (widthU) / -2.f; i < (widthU) / 2.f; i += unit)
		{
			v = 0;
			for (f32 k = (depthU) / -2.f; k < (depthU) / 2.f; k += unit)
			{
				Vec3 a = Vec3(i, 0, k);
				Vec3 b = Vec3(i, 0, k + unit);
				Vec3 c = Vec3(i + unit, 0, k);
				Vec3 d = Vec3(i + unit, 0, k + unit);

				// set heights
				Vec2 at = Vec2(((a.x / (widthU)) + 0.5f) * imgWidth, ((a.z / (depthU)) + 0.5f) * (imgHeight - 1));
				int ai = (int)at.x * 4 + (int)at.y * imgWidth * 4;
				f32 ah = ((f32)texels[ai]) / 255.f;
				Vec2 bt = Vec2(((b.x / (widthU)) + 0.5f) * imgWidth, ((b.z / (depthU)) + 0.5f) * (imgHeight - 1));
				int bi = (int)bt.x * 4 + (int)bt.y * imgWidth * 4;
				f32 bh = ((f32)texels[bi]) / 255.f;
				Vec2 ct = Vec2(((c.x / (widthU)) + 0.5f) * imgWidth, ((c.z / (depthU)) + 0.5f) * (imgHeight - 1));
				int ci = (int)ct.x * 4 + (int)ct.y * imgWidth * 4;
				f32 ch = ((f32)texels[ci]) / 255.f;
				Vec2 dt = Vec2(((d.x / (widthU)) + 0.5f) * imgWidth, ((d.z / (depthU)) + 0.5f) * (imgHeight - 1));
				int di = (int)dt.x * 4 + (int)dt.y * imgWidth * 4;
				f32 dh = ((f32)texels[di]) / 255.f;
				a.y = ah * height; b.y = bh * height; c.y = ch * height; d.y = dh * height;

				Vec3 normal = ((b - a).cross(c - a)).normalize();

				geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);
				geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);
				geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);
				geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);

				if (repeatUV)
				{
					geometry->tTexcoord.push_back(Vec2(0, 0));
					geometry->tTexcoord.push_back(Vec2(1, 0));
					geometry->tTexcoord.push_back(Vec2(0, 1));
					geometry->tTexcoord.push_back(Vec2(1, 1));
				}
				else
				{
					geometry->tTexcoord.push_back(Vec2(u, v));
					geometry->tTexcoord.push_back(Vec2(u, v + (unit / depthU)));
					geometry->tTexcoord.push_back(Vec2(u + (unit / widthU), v));
					geometry->tTexcoord.push_back(Vec2(u + (unit / widthU), v + (unit / depthU)));
				}

				geometry->index.push_back(vert);
				geometry->index.push_back(vert + 1);
				geometry->index.push_back(vert + 2);
				geometry->index.push_back(vert + 2);
				geometry->index.push_back(vert + 1);
				geometry->index.push_back(vert + 3);

				vert += 4;
				v += (unit / depthU);
			}
			u += (unit / widthU);
		}

		// Build LODs
		uint32 divLOD = 2;
		uint32 segBefore = segments;
		uint32 vertexPerQuad = 6;

		/*while (segBefore % divLOD == 0 && segBefore)
		{
			uint32 row = vertexPerQuad * segBefore;
			std::vector<uint32> __index;
			uint32 counter = 0;

			for (uint32 i = 0; i < segBefore * row; i += vertexPerQuad * 2)
			{
				__index.push_back(geometry->index[i]);
				__index.push_back(geometry->index[i + vertexPerQuad + 1]);
				__index.push_back(geometry->index[i + row + 2]);

				__index.push_back(geometry->index[i + row + 2]);
				__index.push_back(geometry->index[i + vertexPerQuad + 1]);
				__index.push_back(geometry->index[i + row + vertexPerQuad + 5]);

				counter += 2;
				if (counter == segBefore)
				{
					i += row;
					counter = 0;
				}
			}
			geometry->index = __index;

			segBefore /= divLOD;
		}*/

		Vec3 min = geometry->tVertex[0];
		for (uint32 i = 1; i < geometry->tVertex.size(); i++)
		{
			if (geometry->tVertex[i].x < min.x) min.x = geometry->tVertex[i].x;
			if (geometry->tVertex[i].y < min.y) min.y = geometry->tVertex[i].y;
			if (geometry->tVertex[i].z < min.z) min.z = geometry->tVertex[i].z;
		}

		isSmooth = smooth;

		Build();

		// Bounding Box
		minBounds = min;
		maxBounds = min.negate();

		// Bounding Sphere
		BoundingSphereCenter = Vec3(0, 0, 0);
		BoundingSphereRadius = min.distance(Vec3::ZERO);

	}

	TerrainRenderingComponent::TerrainRenderingComponent(Terrain* renderable, IMaterial* Material) : RenderingComponent(renderable, Material)
	{
		segunits = renderable->seg * renderable->unit;
	}

	bool TerrainRenderingComponent::GetHeightFromLocalCoords(Vec3 &coords)
	{
		if ((coords.x >= 0 && coords.x <= segunits) && (coords.z >= 0 && coords.z <= segunits))
		{
			Terrain* t = (Terrain*)renderable;

			f32 indexX = (int)(coords.x / t->unit) * t->seg * 6;
			f32 indexZ = (int)(coords.z / t->unit) * 6;

			f32 x = fmodf(coords.x, t->unit) / t->unit;
			f32 z = fmodf(coords.z, t->unit) / t->unit;

			if (x <= (1 - z)) {
				coords.y = p3d::Math::barryCentric(
					Vec3(0, t->Geometries[0]->GetVertexData()[t->geometry->index[(indexX + indexZ)]].y, 0),
					Vec3(0, t->Geometries[0]->GetVertexData()[t->geometry->index[(indexX + indexZ) + 1]].y, 1),
					Vec3(1, t->Geometries[0]->GetVertexData()[t->geometry->index[(indexX + indexZ) + 2]].y, 0),
					Vec2(x, z));
			}
			else {
				coords.y = p3d::Math::barryCentric(
					Vec3(1, t->Geometries[0]->GetVertexData()[t->geometry->index[(indexX + indexZ) + 3]].y, 0),
					Vec3(0, t->Geometries[0]->GetVertexData()[t->geometry->index[(indexX + indexZ) + 4]].y, 1),
					Vec3(1, t->Geometries[0]->GetVertexData()[t->geometry->index[(indexX + indexZ) + 5]].y, 1),
					Vec2(x, z));
			}
			return true;
		}

		coords.y = 0;
		return false;
	}

	bool TerrainRenderingComponent::GetHeightFromGlobalCoords(Vec3 &coords)
	{
		if (GetOwner() != NULL)
		{
			Vec3 pos = GetOwner()->GetWorldPosition();
			Vec3 originPos;
			originPos.x = pos.x - segunits *.5f;
			originPos.z = pos.z - segunits *.5f;

			Vec3 localPos;
			localPos.x = coords.x - originPos.x;
			localPos.z = coords.z - originPos.z;

			if (GetHeightFromLocalCoords(localPos))
			{
				coords.y = localPos.y;
				return true;
			}
		}
		coords.y = 0;
		return false;
	}

};