//============================================================================
// Name        : IEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Effect Interface
//============================================================================

#ifndef IEFFECT_H
#define IEFFECT_H

#include "../../../Materials/Shaders/Shaders.h"
#include "../../../Utils/Pointers/SuperSmartPointer.h"
#include "../../../Core/Projection/Projection.h"
#include "../../../Textures/Texture.h"

//#include <iostream>

namespace Fishy3D {

    namespace RTT {
        enum {
            Color = 1 << 0,
            Normal_Depth = 1 << 1,
            Position = 1 << 2,
            LastRTT = 1 << 3,
            CustomTexture = 1 << 4
        };
        struct Info {
            unsigned Type;
            Texture texture;
            unsigned Unit;
            Info(const unsigned &type, const unsigned &unit = 0) { Type = type; Unit = unit; }
            Info(const Texture &texture, const unsigned &type, const unsigned &unit = 0) { Type = type; Unit = unit; this->texture = texture; }
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
            const unsigned ShaderProgram();            

            // Destroy
            void Destroy();
            
            // Custom Dimensions
            void Resize(const unsigned &width, const unsigned &height);
            bool HaveCustomDimensions();
            const unsigned GetWidth() const;
            const unsigned GetHeight() const;
            
        protected:
            
            int positionHandle, texcoordHandle;
            std::vector<Uniform::Uniform> Uniforms;
            
            // RTT to Use
            void UseCustomTexture(const Texture &texture);
            void UseRTT(const unsigned &RTT);
            
            // Shaders Strings
            std::string FragmentShaderString;
            // Shaders
            SuperSmartPointer<Shader> VertexShader, FragmentShader;
            // Shader Program
            unsigned ProgramObject;
            
            // Texture Units 
            unsigned TextureUnits;
            
            // RTT Order
            std::vector<RTT::Info> RTTOrder;
            
            // Custom Dimensions
            bool customDimensions;
            unsigned Width, Height;
            
        private:
            std::string VertexShaderString;   
            
            void UseColor();
            void UseNormalDepth();
            void UsePosition();
            void UseLastRTT();
            
    };

};

#endif	/* IEFFECT_H */