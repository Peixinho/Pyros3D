//============================================================================
// Name        : IEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Effect Interface
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

namespace p3d {

    // Same shared-active-device pattern as Shaders.cpp/GeometryBuffer.cpp -
    // only needed here to destroy pipelineHandle on teardown.
    static IRenderDevice& Device()
    {
        return GetActiveRenderDevice();
    }

    IEffect::IEffect(const uint32 Width, const uint32 Height)
    {

        // Reset Handles
        positionHandle = texcoordHandle = -2;

        // Initialize Shaders
        shader = new Shader();
        pipelineHandle = 0;
        pipelineBuiltForSwapchainGeneration = 0;
        extraUniformsBinding = 0;
        extraUniformsSize = 0;
        extraUniformsBufferHandle = 0;

        // Set Vertex Shader
        // Because its always the same
        VertexShaderString =
					"#define varying_in in\n"
					"#define varying_out out\n"
					"#define attribute_in in\n"
					"#define texture_2D texture\n"
					"#define texture_cube texture\n"
					#if defined(GLES3)
						"precision mediump float;\n"
					#endif
					// gl_VertexID doesn't exist in Vulkan/SPIR-V GLSL - the
					// equivalent builtin is gl_VertexIndex. VULKAN is
					// predefined by shaderc itself for any Vulkan-target
					// compile (see SpirvShaderCompiler::Compile()'s
					// comment) - never true for the GL path, which keeps
					// using the real gl_VertexID unchanged. SPIR-V also
					// requires a static layout(location=) on every
					// varying (GL matches by name at link time instead) -
					// same IO_LOCATION pattern as PyrosShader.glsl. Every
					// effect's fragment shader must declare vTexcoord at
					// this same location (0) for the two stages to agree.
					"#if defined(VULKAN)\n"
					"#define gl_VertexID gl_VertexIndex\n"
					"#define IO_LOCATION(n) layout(location = n)\n"
					"#else\n"
					"#define IO_LOCATION(n)\n"
					"#endif\n"
				"IO_LOCATION(0) varying_out vec2 vTexcoord;\n"
				"void main() {\n"
					"gl_Position = vec4(-1.0 + vec2((gl_VertexID & 1) << 2, (gl_VertexID & 2) << 1), 0.0, 1.0);\n"
					// v is derived from clip-space Y, so it inherits whichever
					// way NDC Y points - and it then indexes a texture, whose
					// v=0 row is whichever end the *render target* stored
					// first. Those two agree on GL (NDC Y up, texture v=0 at
					// the bottom = the end NDC -1 wrote) and on Vulkan (NDC Y
					// down, v=0 at the top = again the end NDC -1 wrote), so
					// the naive mapping is correct on both. Metal is the odd
					// one out: NDC Y up like GL, but v=0 at the *top* like
					// Vulkan - so NDC -1 (bottom) has to read v=1, not v=0.
					// Without this every post-effect sampled its input
					// upside down, which is why deferred+tonemap scenes came
					// out vertically mirrored while plain forward ones (no
					// post-effect, drawing straight to the swapchain) did not.
					"#if defined(METAL)\n"
					"vTexcoord = vec2((gl_Position.x+1.0)*0.5, (1.0-gl_Position.y)*0.5);\n"
					"#else\n"
					"vTexcoord = (gl_Position.xy+1.0)*0.5;\n"
					"#endif\n"
				"}\n";

        // Reset
        TextureUnits = 0;

	// Set FrameBuffers
	fbo = new FrameBuffer();
	fbo->SetDebugName("Post effect");
	
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
		if (pipelineHandle != 0)
			Device().DestroyPipeline(pipelineHandle);
		if (extraUniformsBufferHandle != 0)
			Device().DestroyUniformBuffer(extraUniformsBufferHandle);
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
