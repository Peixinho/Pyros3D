//============================================================================
// Name        : IMaterial.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IMaterial Interface
//============================================================================

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Rendering/RenderState.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

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
		// Saved with every material whether the bias is on or not: left
		// unset they were whatever was in memory, different in every file.
		depthFactor = depthUnits = 0.f;
		// Was never set: IsTransparent() is `opacity < 1`, so a garbage
		// opacity (common: a small float left on the stack) made the mesh
		// skip the shadow pass entirely - it still drew and received
		// shadows, but never cast or self-shadowed.
		opacity = 1.f;
		depthTest = depthWrite = true;
		depthTestMode = 0; // Less
		forceDepthWrite = false;
		blending = false;
		// Were never initialised: 0 on macOS, which is Zero/Zero - so the
		// editor's "Blending" checkbox, which sets no factors, drew every
		// material solid black - and whatever the stack held on Windows.
		// Standard alpha blending, the same pair IRenderer uses for a
		// transparent material without its own.
		sfactor = BlendFunc::Src_Alpha;
		dfactor = BlendFunc::One_Minus_Src_Alpha;
		mode = BlendEq::Add;

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
	IMaterial::~IMaterial()
	{
		for (int i = 0; i < 2; i++)
			if (extraUniforms[i].bufferHandle != 0)
				GetActiveRenderDevice().DestroyUniformBuffer(extraUniforms[i].bufferHandle);
	}

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

	void IMaterial::SetExtraUniformBlock(int index, uint32 binding, const std::string &blockName,
		uint32 size, const std::map<std::string, uint32> &offsets)
	{
		if (index < 0 || index > 1) return;
		ExtraUniformsBlock &b = extraUniforms[index];
		b.binding = binding;
		b.blockName = blockName;
		b.size = size;
		b.scratch.assign(size, 0);
		b.offsets = offsets;
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
