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
#include <Pyros3D/Other/Export.h>

//#include <iostream>

namespace p3d {

	struct __UniformPostProcess
	{
		Uniforms::Uniform uniform;
		int32 handle;
	};

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
                Other
            };
        }
    }
    
    class PYROS3D_API IEffect {
        
        friend class PostEffectsManager;
        
        public:
                  
            IEffect();
            
            virtual ~IEffect();

            // Compile Shader
            void CompileShaders();           

            // Destroy
            void Destroy();
            
            // Custom Dimensions
            void Resize(const uint32 width, const uint32 height);
            bool HaveCustomDimensions();
            const uint32 GetWidth() const;
            const uint32 GetHeight() const;
            
            // Send Uniforms
            void SetUniformValue(std::string Uniform, int32 value);
            void SetUniformValue(StringID UniformID, int32 value); 
            void SetUniformValue(std::string Uniform, f32 value);
            void SetUniformValue(StringID UniformID, f32 value); 
            void SetUniformValue(std::string Uniform, void* value, const uint32 elementCount = 1);
            void SetUniformValue(StringID UniformID, void* value, const uint32 elementCount = 1);

        protected:

            void AddUniform(const Uniform &Data);

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
            
            // Custom Dimensions
            bool customDimensions;
            uint32 Width, Height;

        private:
            std::map<uint32, __UniformPostProcess> Uniforms;
            std::string VertexShaderString;   
            
            void UseColor();
            void UseDepth();
            void UseLastRTT();
            
    };

};

#endif	/* IEFFECT_H */