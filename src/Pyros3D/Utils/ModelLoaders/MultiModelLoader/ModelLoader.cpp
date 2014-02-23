//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads Pyros3D Own Model Format
//============================================================================

#include "ModelLoader.h"

namespace p3d {

    ModelLoader::ModelLoader() {}

    ModelLoader::~ModelLoader() {}

#ifndef ANDROID

    bool ModelLoader::Load(const std::string& Filename)
    {
		BinaryFile* bin = new BinaryFile();
		bin->Open(Filename.c_str(),'r');

		int materialsSize;
		bin->Read(&materialsSize, sizeof(int));

        // Materials
        for (int i=0;i<materialsSize;i++)
        {
        	MaterialProperties mat;

        	// Material ID
        	bin->Read(&mat.id, sizeof(int));

        	// Name
        	int nameSize;
        	bin->Read(&nameSize, sizeof(int));
        	if (nameSize>0)
        	{
        		mat.Name.resize(nameSize);
	        	bin->Read(&mat.Name[0], sizeof(char) * nameSize);
			}
        	// Color
        	uchar In;
        	bin->Read(&In, sizeof(uchar));
        	mat.haveColor = (bool)In;
        	bin->Read(&mat.Color, sizeof(Vec4));

        	// Specular
        	bin->Read(&In, sizeof(uchar));
        	mat.haveSpecular = (bool)In;
        	bin->Read(&mat.Specular, sizeof(Vec4));

        	// Ambient
        	bin->Read(&In, sizeof(uchar));
        	mat.haveAmbient = (bool)In;
        	bin->Read(&mat.Ambient, sizeof(Vec4));

        	// Emissive
        	bin->Read(&In, sizeof(uchar));
        	mat.haveEmissive = (bool)In;
        	bin->Read(&mat.Emissive, sizeof(Vec4));

        	// WireFrame
        	bin->Read(&In, sizeof(uchar));
        	mat.WireFrame = (bool)In;

        	// Twosided
        	bin->Read(&In, sizeof(uchar));
        	mat.Twosided = (bool)In;

        	// Opacity
        	bin->Read(&mat.Opacity,sizeof(f32));

        	// Shininess
			bin->Read(&mat.Shininess,sizeof(f32));

			// Shininess
			bin->Read(&mat.ShininessStrength,sizeof(f32));

			// Color Map
			bin->Read(&In, sizeof(uchar));
        	mat.haveColorMap = (bool)In;
        	bin->Read(&nameSize, sizeof(int));
        	if (nameSize>0)
        	{
        		mat.colorMap.resize(nameSize);
	        	bin->Read(&mat.colorMap[0], sizeof(char) * nameSize);
			}

			// Specular Map
			bin->Read(&In, sizeof(uchar));
        	mat.haveSpecularMap = (bool)In;
        	bin->Read(&nameSize, sizeof(int));
        	if (nameSize>0)
        	{
        		mat.specularMap.resize(nameSize);
	        	bin->Read(&mat.specularMap[0], sizeof(char) * nameSize);	        	
        	}
			// Normal Map
			bin->Read(&In, sizeof(uchar));
        	mat.haveNormalMap = (bool)In;
        	bin->Read(&nameSize, sizeof(int));
        	if (nameSize>0)
        	{
        		mat.normalMap.resize(nameSize);
	        	bin->Read(&mat.normalMap[0], sizeof(char) * nameSize);
	        }

    		bin->Read(&In, sizeof(uchar));
            mat.haveBones = (bool)In;

        	materials.push_back(mat);

        }

        // Skeleton
        int skeletonSize;
        bin->Read(&skeletonSize, sizeof(int));
        for (uint32 i=0;i<skeletonSize;i++)
        {
        	int boneID;
        	bin->Read(&boneID, sizeof(int));
        	Bone b;

        	// Name
            int nameSize;
            bin->Read(&nameSize,sizeof(int));
            if (nameSize>0)
            {
            	b.name.resize(nameSize);
                bin->Read(&b.name[0],sizeof(char)*nameSize);
            }


            // Self
            bin->Read(&b.self, sizeof(int32));

            // Parent
            bin->Read(&b.parent, sizeof(int32));

            // Pos
            bin->Read(&b.pos, sizeof(Vec3));

            // Rot
            bin->Read(&b.rot, sizeof(Quaternion));

            // Scale
            bin->Read(&b.scale, sizeof(Vec3));

            // BindPose
            bin->Read(&b.bindPoseMat.m[0], sizeof(Matrix));

            // Skinned
            uchar In;
    		bin->Read(&In, sizeof(uchar));
            b.skinned = (bool)In;

            skeleton[boneID] = b;
        }

        // SubMeshes
        int subMeshesSize;
        bin->Read(&subMeshesSize, sizeof(int));

        for (int32 i=0;i<subMeshesSize;i++)
        {
        	SubMesh c_submesh;

        	// Name
            int nameSize;
            bin->Read(&nameSize,sizeof(int));
            if (nameSize>0)
            {
            	c_submesh.Name.resize(nameSize);
                bin->Read(&c_submesh.Name[0],sizeof(char)*nameSize);
            }

            // Index
        	int indexSize;
        	bin->Read(&indexSize, sizeof(int));
        	if (indexSize>0)
        	{
        		c_submesh.tIndex.resize(indexSize);
        		bin->Read(&c_submesh.tIndex[0],sizeof(int)*indexSize);
			}
        	// Vertex
        	int vertexSize;
        	bin->Read(&vertexSize, sizeof(int));
        	if (vertexSize>0)
        	{
        		c_submesh.hasVertex = true;
        		c_submesh.tVertex.resize(vertexSize);
        		bin->Read(&c_submesh.tVertex[0],sizeof(Vec3)*vertexSize);
        	} else c_submesh.hasVertex = false;

        	// Normal
        	int normalSize;
        	bin->Read(&normalSize, sizeof(int));
        	if (normalSize>0)
        	{
        		c_submesh.hasNormal = true;
        		c_submesh.tNormal.resize(normalSize);
        		bin->Read(&c_submesh.tNormal[0],sizeof(Vec3)*normalSize);
        	} else c_submesh.hasNormal = false;

        	// Tangent
        	int tangentSize;
        	bin->Read(&tangentSize, sizeof(int));
        	if (tangentSize>0)
        	{
        		c_submesh.hasTangentBitangent = true;
        		c_submesh.tTangent.resize(tangentSize);
        		bin->Read(&c_submesh.tTangent[0],sizeof(Vec3)*tangentSize);
        		c_submesh.tBitangent.resize(tangentSize);
        		bin->Read(&c_submesh.tBitangent[0],sizeof(Vec3)*tangentSize);
        	} else c_submesh.hasTangentBitangent = false;

        	// Texcoord
        	int texcoordSize;
        	bin->Read(&texcoordSize, sizeof(int));
        	if (texcoordSize>0)
        	{
        		c_submesh.hasTexcoord = true;
        		c_submesh.tTexcoord.resize(texcoordSize);
        		bin->Read(&c_submesh.tTexcoord[0],sizeof(Vec2)*texcoordSize);
        	} else c_submesh.hasTexcoord = false;

        	// Vertex Color
        	int vertexColorSize;
        	bin->Read(&vertexColorSize, sizeof(int));
        	if (vertexColorSize>0)
        	{
        		c_submesh.hasVertexColor = true;
        		c_submesh.tVertexColor.resize(vertexColorSize);
        		bin->Read(&c_submesh.tVertexColor[0],sizeof(Vec4)*vertexColorSize);
        	} else c_submesh.hasVertexColor = false;

        	// Bones
        	int BonesSize;
        	bin->Read(&BonesSize, sizeof(int));
        	if (BonesSize>0)
        	{
        		c_submesh.hasBones = true;
        		c_submesh.tBonesID.resize(BonesSize);
        		bin->Read(&c_submesh.tBonesID[0], sizeof(Vec4)*BonesSize);
        		c_submesh.tBonesWeight.resize(BonesSize);
        		bin->Read(&c_submesh.tBonesWeight[0], sizeof(Vec4)*BonesSize);
        	} else c_submesh.hasBones = false;

        	// Map Bones IDs
        	int MapBoneIDsSize;
        	bin->Read(&MapBoneIDsSize, sizeof(int));
        	if (MapBoneIDsSize>0)
        	{
        		for (int32 k=0;k<MapBoneIDsSize;k++)
        		{
	        		int boneID;
	        		bin->Read(&boneID, sizeof(int));
	        		bin->Read(&c_submesh.MapBoneIDs[boneID], sizeof(int));
	        	}
	        }

        	// Offset Matrix
        	int BoneOffsetMatrixSize;
        	bin->Read(&BoneOffsetMatrixSize, sizeof(int));
        	if (BoneOffsetMatrixSize>0)
            {
	        	for (int32 k=0;k<BoneOffsetMatrixSize;k++)
	        	{
	        		int boneID;
	        		bin->Read(&boneID, sizeof(int));
	        		bin->Read(&c_submesh.BoneOffsetMatrix[boneID].m[0], sizeof(Matrix));
	        	}
        	}

        	bin->Read(&c_submesh.materialID, sizeof(int));

        	subMeshes.push_back(c_submesh);

        }
        
        bin->Close();

        delete bin;

        return true;
    }

#endif

}