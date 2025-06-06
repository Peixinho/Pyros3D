//============================================================================
// Name        : IEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Effect Interface
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

namespace p3d {

    IEffect::IEffect(const uint32 Width, const uint32 Height) 
    {
        
        // Reset Handles
        positionHandle = texcoordHandle = -2;
        
        // Initialize Shaders
        shader = new Shader();
        
        // Set Vertex Shader
        // Because its always the same
        VertexShaderString =  
				#if defined(GLES2)
					"#define varying_in varying\n"
					"#define varying_out varying\n"
					"#define attribute_in attribute\n"
					"#define texture_2D texture2D\n"
					"#define texture_cube textureCube\n"
					"precision mediump float;"
				#else
					"#define varying_in in\n"
					"#define varying_out out\n"
					"#define attribute_in in\n"
					"#define texture_2D texture\n"
					"#define texture_cube texture\n"
					#if defined(GLES3)
						"precision mediump float;\n"
					#endif
				#endif
				"varying_out vec2 vTexcoord;\n"
				"void main() {\n"
					"gl_Position = vec4(-1.0 + vec2((gl_VertexID & 1) << 2, (gl_VertexID & 2) << 1), 0.0, 1.0);\n"
					"vTexcoord = (gl_Position.xy+1.0)*0.5;\n"
				"}\n";
        
        // Reset
        TextureUnits = 0;

	// Set FrameBuffers
	fbo = new FrameBuffer();
	
	attachment = new Texture();
	attachment->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height);
	attachment->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	fbo->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, attachment);

	this->Width = Width;
	this->Height = Height;
        
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
    
    IEffect::~IEffect() 
	{
		shader->DeleteShader();
		delete shader;
		delete fbo;
		delete attachment;
    }

    Uniform* IEffect::AddUniform(const Uniform &Data)
    {
		for (std::list<__UniformPostProcess>::iterator i = Uniforms.begin(); i != Uniforms.end(); i++)
		{
			if ((*i).uniform.NameID == Data.NameID)
			{
				Uniforms.erase(i);
				break;
			}
		}
		Uniforms.push_back(__UniformPostProcess(Data));
		return &Uniforms.back().uniform;
    }

    void IEffect::UseColor()
    {
        Uniform Color;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
        AddUniform(Uniform(toSTR.str(), Uniforms::DataType::Int, &TextureUnits));
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Color, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseDepth()
    {
        Uniform Depth;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
		AddUniform(Uniform(toSTR.str(), Uniforms::DataType::Int, &TextureUnits));
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::Depth, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseLastRTT()
    {        
        Uniform RTT;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
		AddUniform(Uniform(toSTR.str(), Uniforms::DataType::Int, &TextureUnits));
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(RTT::LastRTT, TextureUnits));
        
        TextureUnits++;
    }
    void IEffect::UseCustomTexture(Texture* texture)
    {        
        Uniform CustomTexture;
        std::ostringstream toSTR; toSTR << "uTex" << TextureUnits;
	AddUniform(Uniform(toSTR.str(), Uniforms::DataType::Int, &TextureUnits));
        
        // Set RTT Order
        RTTOrder.push_back(RTT::Info(texture, RTT::CustomTexture, TextureUnits));
        
        TextureUnits++;
    }
    
    void IEffect::Resize(const uint32 width, const uint32 height)
    {
        // Save Dimensions
        Width = width;
        Height = height;
	fbo->Resize(width, height);
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
