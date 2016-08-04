//============================================================================
// Name        : ModelLoader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Loader Interface
//============================================================================

#include <Pyros3D/Utils/ModelLoaders/IModelLoader.h>

namespace p3d {

    IModelLoader::IModelLoader() {}

    IModelLoader::~IModelLoader() {}
    
    std::string IModelLoader::LoadFile(const std::string& Filename)
    {
        std::ifstream t(Filename.c_str());
        std::string str;

        t.seekg(0, std::ios::end);   
        str.reserve((size_t)t.tellg());
        t.seekg(0, std::ios::beg);

        // copy file to string
        str.assign((std::istreambuf_iterator<char>(t)),std::istreambuf_iterator<char>());
                    
        return str;
    }
    
    uint32 IModelLoader::GetBoneID(const std::string& BoneName)
    {
        StringID ID (MakeStringID(BoneName));
        return skeleton[ID].self;
    }
    uint32 IModelLoader::GetBoneID(const StringID& BoneID)
    {        
        return skeleton[BoneID].self;
    }
    
    void IModelLoader::DebugSkeleton()
    {
        // show skeleton
        for (std::map<StringID, Bone>::iterator i=skeleton.begin();i!=skeleton.end();i++)
        {
            if ((*i).second.self==0) 
            {
                std::cout << "ID: "<< (*i).second.self << " Name: " << (*i).second.name << std::endl;
                GetBoneChilds(skeleton,0,0);
            }
        }
    }
    void IModelLoader::GetBoneChilds(std::map<StringID,Bone> Skeleton, const int32 id, const uint32 iterations)
    {
        for (std::map<StringID, Bone>::iterator i=Skeleton.begin();i!=Skeleton.end();i++)
        {            
            if ((*i).second.parent==id) 
            {
                for (uint32 j=0;j<iterations+1;j++) if (j==iterations) std::cout << " |_"; else std::cout << "   ";
                std::cout << "___" << "ID: "<< (*i).second.self << " Name: " << (*i).second.name << std::endl;              
                GetBoneChilds(Skeleton,(*i).second.self,iterations+1);
            }
        }
    }
}
