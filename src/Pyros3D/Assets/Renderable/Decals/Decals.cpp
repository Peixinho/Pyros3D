//============================================================================
// Name        : Decals.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Decals Geometry based on:
// https://github.com/spite/THREE.DecalGeometry
// http://blog.wolfire.com/2009/06/how-to-project-decals/
//============================================================================

#include <Pyros3D/Assets/Renderable/Decals/Decals.h>

namespace p3d {

	Decal::Decal(std::vector<DecalVertex> vertices)
	{
		isFlipped = false;
		isSmooth = true;
		calculateTangentBitangent = true;

		uint32 counter = 0;
		for (std::vector<DecalVertex>::iterator i = vertices.begin(); i != vertices.end(); i++)
		{
			geometry->tVertex.push_back((*i).vertex);   geometry->tNormal.push_back((*i).normal);   geometry->tTexcoord.push_back((*i).uv);
			geometry->index.push_back(counter++);
		}

		// Build and Send Buffers
		Build();
	}

	void DecalGeometry::clipFace(std::vector<DecalVertex> &inVertices, Vec3 plane)
	{
		size = .5f * fabs(dimensions.dotProduct(plane));

		std::vector<DecalVertex> outVertices;

		for (uint32 j = 0; j < inVertices.size(); j += 3) {

			uint32 total = 0;

			f32 d1 = inVertices[j + 0].vertex.dotProduct(plane) - size;
			f32 d2 = inVertices[j + 1].vertex.dotProduct(plane) - size;
			f32 d3 = inVertices[j + 2].vertex.dotProduct(plane) - size;

			bool v1Out = d1 > 0;
			bool v2Out = d2 > 0;
			bool v3Out = d3 > 0;

			total = (v1Out ? 1 : 0) + (v2Out ? 1 : 0) + (v3Out ? 1 : 0);

			switch (total) {
			case 0: {
				outVertices.push_back(inVertices[j + 0]);
				outVertices.push_back(inVertices[j + 1]);
				outVertices.push_back(inVertices[j + 2]);
				break;
			}
			case 1: {
				DecalVertex nV1, nV2, nV3, nV4;
				if (v1Out) {
					nV1 = inVertices[j + 1];
					nV2 = inVertices[j + 2];
					nV3 = clip(inVertices[j + 0], nV1, plane);
					nV4 = clip(inVertices[j + 0], nV2, plane);
				}
				if (v2Out) {
					nV1 = inVertices[j + 0];
					nV2 = inVertices[j + 2];
					nV3 = clip(inVertices[j + 1], nV1, plane);
					nV4 = clip(inVertices[j + 1], nV2, plane);

					outVertices.push_back(nV3);
					outVertices.push_back(nV2);
					outVertices.push_back(nV1);

					outVertices.push_back(nV2);
					outVertices.push_back(nV3);
					outVertices.push_back(nV4);
					break;
				}
				if (v3Out) {
					nV1 = inVertices[j + 0];
					nV2 = inVertices[j + 1];
					nV3 = clip(inVertices[j + 2], nV1, plane);
					nV4 = clip(inVertices[j + 2], nV2, plane);
				}

				outVertices.push_back(nV1);
				outVertices.push_back(nV2);
				outVertices.push_back(nV3);

				outVertices.push_back(nV4);
				outVertices.push_back(nV3);
				outVertices.push_back(nV2);

				break;
			}
			case 2: {
				DecalVertex nV1, nV2, nV3;
				if (!v1Out) {
					nV1 = inVertices[j + 0];
					nV2 = clip(nV1, inVertices[j + 1], plane);
					nV3 = clip(nV1, inVertices[j + 2], plane);
					outVertices.push_back(nV1);
					outVertices.push_back(nV2);
					outVertices.push_back(nV3);
				}
				if (!v2Out) {
					nV1 = inVertices[j + 1];
					nV2 = clip(nV1, inVertices[j + 2], plane);
					nV3 = clip(nV1, inVertices[j], plane);
					outVertices.push_back(nV1);
					outVertices.push_back(nV2);
					outVertices.push_back(nV3);
				}
				if (!v3Out) {
					nV1 = inVertices[j + 2];
					nV2 = clip(nV1, inVertices[j], plane);
					nV3 = clip(nV1, inVertices[j + 1], plane);
					outVertices.push_back(nV1);
					outVertices.push_back(nV2);
					outVertices.push_back(nV3);
				}

				break;
			}
			case 3:
				break;
			}

		}

		inVertices = outVertices;
	}

	DecalVertex DecalGeometry::clip(DecalVertex v0, DecalVertex v1, Vec3 p)
	{
		f32 d0 = v0.vertex.dotProduct(p) - size;
		f32	d1 = v1.vertex.dotProduct(p) - size;

		f32 s = d0 / (d0 - d1);
		DecalVertex v = DecalVertex(
			Vec3(
				v0.vertex.x + s * (v1.vertex.x - v0.vertex.x),
				v0.vertex.y + s * (v1.vertex.y - v0.vertex.y),
				v0.vertex.z + s * (v1.vertex.z - v0.vertex.z)
				),
			Vec3(
				v0.normal.x + s * (v1.normal.x - v0.normal.x),
				v0.normal.y + s * (v1.normal.y - v0.normal.y),
				v0.normal.z + s * (v1.normal.z - v0.normal.z)
				)
			);

		// need to clip more values (texture coordinates)? do it this way:
		//intersectpoint.value = a.value + s*(b.value-a.value);

		return v;
	}

	void DecalGeometry::ComputeDecal()
	{

		std::vector<DecalVertex> finalVertices;

		for (uint32 k = 0; k < rcomp->GetMeshes().size(); k++)
		{
			RenderingMesh* rc = (RenderingMesh*)rcomp->GetMeshes()[k];
			std::vector<DecalVertex> vertices;

			for (uint32 i = 0; i < rc->Geometry->GetIndexData().size(); i += 3)
			{
				Vec3 v = rc->renderingComponent->GetOwner()->GetLocalTransformation() * iCubeMatrix * rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i]];
				Vec3 n = rc->Geometry->GetNormalData()[rc->Geometry->GetIndexData()[i]];
				vertices.push_back(DecalVertex(v, n));

				v = rc->renderingComponent->GetOwner()->GetLocalTransformation() * iCubeMatrix * rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i + 1]];
				n = rc->Geometry->GetNormalData()[rc->Geometry->GetIndexData()[i + 1]];
				vertices.push_back(DecalVertex(v, n));

				v = rc->renderingComponent->GetOwner()->GetLocalTransformation() * iCubeMatrix * rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i + 2]];
				n = rc->Geometry->GetNormalData()[rc->Geometry->GetIndexData()[i + 2]];
				vertices.push_back(DecalVertex(v, n));

				if (check.x) {
					clipFace(vertices, Vec3(1, 0, 0));
					clipFace(vertices, Vec3(-1, 0, 0));
				}
				if (check.y) {
					clipFace(vertices, Vec3(0, 1, 0));
					clipFace(vertices, Vec3(0, -1, 0));
				}
				if (check.z) {
					clipFace(vertices, Vec3(0, 0, 1));
					clipFace(vertices, Vec3(0, 0, -1));
				}

				for (uint32 j = 0; j < vertices.size(); j++)
				{
					DecalVertex* v = &vertices[j];
					v->uv = Vec2(
						.5f + (v->vertex.x / dimensions.x),
						.5f + (v->vertex.y / dimensions.y)*-1.f
						);

					v->vertex = (CubeMatrix * v->vertex);

				}

				if (vertices.size() == 0) continue;

				for (uint32 j = 0; j < vertices.size(); j++)
				{
					finalVertices.push_back(vertices[j]);
				}

			}

		}

		decal = new Decal(finalVertices);

		/*
		this.vertices.push(
		finalVertices[k].vertex,
		finalVertices[k + 1].vertex,
		finalVertices[k + 2].vertex
		);

		var f = new THREE.Face3(
		k,
		k + 1,
		k + 2
		)
		f.vertexNormals.push(finalVertices[k + 0].normal);
		f.vertexNormals.push(finalVertices[k + 1].normal);
		f.vertexNormals.push(finalVertices[k + 2].normal);

		this.faces.push(f);

		this.faceVertexUvs[0].push([
		this.uvs[k],
		this.uvs[k + 1],
		this.uvs[k + 2]
		]);

		}*/

	}

	DecalGeometry::DecalGeometry(RenderingComponent* rcomp, Vec3 position, Vec3 rotation, Vec3 dimensions, Vec3 check)
	{
		this->rcomp = rcomp;
		this->position = position;
		this->rotation = rotation;
		this->dimensions = dimensions;
		this->check = check;

		CubeMatrix = Matrix();
		CubeMatrix.RotationX(rotation.x);
		CubeMatrix.RotationY(rotation.y);
		CubeMatrix.RotationZ(rotation.z);
		CubeMatrix.Translate(position);
		CubeMatrix.Scale(1.f, 1.f, 1.f);

		iCubeMatrix = CubeMatrix.Inverse();

		ComputeDecal();
	}

	DecalGeometry::~DecalGeometry()
	{
		delete decal;
	}
};