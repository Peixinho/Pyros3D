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
        Uniforms.push_back(proj);
        
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

    void IEffect::UseColor()
    {
        Uniform::Uniform Color;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        Color.Name = toSTR.str();
        Color.Type = Uniform::DataType::Int;
        Color.Usage = Uniform::DataUsage::Other;
        Color.SetValue(&TextureUnits);
        Uniforms.push_back(Color);
        
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
        Depth.Usage = Uniform::DataUsage::Other;
        Depth.SetValue(&TextureUnits);
        Uniforms.push_back(Depth);
        
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
        RTT.Usage = Uniform::DataUsage::Other;
        RTT.SetValue(&TextureUnits);
        Uniforms.push_back(RTT);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::LastRTT, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseCustomTexture(const Texture &texture)
    {        
        Uniform::Uniform CustomTexture;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        CustomTexture.Name = toSTR.str();
        CustomTexture.Type = Uniform::DataType::Int;
        CustomTexture.Usage = Uniform::DataUsage::Other;
        CustomTexture.SetValue(&TextureUnits);
        Uniforms.push_back(CustomTexture);
        
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