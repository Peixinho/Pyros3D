//============================================================================
// Name        : CustomShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//=================================================================================

#include "CustomShaderMaterial.h"

namespace Fishy3D
{
    
    std::map<unsigned, SuperSmartPointer<Shaders> > CustomShaderMaterial::ShadersList;    
    
    CustomShaderMaterial::CustomShaderMaterial(const std::string& vertexShaderFile, const std::string& fragmentShaderFile) : IMaterial()
    {
        StringID number = (MakeStringID(vertexShaderFile)) + (MakeStringID(fragmentShaderFile));
        
        if (ShadersList.find(number)==ShadersList.end())
        {
        
            // Not Found, Then Load Shader
            SuperSmartPointer<Shaders> shader = SuperSmartPointer<Shaders> (new Shaders());
        
            shader->vertexShader->loadShaderFile(vertexShaderFile.c_str());
            shader->fragmentShader->loadShaderFile(fragmentShaderFile.c_str());
        
            shader->vertexShader->compileShader(&shader->shaderProgram);
            shader->fragmentShader->compileShader(&shader->shaderProgram);
        
            ShadersList[number] = shader;
        }
        
        // Save Shader Location
        shaderID = number;
        
        // Add Counter
        ShadersList[number]->currentMaterials++;
        
        // Get Shader Program
        shaderProgram = ShadersList[number]->shaderProgram;
        
    }
    
    CustomShaderMaterial::~CustomShaderMaterial()
    {
        if (ShadersList.find(shaderID)!=ShadersList.end())
        {
            ShadersList[shaderID]->currentMaterials--;
            if (ShadersList[shaderID]->currentMaterials==0)
                ShadersList[shaderID].Dispose();
        }
    }
}