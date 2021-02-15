//============================================================================
// Name        : IEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Effect Interface
//============================================================================

#ifndef IEFFECT_H
#define IEFFECT_H

#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Other/Export.h>
#include <list>

//#include <iostream>

namespace p3d {

	namespace RTT {
		enum {
			Color = 1 << 0,
			Depth = 1 << 1,
			LastRTT = 1 << 2,
			CustomTexture = 1 << 3
		};
		struct Info {
			uint32 Type;
			Texture* texture;
			uint32 Unit;
			Info(const uint32 type, const uint32 unit = 0) { Type = type; Unit = unit; }
			Info(Texture *texture, const uint32 type, const uint32 unit = 0) { Type = type; Unit = unit; this->texture = texture; }
		};
	}

	namespace Uniforms {
		namespace PostEffects {
			enum {
				NearFarPlane,
				ScreenDimensions,
				ProjectionFromScene,
				Other
			};
		}
	}

	struct __UniformPostProcess
	{
		__UniformPostProcess(const Uniform &Data)
		{
			uniform = Data;
			handle = -2;
		}
		Uniform uniform;
		int32 handle;
	};

	class PYROS3D_API IEffect {

		friend class PostEffectsManager;

	public:

		IEffect(const uint32 Width, const uint32 Height);

		virtual ~IEffect();

		// Compile Shader
		void CompileShaders();

		// Custom Dimensions
		void Resize(const uint32 width, const uint32 height);
		const uint32 GetWidth() const;
		const uint32 GetHeight() const;

		Texture* GetTexture() { return attachment; }

	protected:

		Uniform* AddUniform(const Uniform &Data);

		int32 positionHandle, texcoordHandle;

		// RTT to Use
		void UseCustomTexture(Texture *texture);
		void UseRTT(const uint32 RTT);

		// Shaders Strings
		std::string FragmentShaderString;

		// Shaders
		Shader* shader;

		// Texture Units 
		int32 TextureUnits;

		// RTT Order
		std::vector<RTT::Info> RTTOrder;

		uint32 Width, Height;
		FrameBuffer* fbo;
		Texture* attachment;

		std::string VertexShaderString;

	private:
		std::list<__UniformPostProcess> Uniforms;

		void UseColor();
		void UseDepth();
		void UseLastRTT();

	};

};

#endif	/* IEFFECT_H */
