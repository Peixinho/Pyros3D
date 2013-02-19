//============================================================================
// Name        : IEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Effect Interface
//============================================================================

#include "IEffect.h"
#include "SFML/Graphics/Color.hpp"

namespace Fishy3D {

    IEffect::IEffect() 
    {
        
        // Custom Dimensions
        customDimensions = false;
        
        // Reset Handles
        positionHandle = texcoordHandle = -2;
        
        // Reset Shader Program
        ProgramObject = 0;
        
        // Initialize Shaders
        VertexShader = SuperSmartPointer<Shader> (new Shader(ShaderType::VertexShader));
        FragmentShader = SuperSmartPointer<Shader> (new Shader(ShaderType::FragmentShader));
        
        
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
    
    void IEffect::UseRTT(const unsigned& RTT)
    {
        // Set Textures
        if (RTT & RTT::Color) UseColor();
        if (RTT & RTT::Position) UsePosition();
        if (RTT & RTT::LastRTT) UseLastRTT();
        if (RTT & RTT::Normal_Depth) UseNormalDepth(); 
    }
    
    void IEffect::CompileShaders()
    {
        VertexShader->loadShaderText(VertexShaderString);
        FragmentShader->loadShaderText(FragmentShaderString);
        
        VertexShader->compileShader(&ProgramObject);
        FragmentShader->compileShader(&ProgramObject);
    }
    
    const unsigned IEffect::ShaderProgram()
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
    void IEffect::UseNormalDepth()
    {
        Uniform::Uniform NormalDepth;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        NormalDepth.Name = toSTR.str();
        NormalDepth.Type = Uniform::DataType::Int;
        NormalDepth.Usage = Uniform::DataUsage::Other;
        NormalDepth.SetValue(&TextureUnits);
        Uniforms.push_back(NormalDepth);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Normal_Depth, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UsePosition()
    {
        Uniform::Uniform Position;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        Position.Name = toSTR.str();
        Position.Type = Uniform::DataType::Int;
        Position.Usage = Uniform::DataUsage::Other;
        Position.SetValue(&TextureUnits);
        Uniforms.push_back(Position);
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Position,TextureUnits));
        
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
    
    void IEffect::Resize(const unsigned& width, const unsigned& height)
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
    
    const unsigned IEffect::GetWidth() const
    {
        return Width;
    }
    const unsigned IEffect::GetHeight() const
    {
        return Height;
    }
}