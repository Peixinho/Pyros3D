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
        
        // Reset Shader Program
        ProgramObject = 0;
        
        // Initialize Shaders
        VertexShader = new Shader(ShaderType::VertexShader);
        FragmentShader = new Shader(ShaderType::FragmentShader);
        
        
        // Set Vertex Shader
        // Because its always the same
        VertexShaderString =  
                                    "attribute vec3 aPosition;"
                                    "attribute vec2 aTexcoord;"
                                    "varying vec2 vTexcoord;"
                                    "uniform mat4 uOrtho;"
                                    "void main() {"
                                        "gl_Position = uOrtho * vec4(aPosition,1.0);"
                                        "vTexcoord = aTexcoord;"
                                    "}";
        
        // Add Projection Matrix Uniform
        Uniform::Uniform proj;
        proj.Name = "uOrtho";
        proj.Type = Uniform::DataType::Matrix;
        proj.Usage = Uniform::DataUsage::ProjectionMatrix;
        AddUniform(proj);
        
        // Reset
        TextureUnits = 0;
        
    }

    void IEffect::UseRTT(const uint32& RTT)
    {
        // Set Textures
        if (RTT & RTT::Color) UseColor();
        if (RTT & RTT::LastRTT) UseLastRTT();
        if (RTT & RTT::Depth) UseDepth(); 
    }
    
    void IEffect::CompileShaders()
    {
        VertexShader->loadShaderText(VertexShaderString);
        FragmentShader->loadShaderText(FragmentShaderString);

        VertexShader->compileShader(&ProgramObject);
        FragmentShader->compileShader(&ProgramObject);
		Shader::LinkProgram(ProgramObject);
    }
    
    const uint32 IEffect::ShaderProgram()
    {
        return ProgramObject;
    }
    
    void IEffect::Destroy()
    {
        VertexShader->DeleteShader(ProgramObject);
        FragmentShader->DeleteShader(ProgramObject);
        Shader::DeleteProgram(&ProgramObject);
    }
    
    IEffect::~IEffect() {
    }

    void IEffect::AddUniform(const Uniform::Uniform &Data)
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
    void IEffect::SetUniformValue(StringID UniformID, void* value, const uint32 &elementCount)
    {
        Uniforms[UniformID].uniform.SetValue(value,elementCount);
    }
    void IEffect::SetUniformValue(std::string Uniform, void* value, const uint32 &elementCount)
    {
        StringID ID(MakeStringID(Uniform));
        Uniforms[ID].uniform.SetValue(value,elementCount);
    }

    void IEffect::UseColor()
    {
        Uniform::Uniform Color;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        Color.Name = toSTR.str();
        Color.Type = Uniform::DataType::Int;
        Color.Usage = Uniform::PostEffects::Other;
        Color.SetValue(&TextureUnits);
        AddUniform(Color);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Color, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseDepth()
    {
        Uniform::Uniform Depth;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        Depth.Name = toSTR.str();
        Depth.Type = Uniform::DataType::Int;
        Depth.Usage = Uniform::PostEffects::Other;
        Depth.SetValue(&TextureUnits);
        AddUniform(Depth);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Depth, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseLastRTT()
    {        
        Uniform::Uniform RTT;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        RTT.Name = toSTR.str();
        RTT.Type = Uniform::DataType::Int;
        RTT.Usage = Uniform::PostEffects::Other;
        RTT.SetValue(&TextureUnits);
        AddUniform(RTT);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::LastRTT, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseCustomTexture(Texture* texture)
    {        
        Uniform::Uniform CustomTexture;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        CustomTexture.Name = toSTR.str();
        CustomTexture.Type = Uniform::DataType::Int;
        CustomTexture.Usage = Uniform::PostEffects::Other;
        CustomTexture.SetValue(&TextureUnits);
        AddUniform(CustomTexture);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(texture, RTT::CustomTexture, TextureUnits));
        
        TextureUnits++;
    }
    
    void IEffect::Resize(const uint32& width, const uint32& height)
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