//============================================================================
// Name        : IMaterial.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IMaterial Interface
//============================================================================

#include <Pyros3D/Materials/IMaterial.h>

namespace p3d {
    
    uint32 IMaterial::_InternalID = 0;
    
    IMaterial::IMaterial()
    {
        // Values By Default
        isTransparent = false;
        isWireFrame = false;
        isCastingShadows = false;
        cullFace = CullFace::BackFace;
        
        // Set Internal ID
        materialID = _InternalID;
        
        // Increase Internal ID
        _InternalID++;
    }
    void IMaterial::Destroy() {}
    void IMaterial::SetOpacity(const f32 &opacity)
    {
        this->opacity = opacity;
        if (opacity<1.f) isTransparent = true;
        else isTransparent = false;
        AddUniform(Uniform("uOpacity",DataType::Float,&this->opacity));
    }
    void IMaterial::SetCullFace(const uint32 &face)
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
    uint32 IMaterial::GetCullFace() const
    {
        return cullFace;
    }
    const f32 &IMaterial::GetOpacity() const
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
    void IMaterial::SetUniformValue(std::string Uniform, int32 value)
    {
        StringID ID(MakeStringID(Uniform));
        UserUniforms[ID].SetValue(&value,1);
    }
    void IMaterial::SetUniformValue(StringID UniformID, int32 value)
    {
        UserUniforms[UniformID].SetValue(&value,1);
    } 
    void IMaterial::SetUniformValue(std::string Uniform, f32 value)
    {
        StringID ID(MakeStringID(Uniform));
        UserUniforms[ID].SetValue(&value,1);
    }
    void IMaterial::SetUniformValue(StringID UniformID, f32 value)
    {
        UserUniforms[UniformID].SetValue(&value,1);
    } 
    void IMaterial::SetUniformValue(StringID UniformID, void* value, const uint32 &elementCount)
    {
        UserUniforms[UniformID].SetValue(value,elementCount);
    }
    void IMaterial::SetUniformValue(std::string Uniform, void* value, const uint32 &elementCount)
    {
        StringID ID(MakeStringID(Uniform));
        UserUniforms[ID].SetValue(value,elementCount);
    }
        
    void IMaterial::AddUniform(const Uniform &Data)
    {
        // Global Uniforms
        if ((int)Data.Usage<DataUsage::Other)
        {
            GlobalUniforms.push_back(Data);
        }
        // Game Object Uniforms
        else if ((int)Data.Usage>DataUsage::Other)
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
    
    const uint32 &IMaterial::GetShader() const
    {
        return shaderProgram;
    }

    uint32 IMaterial::GetInternalID()
    {
        return materialID;
    }
}
