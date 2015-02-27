//============================================================================
// Name        : CustomShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//=================================================================================

#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

namespace p3d
{
    
    std::map<uint32, Shader*> CustomShaderMaterial::ShadersList;
    
    CustomShaderMaterial::CustomShaderMaterial(const std::string& vertexShaderFile, const std::string& fragmentShaderFile) : IMaterial()
    {
        StringID number = (MakeStringID(vertexShaderFile)) + (MakeStringID(fragmentShaderFile));
        
        if (ShadersList.find(number)==ShadersList.end())
        {
        
            // Not Found, Then Load Shader
            Shader* shader = new Shader();
        
            shader->LoadShaderFile(vertexShaderFile.c_str());
            shader->CompileShader(ShaderType::VertexShader);

            shader->LoadShaderFile(fragmentShaderFile.c_str());
            shader->CompileShader(ShaderType::FragmentShader);
        
            shader->LinkProgram();

            ShadersList[number] = shader;
        }
        
        // Save Shader Location
		shaderID = number;
        
        // Add Counter
        ShadersList[number]->currentMaterials++;
        
        // Get Shader Program
        shaderProgram = ShadersList[number]->ShaderProgram();

        f32 opacity = 1.0;
        AddUniform(Uniform("uOpacity",DataType::Float,&opacity));
        SetOpacity(opacity);
    }
    
    CustomShaderMaterial::~CustomShaderMaterial()
    {
        if (ShadersList.find(shaderID)!=ShadersList.end())
        {
            ShadersList[shaderID]->currentMaterials--;
            if (ShadersList[shaderID]->currentMaterials==0)
                delete ShadersList[shaderID];
        }
    }
}
