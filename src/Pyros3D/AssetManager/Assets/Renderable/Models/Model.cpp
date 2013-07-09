//============================================================================
// Name        : Model.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Geometry
//============================================================================

#include "Model.h"

namespace p3d {
  
    namespace Renderables {

        void ModelGeometry::CreateBuffers()
        {
             CalculateBounding();

             AttributeBuffer* Vertex  = new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Static);
             if (tVertex.size()>0) Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tVertex[0],tVertex.size());
             if (tNormal.size()>0) Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3,&tNormal[0],tNormal.size());
             if (tTexcoord.size()>0) Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2,&tTexcoord[0],tTexcoord.size());
             if (tTangent.size()>0) Vertex->AddAttribute("aTangent", Buffer::Attribute::Type::Vec3,&tTangent[0],tTangent.size());
             if (tBitangent.size()>0) Vertex->AddAttribute("aBitangent", Buffer::Attribute::Type::Vec3,&tBitangent[0],tBitangent.size());
             if (tBonesID.size()>0) Vertex->AddAttribute("aBonesID", Buffer::Attribute::Type::Vec4,&tBonesID[0],tBonesID.size());
             if (tBonesWeight.size()>0) Vertex->AddAttribute("aBonesWeight", Buffer::Attribute::Type::Vec4,&tBonesWeight[0],tBonesWeight.size());
             AttributesBuffer.push_back(Vertex);
        }

        void ModelGeometry::CalculateBounding()
        {
             // Bounding Box
             for (uint32 i=0;i<tVertex.size();i++)
             {
                 if (i==0) {
                     minBounds = tVertex[i];
                     maxBounds = tVertex[i];
                 } else {
                     if (tVertex[i].x<minBounds.x) minBounds.x = tVertex[i].x;
                     if (tVertex[i].y<minBounds.y) minBounds.y = tVertex[i].y;
                     if (tVertex[i].z<minBounds.z) minBounds.z = tVertex[i].z;
                     if (tVertex[i].x>maxBounds.x) maxBounds.x = tVertex[i].x;
                     if (tVertex[i].y>maxBounds.y) maxBounds.y = tVertex[i].y;
                     if (tVertex[i].z>maxBounds.z) maxBounds.z = tVertex[i].z;
                 }            
             }
             // Bounding Sphere
             BoundingSphereCenter = maxBounds-minBounds;
             f32 a = maxBounds.distance(BoundingSphereCenter);
             f32 b = minBounds.distance(BoundingSphereCenter);        
             BoundingSphereRadius = (a>b?a:b);
         }

        Model::Model(const std::string ModelPath)
        {
             mesh = new ModelLoader();
             mesh->Load(ModelPath);

             // List of Material Properties
             std::vector<MaterialProperties> materialProperties;
             // Build Materials
             for (uint32 i=0;i<mesh->materials.size();i++)
             {
                 materialProperties.push_back(mesh->materials[i]);
             }

             for (uint32 i=0;i<mesh->subMeshes.size();i++)
             {

                 ModelGeometry* c_submesh = new ModelGeometry();

                 // Set Material From ID
                 c_submesh->materialProperties = materialProperties[mesh->subMeshes[i].materialID];

                 c_submesh->index.resize(mesh->subMeshes[i].tIndex.size());
                 memcpy(&c_submesh->index[0],&mesh->subMeshes[i].tIndex[0],mesh->subMeshes[i].tIndex.size()*sizeof(uint32));

                 if (mesh->subMeshes[i].hasVertex==true)
                 {
                     c_submesh->tVertex.resize(mesh->subMeshes[i].tVertex.size());
                     memcpy(&c_submesh->tVertex[0],&mesh->subMeshes[i].tVertex[0],mesh->subMeshes[i].tVertex.size()*sizeof(Vec3));
                 }
                 if (mesh->subMeshes[i].hasNormal==true)
                 {
                     c_submesh->tNormal.resize(mesh->subMeshes[i].tNormal.size());
                     memcpy(&c_submesh->tNormal[0],&mesh->subMeshes[i].tNormal[0],mesh->subMeshes[i].tNormal.size()*sizeof(Vec3));
                 }
                 if (mesh->subMeshes[i].hasTexcoord==true)
                 {
                     c_submesh->tTexcoord.resize(mesh->subMeshes[i].tTexcoord.size());
                     memcpy(&c_submesh->tTexcoord[0],&mesh->subMeshes[i].tTexcoord[0],mesh->subMeshes[i].tTexcoord.size()*sizeof(Vec2));
                 }
                 if (mesh->subMeshes[i].hasTangentBitangent==true)
                 {
                     c_submesh->tTangent.resize(mesh->subMeshes[i].tTangent.size());
                     memcpy(&c_submesh->tTangent[0],&mesh->subMeshes[i].tTangent[0],mesh->subMeshes[i].tTangent.size()*sizeof(Vec3));

                     c_submesh->tBitangent.resize(mesh->subMeshes[i].tBitangent.size());
                     memcpy(&c_submesh->tBitangent[0],&mesh->subMeshes[i].tBitangent[0],mesh->subMeshes[i].tBitangent.size()*sizeof(Vec3));
                 }
                 if (mesh->subMeshes[i].hasBones==true)
                 {
                     c_submesh->tBonesID.resize(mesh->subMeshes[i].tVertex.size());
                     memcpy(&c_submesh->tBonesID[0],&mesh->subMeshes[i].tBonesID[0],mesh->subMeshes[i].tBonesID.size()*sizeof(Vec4));

                     c_submesh->tBonesWeight.resize(mesh->subMeshes[i].tVertex.size());
                     memcpy(&c_submesh->tBonesWeight[0],&mesh->subMeshes[i].tBonesWeight[0],mesh->subMeshes[i].tBonesWeight.size()*sizeof(Vec4));

                     // Save SubMesh Map
                     c_submesh->MapBoneIDs = mesh->subMeshes[i].MapBoneIDs;
                     // Save SubMesh Bones Offset Matrix
                     c_submesh->BoneOffsetMatrix = mesh->subMeshes[i].BoneOffsetMatrix;
                     // Set Skinning Flag on
                     c_submesh->materialProperties.haveBones = true;
                 }

                 Geometries.push_back(c_submesh);
             }

             Build();

             // Save Skeleton
             skeleton = mesh->skeleton;

             // Delete Model Loader
             delete mesh;
        }

        void Model::Build()
        {
            for (std::vector<IGeometry*>::iterator i=Geometries.begin();i!=Geometries.end();i++)
            {
                 ModelGeometry *geometry = (ModelGeometry*)(*i);
                 // Create Attributes Buffers
                 geometry->CreateBuffers();
                 // Send Index and Attributes Buffers
                 geometry->SendBuffers();
            }
            
            // Calculate Model's Bounding Box
            CalculateBounding();
        }
    };
   
};