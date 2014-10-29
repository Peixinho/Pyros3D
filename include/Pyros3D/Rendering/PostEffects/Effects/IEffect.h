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

//#include <iostream>

namespace p3d {

	struct __UniformPostProcess
	{
		Uniform::Uniform uniform;
		int32 handle;

		__UniformPostProcess(Uniform::Uniform u) { handle = -2; uniform = u; }
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
            Texture texture;
            uint32 Unit;
            Info(const uint32 &type, const uint32 &unit = 0) { Type = type; Unit = unit; }
            Info(const Texture &texture, const uint32 &type, const uint32 &unit = 0) { Type = type; Unit = unit; this->texture = texture; }
        };
    }
    
    namespace Uniform {
        namespace PostEffects {
            enum {
                ProjectionMatrix = 0,
                NearFarPlane
            };
        }
    }
    
    class IEffect {
        
        friend class PostEffectsManager;
        
        public:
                  
            IEffect();
            
            virtual ~IEffect();

            // Compile Shader
            void CompileShaders();
            // Shader Program
            const uint32 ShaderProgram();            

            // Destroy
            void Destroy();
            
            // Custom Dimensions
            void Resize(const uint32 &width, const uint32 &height);
            bool HaveCustomDimensions();
            const uint32 GetWidth() const;
            const uint32 GetHeight() const;
            
        protected:

            int32 positionHandle, texcoordHandle;
            std::vector<__UniformPostProcess> Uniforms;
            
            // RTT to Use
            void UseCustomTexture(const Texture &texture);
            void UseRTT(const uint32 &RTT);
            
            // Shaders Strings
            std::string FragmentShaderString;
            // Shaders
            Shader* VertexShader, *FragmentShader;
            // Shader Program
            uint32 ProgramObject;
            
            // Texture Units 
            uint32 TextureUnits;
            
            // RTT Order
            std::vector<RTT::Info> RTTOrder;
            
            // Custom Dimensions
            bool customDimensions;
            uint32 Width, Height;
            
        private:
            std::string VertexShaderString;   
            
            void UseColor();
            void UseDepth();
            void UseLastRTT();
            
    };

};

#endif	/* IEFFECT_H */