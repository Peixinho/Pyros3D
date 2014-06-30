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
    
    std::map<uint32, Shaders*> CustomShaderMaterial::ShadersList;
    
    CustomShaderMaterial::CustomShaderMaterial(const std::string& vertexShaderFile, const std::string& fragmentShaderFile) : IMaterial()
    {
        StringID number = (MakeStringID(vertexShaderFile)) + (MakeStringID(fragmentShaderFile));
        
        if (ShadersList.find(number)==ShadersList.end())
        {
        
            // Not Found, Then Load Shader
            Shaders* shader = new Shaders();
        
            shader->vertexShader->loadShaderFile(vertexShaderFile.c_str());
            shader->fragmentShader->loadShaderFile(fragmentShaderFile.c_str());
			
            shader->vertexShader->compileShader(&shader->shaderProgram);
            shader->fragmentShader->compileShader(&shader->shaderProgram);
        
            shader->Link();

            ShadersList[number] = shader;
        }
        
        // Save Shader Location
		shaderID = number;
        
        // Add Counter
        ShadersList[number]->currentMaterials++;
        
        // Get Shader Program
        shaderProgram = ShadersList[number]->shaderProgram;

	f32 opacity = 1.0;
        AddUniform(Uniform::Uniform("uOpacity",Uniform::DataType::Float,&opacity));
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
