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
		depthBias = false;
		depthTest = depthWrite = true;
		depthTestMode = 0; // Less
		forceDepthWrite = false;
		blending = false;

		// Add Opacity Uniform
		opacityHandle = AddUniform(Uniform("uOpacity", Uniforms::DataType::Float, &this->opacity));

		// Set Internal ID
		materialID = _InternalID;

		// Increase Internal ID
		_InternalID++;
	}
	void IMaterial::Destroy() {}
	void IMaterial::SetOpacity(const f32 &opacity)
	{
		this->opacity = opacity;
		opacityHandle->SetValue(&this->opacity);
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
		if (isTransparent) return isTransparent;
		else return (opacity < 1);
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
	void IMaterial::EnableDethBias(f32 factor, f32 units)
	{
		depthBias = true;
		depthFactor = factor;
		depthUnits = units;
	}
	void IMaterial::DisableDethBias()
	{
		depthBias = false;
	}

	Uniform* IMaterial::AddUniform(const Uniform Data)
	{
		// Global Uniforms
		if ((int)Data.Usage<Uniforms::DataUsage::Other)
		{
			for (std::list<Uniform>::iterator i = GlobalUniforms.begin(); i != GlobalUniforms.end(); i++)
			{
				if ((*i).NameID == Data.NameID)
				{
					GlobalUniforms.erase(i);
					break;
				}
			}
			GlobalUniforms.push_back(Data);
			return &(GlobalUniforms.back());
		}
		// Game Object Uniforms
		else if ((int)Data.Usage>Uniforms::DataUsage::Other)
		{
			for (std::list<Uniform>::iterator i = ModelUniforms.begin(); i != ModelUniforms.end(); i++)
			{
				if ((*i).NameID == Data.NameID)
				{
					ModelUniforms.erase(i);
					break;
				}
			}
			ModelUniforms.push_back(Data);
			return &(ModelUniforms.back());
		}
		else // User Specific
		{
			for (std::list<Uniform>::iterator i = UserUniforms.begin(); i != UserUniforms.end(); i++)
			{
				if ((*i).NameID == Data.NameID)
				{
					UserUniforms.erase(i);
					break;
				}
			}
			UserUniforms.push_back(Data);
			return &(UserUniforms.back());
		}
	}
	
	void IMaterial::RemoveUniform(Uniform* handle)
	{
		// Global Uniforms
		if ((int)handle->Usage<Uniforms::DataUsage::Other)
		{
			for (std::list<Uniform>::iterator i = GlobalUniforms.begin(); i != GlobalUniforms.end(); i++)
			{
				if (&(*i) == handle)
				{
					GlobalUniforms.erase(i);
					break;
				}
			}
		}
		// Game Object Uniforms
		else if ((int)handle->Usage>Uniforms::DataUsage::Other)
		{
			for (std::list<Uniform>::iterator i = ModelUniforms.begin(); i != ModelUniforms.end(); i++)
			{
				if (&(*i) == handle)
				{
					ModelUniforms.erase(i);
					break;
				}
			}
		}
		else // User Specific
		{
			for (std::list<Uniform>::iterator i = UserUniforms.begin(); i != UserUniforms.end(); i++)
			{
				if (&(*i) == handle)
				{
					UserUniforms.erase(i);
					break;
				}
			}
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
