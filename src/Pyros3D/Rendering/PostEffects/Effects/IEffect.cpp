//============================================================================
// Name        : IEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Effect Interface
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

namespace p3d {

    IEffect::IEffect() 
    {
        
        // Custom Dimensions
        customDimensions = false;
        
        // Reset Handles
        positionHandle = texcoordHandle = -2;
        
        // Initialize Shaders
        shader = new Shader();
        
        // Set Vertex Shader
        // Because its always the same
        VertexShaderString =  
                                "attribute vec3 aPosition;\n"
                                "attribute vec2 aTexcoord;\n"
                                "varying vec2 vTexcoord;\n"
                                "void main() {\n"
                                    "gl_Position = vec4(aPosition,1.0);\n"
                                    "vTexcoord = aTexcoord;\n"
                                "}\n";
        
        // Reset
        TextureUnits = 0;
        
    }

    void IEffect::UseRTT(const uint32 RTT)
    {
        // Set Textures
        if (RTT & RTT::Color) UseColor();
        if (RTT & RTT::LastRTT) UseLastRTT();
        if (RTT & RTT::Depth) UseDepth(); 
    }
    
    void IEffect::CompileShaders()
    {
        shader->LoadShaderText(VertexShaderString);
        shader->CompileShader(ShaderType::VertexShader);

        shader->LoadShaderText(FragmentShaderString);
        shader->CompileShader(ShaderType::FragmentShader);

		shader->LinkProgram();
    }
    
    void IEffect::Destroy()
    {
        shader->DeleteShader();
    }
    
    IEffect::~IEffect() {
    }

    void IEffect::AddUniform(const Uniform &Data)
    {
        StringID ID(MakeStringID(Data.Name));
        Uniforms[(uint32)ID].uniform = Data;
        Uniforms[(uint32)ID].handle = -2;
    }

    // send uniforms
    void IEffect::SetUniformValue(std::string Uniform, int32 value)
    {
        StringID ID(MakeStringID(Uniform));
        Uniforms[ID].uniform.SetValue(&value,1);
    }
    void IEffect::SetUniformValue(StringID UniformID, int32 value)
    {
       Uniforms[UniformID].uniform.SetValue(&value,1);
    }
    void IEffect::SetUniformValue(std::string Uniform, f32 value)
    {
        StringID ID(MakeStringID(Uniform));
        Uniforms[ID].uniform.SetValue(&value,1);
    }
    void IEffect::SetUniformValue(StringID UniformID, f32 value)
    {
        Uniforms[UniformID].uniform.SetValue(&value,1);
    } 
    void IEffect::SetUniformValue(StringID UniformID, void* value, const uint32 elementCount)
    {
        Uniforms[UniformID].uniform.SetValue(value,elementCount);
    }
    void IEffect::SetUniformValue(std::string Uniform, void* value, const uint32 elementCount)
    {
        StringID ID(MakeStringID(Uniform));
        Uniforms[ID].uniform.SetValue(value,elementCount);
    }

    void IEffect::UseColor()
    {
        Uniform Color;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        Color.Name = toSTR.str();
        Color.Type = DataType::Int;
        Color.Usage = PostEffects::Other;
        Color.SetValue(&TextureUnits);
        AddUniform(Color);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Color, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseDepth()
    {
        Uniform Depth;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        Depth.Name = toSTR.str();
        Depth.Type = DataType::Int;
        Depth.Usage = PostEffects::Other;
        Depth.SetValue(&TextureUnits);
        AddUniform(Depth);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Depth, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseLastRTT()
    {        
        Uniform RTT;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        RTT.Name = toSTR.str();
        RTT.Type = DataType::Int;
        RTT.Usage = PostEffects::Other;
        RTT.SetValue(&TextureUnits);
        AddUniform(RTT);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::LastRTT, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseCustomTexture(Texture* texture)
    {        
        Uniform CustomTexture;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        CustomTexture.Name = toSTR.str();
        CustomTexture.Type = DataType::Int;
        CustomTexture.Usage = PostEffects::Other;
        CustomTexture.SetValue(&TextureUnits);
        AddUniform(CustomTexture);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(texture, RTT::CustomTexture, TextureUnits));
        
        TextureUnits++;
    }
    
    void IEffect::Resize(const uint32 width, const uint32 height)
    {
        // Save Dimensions
        Width = width;
        Height = height;
        customDimensions = true;
    }
    
    bool IEffect::HaveCustomDimensions()
    {
        return customDimensions;
    }
    
    const uint32 IEffect::GetWidth() const
    {
        return Width;
    }
    const uint32 IEffect::GetHeight() const
    {
        return Height;
    }
}