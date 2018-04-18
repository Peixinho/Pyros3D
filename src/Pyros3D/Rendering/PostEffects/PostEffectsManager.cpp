//============================================================================
// Name        : PostEffectsManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	PostEffectsManager::PostEffectsManager(const uint32 width, const uint32 height)
	{
		// Save Dimensions
		Width = width;
		Height = height;

		Color = new Texture();
		Color->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
		Color->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		Depth = new Texture();
		Depth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
		Depth->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// Initialize Internal FBO
		ExternalFBO = new FrameBuffer();
		ExternalFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, Depth);
		ExternalFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, Color);
	}

	FrameBuffer* PostEffectsManager::GetExternalFrameBuffer()
	{
		return ExternalFBO;
	}

	void PostEffectsManager::CaptureFrame()
	{
		ExternalFBO->Bind();
	}
	void PostEffectsManager::EndCapture()
	{
		ExternalFBO->UnBind();
	}

	void PostEffectsManager::Resize(const uint32 width, const uint32 height)
	{
		// Save Dimensions
		Width = width;
		Height = height;

		// Resize External FBO
		ExternalFBO->Resize(Width, Height);
	}

	void PostEffectsManager::ProcessPostEffects(Projection* projection)
	{
		// Set Counter
		uint32 counter = 1;

		// Save Near and Far Planes
		Vec2 NearFarPlane = Vec2(projection->Near, projection->Far);
		Vec2 ScreenDimensions = Vec2((f32)Width, (f32)Height);

		// Run Through Effects
		for (std::vector<IEffect*>::iterator effect = effects.begin(); effect != effects.end(); effect++)
		{
			if (counter == effects.size())
			{
				glViewport(0, 0, Width, Height);
			}
			else {

				activeFBO = (*effect)->fbo;

				glViewport(0, 0, (*effect)->Width, (*effect)->Height);

				// Bind FBO
				activeFBO->Bind();
			}

			// Clear Screen
			GLCHECKER(glClearColor(0.f, 0.f, 0.f, 0.f));
			GLCHECKER(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

			#if !defined(GLES2)
			GLuint vao;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
			#endif

			// Start Shader Program
			GLCHECKER(glUseProgram((*effect)->shader->ShaderProgram()));

			// Bind MRT
			for (std::vector<RTT::Info>::iterator i = (*effect)->RTTOrder.begin(); i != (*effect)->RTTOrder.end(); i++)
			{
				switch ((*i).Type)
				{
				case RTT::Color:
					Color->Bind();
					break;
				case RTT::Depth:
					Depth->Bind();
					break;
				case RTT::LastRTT:
					LastRTT->Bind();
					break;
				default:
					(*i).texture->Bind();
					break;
				}
			}

			// Send Uniforms
			for (std::list<__UniformPostProcess>::iterator i = (*effect)->Uniforms.begin(); i != (*effect)->Uniforms.end(); i++)
			{
				if ((*i).handle == -2)
				{
					(*i).handle = Shader::GetUniformLocation((*effect)->shader->ShaderProgram(), (*i).uniform.Name);
				}
				if ((*i).handle != -1)
				{
					switch ((*i).uniform.Usage)
					{
					case PostEffects::NearFarPlane:
					{
						(*i).uniform.Type = Uniforms::DataType::Vec2;
						Shader::SendUniform((*i).uniform, &NearFarPlane, (*i).handle);
					}
					break;
					case PostEffects::ScreenDimensions:
					{
						(*i).uniform.Type = Uniforms::DataType::Vec2;
						Shader::SendUniform((*i).uniform, &ScreenDimensions, (*i).handle);
					}
					break;
					case PostEffects::ProjectionFromScene:
					{
						(*i).uniform.Type = Uniforms::DataType::Matrix;
						Shader::SendUniform((*i).uniform, &projection->m, (*i).handle);
					}
					break;
					default:
					case PostEffects::Other:
					{
						Shader::SendUniform((*i).uniform, (*i).handle);
					}
					break;
					}
				}
			}

			GLCHECKER(glDrawArrays(GL_TRIANGLES, 0, 3));

			// Unbind MRT      
			for (std::vector<RTT::Info>::reverse_iterator i = (*effect)->RTTOrder.rbegin(); i != (*effect)->RTTOrder.rend(); i++)
			{
				switch ((*i).Type)
				{
				case RTT::Color:
					Color->Unbind();
					break;
				case RTT::Depth:
					Depth->Unbind();
					break;
				case RTT::LastRTT:
					LastRTT->Unbind();
					break;
				default:
					(*i).texture->Unbind();
					break;
				}
			}

			// Unbind FBO if is using and set the RTT
			if (counter < effects.size())
			{
				activeFBO->UnBind();
				// Get RTT
				LastRTT = activeFBO->GetAttachments()[0]->TexturePTR;
			}

			// count loop
			counter++;

		}

		// Disable Shader Program
		GLCHECKER(glUseProgram(0));
	}

	PostEffectsManager::~PostEffectsManager()
	{
		for (std::vector<IEffect*>::iterator i = effects.begin(); i != effects.end(); i++)
		{
			delete (*i);
		}

		delete ExternalFBO;

		// Destroy Textures
		delete Color;
		delete Depth;
	}

	void PostEffectsManager::AddEffect(IEffect* Effect)
	{
		// Add New Effect
		effects.push_back(Effect);
	}
	void PostEffectsManager::RemoveEffect(IEffect* Effect)
	{
		for (std::vector<IEffect*>::iterator i = effects.begin(); i != effects.end(); i++)
		{
			if ((*i) == Effect)
			{
				effects.erase(i);
				break;
			}
		}
	}

	const uint32 PostEffectsManager::GetNumberEffects() const
	{
		return effects.size();
	}

}
