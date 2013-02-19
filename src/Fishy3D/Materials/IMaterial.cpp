//============================================================================
// Name        : IMaterial.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IMaterial Interface
//============================================================================

#include "IMaterial.h"

namespace Fishy3D {    
    
   IMaterial::IMaterial()
    {
        // Values By Default
        isTransparent = false;
        isWireFrame = false;
        isCastingShadows = false;
        cullFace = CullFace::BackFace;
    }
    void IMaterial::SetOpacity(const float &opacity)
    {
        this->opacity = opacity;
		if (opacity<1.f) isTransparent = true;
		isTransparent = false;
        AddUniform(Uniform::Uniform("uOpacity",Uniform::DataType::Float,&this->opacity));
    }
    void IMaterial::SetCullFace(const unsigned &face)
    {
        this->cullFace = face;
    }
   IMaterial::~IMaterial() {}
    
    void IMaterial::SetTransparencyFlag(bool transparency)
    {
        isTransparent = transparency;
    }
    bool IMaterial::IsTransparent() const
    {
		return isTransparent;
    }
    unsigned IMaterial::GetCullFace() const
    {
        return cullFace;
    }
    const float &IMaterial::GetOpacity() const
    {
        return opacity;
    }
    
    void IMaterial::EnableCastingShadows()
    {
        isCastingShadows = true;
    }
    void IMaterial::DisableCastingShadows()
    {
        isCastingShadows = false;
    }
    bool IMaterial::IsCastingShadows()
    {
        return isCastingShadows;
    }
    // send uniforms
    void IMaterial::SetUniformValue(std::string Uniform, int value)
    {
        StringID ID(MakeStringID(Uniform));
        UserUniforms[ID].SetValue(&value,1);
    }
    void IMaterial::SetUniformValue(StringID UniformID, int value)
    {
        UserUniforms[UniformID].SetValue(&value,1);
    } 
    void IMaterial::SetUniformValue(std::string Uniform, float value)
    {
        StringID ID(MakeStringID(Uniform));
        UserUniforms[ID].SetValue(&value,1);
    }
    void IMaterial::SetUniformValue(StringID UniformID, float value)
    {
        UserUniforms[UniformID].SetValue(&value,1);
    } 
    void IMaterial::SetUniformValue(StringID UniformID, void* value, const unsigned &elementCount)
    {
        UserUniforms[UniformID].SetValue(value,elementCount);
    }
    void IMaterial::SetUniformValue(std::string Uniform, void* value, const unsigned &elementCount)
    {
        StringID ID(MakeStringID(Uniform));
        UserUniforms[ID].SetValue(value,elementCount);
    }
        
    void IMaterial::AddUniform(Uniform::Uniform Data)
    {
        // Global Uniforms
        if ((int)Data.Usage<Uniform::DataUsage::Other)
        {
            GlobalUniforms.push_back(Data);
        }
        // Game Object Uniforms
        else if ((int)Data.Usage>Uniform::DataUsage::Other)
        {
            ModelUniforms.push_back(Data);
        } else // User Specific
        {
            StringID ID(MakeStringID(Data.Name));
            UserUniforms[ID]=Data;
        }
    }
    void IMaterial::StartRenderWireFrame()
    {
        isWireFrame = true;
    }
    void IMaterial::StopRenderWireFrame()
    {
        isWireFrame = false;
    }
    bool IMaterial::IsWireFrame() const
    {
        return isWireFrame;
    }
    
    unsigned IMaterial::GetShader()
    {
        return shaderProgram;
    }

}
