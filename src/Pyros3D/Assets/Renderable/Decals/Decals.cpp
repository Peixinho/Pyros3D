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

	Decal::Decal(std::vector<DecalVertex> vertices, bool haveBones)
	{

		ModelGeometry* geometry = new ModelGeometry();
		uint32 counter = 0;

		for (std::vector<DecalVertex>::iterator i = vertices.begin(); i != vertices.end(); i++)
		{
			geometry->tVertex.push_back((*i).vertex);   geometry->tNormal.push_back((*i).normal);   geometry->tTexcoord.push_back((*i).uv);
			geometry->index.push_back(counter++);

			if (haveBones)
			{
				geometry->tBonesID.push_back((*i).bonesID);
				geometry->tBonesWeight.push_back((*i).bonesWeight);
			}
		}

		Geometries.push_back(geometry);

		// Build and Send Buffers
		Build();

	}

	void DecalGeometry::clipFace(std::vector<DecalVertex> &inVertices, Vec3 plane)
	{
		size = .5f * fabs(dimensions.dotProduct(plane));

		std::vector<DecalVertex> outVertices;

		for (uint32 j = 0; j < inVertices.size(); j += 3) 
		{

			f32 d1 = inVertices[j + 0].vertex.dotProduct(plane) - size;
			f32 d2 = inVertices[j + 1].vertex.dotProduct(plane) - size;
			f32 d3 = inVertices[j + 2].vertex.dotProduct(plane) - size;

			bool v1Out = d1 > 0;
			bool v2Out = d2 > 0;
			bool v3Out = d3 > 0;

			uint32 total = (v1Out ? 1 : 0) + (v2Out ? 1 : 0) + (v3Out ? 1 : 0);

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

		return v;
	}

	void DecalGeometry::ComputeDecal()
	{

		std::vector<DecalVertex> finalVertices;

		RenderingMesh* rc = mesh;
		std::vector<DecalVertex> vertices;
			
		std::vector<Vec3> vertex, normal;
		std::vector<Vec4> bonesID, bonesWeight;

		for (uint32 l = 0; l < rc->Geometry->Attributes.size(); l++)
		for (std::vector<VertexAttribute*>::iterator i = rc->Geometry->Attributes[l]->Attributes.begin(); i != rc->Geometry->Attributes[l]->Attributes.end(); i++)
		{
			if ((*i)->Name.compare(std::string("aPosition")) == 0)
			{
				vertex.resize((*i)->DataLength);
				memcpy(&vertex[0], &(*i)->Data[0], (*i)->DataLength*sizeof(Vec3));
				std::cout << vertex.size() << std::endl;
			}
			else if ((*i)->Name.compare(std::string("aNormal")) == 0)
			{
				normal.resize((*i)->DataLength);
				memcpy(&normal[0], &(*i)->Data[0], (*i)->DataLength*sizeof(Vec3));
				std::cout << normal.size() << std::endl;
			}
			else if ((*i)->Name.compare(std::string("aBonesID")) == 0)
			{
				haveBones = true;
				bonesID.resize((*i)->DataLength);
				memcpy(&bonesID[0], &(*i)->Data[0], (*i)->DataLength*sizeof(Vec3));
			}
			else if ((*i)->Name.compare(std::string("aBonesWeight")) == 0)
			{
				haveBones = true;
				bonesWeight.resize((*i)->DataLength);
				memcpy(&bonesWeight[0], &(*i)->Data[0], (*i)->DataLength*sizeof(Vec3));
			}
		}
			
		for (uint32 i = 0; i < rc->Geometry->GetIndexData().size(); i += 3)
		{
			Vec3 v0 = iCubeMatrix * vertex[rc->Geometry->GetIndexData()[i]];
			Vec3 n0 = normal[rc->Geometry->GetIndexData()[i]];
			Vec2 uv0 = Vec2(0, 0);

			Vec3 v1 = iCubeMatrix * vertex[rc->Geometry->GetIndexData()[i + 1]];
			Vec3 n1 = normal[rc->Geometry->GetIndexData()[i + 1]];
			Vec2 uv1 = Vec2(0, 0);

			Vec3 v2 = iCubeMatrix * vertex[rc->Geometry->GetIndexData()[i + 2]];
			Vec3 n2 = normal[rc->Geometry->GetIndexData()[i + 2]];
			Vec2 uv2 = Vec2(0, 0);

			if (haveBones)
			{
				Vec4 boneID0 = bonesID[rc->Geometry->GetIndexData()[i]];
				Vec4 boneWeight0 = bonesWeight[rc->Geometry->GetIndexData()[i]];
				vertices.push_back(DecalVertex(v0, n0, uv0, boneID0, boneWeight0));

				Vec4 boneID1 = bonesID[rc->Geometry->GetIndexData()[i + 1]];
				Vec4 boneWeight1 = bonesWeight[rc->Geometry->GetIndexData()[i + 1]];
				vertices.push_back(DecalVertex(v1, n1, uv1, boneID1, boneWeight1));

				Vec4 boneID2 = bonesID[rc->Geometry->GetIndexData()[i + 2]];
				Vec4 boneWeight2 = bonesWeight[rc->Geometry->GetIndexData()[i + 2]];
				vertices.push_back(DecalVertex(v2, n2, uv2, boneID2, boneWeight2));
			}
			else {
				vertices.push_back(DecalVertex(v0, n0, uv0));
				vertices.push_back(DecalVertex(v1, n1, uv1));
				vertices.push_back(DecalVertex(v2, n2, uv2));
			}
				
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

		decal = new Decal(finalVertices, haveBones);

	}

	DecalGeometry::DecalGeometry(RenderingMesh* mesh, Matrix targetTransformation, Vec3 position, Vec3 rotation, Vec3 dimensions, Vec3 check)
	{
		this->mesh = mesh;
		this->position = position;
		this->rotation = rotation;
		this->dimensions = dimensions;
		this->check = check;
		this->haveBones = false;
		this->targetTransformation = targetTransformation;

		this->CubeMatrix = Matrix();
		this->CubeMatrix.RotationX(rotation.x);
		this->CubeMatrix.RotationY(rotation.y);
		this->CubeMatrix.RotationZ(rotation.z);
		this->CubeMatrix.Translate(position);
		this->CubeMatrix.Scale(1.f, 1.f, 1.f);

		this->iCubeMatrix = CubeMatrix.Inverse();

		ComputeDecal();
	}

	DecalGeometry::DecalGeometry(RenderingMesh* mesh, Matrix targetTransformation, Matrix transform, Vec3 dimensions, Vec3 check)
	{
		this->mesh = mesh;
		this->position = transform.GetTranslation();
		this->rotation = transform.GetEulerFromRotationMatrix();
		this->dimensions = dimensions;
		this->check = check;
		this->haveBones = false;
		this->targetTransformation = targetTransformation;

		this->CubeMatrix = transform;
		this->iCubeMatrix = CubeMatrix.Inverse();

		ComputeDecal();
	}

	DecalGeometry::~DecalGeometry()
	{
		delete decal;
	}
};