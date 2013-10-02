//============================================================================
// Name        : IMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IMaterial Interface
//============================================================================

#ifndef IMaterial_H
#define IMaterial_H

#include "../Core/Math/Math.h"
#include "../Utils/Pointers/SuperSmartPointer.h"
#include "GenericShaderMaterials/ShaderLib.h"
#include "Shaders/Shaders.h"
#include "Shaders/Uniforms.h"
#include "../Utils/StringIDs/StringID.hpp"
#include "../Textures/Texture.h"

#include <list>

namespace Fishy3D {    
    
    // Culling
    namespace CullFace {
        enum {
            BackFace,
            FrontFace,
            DoubleSided
        };
    }
    
    class IMaterial {
        
        friend class IRenderer;
        friend class ForwardRenderer;
        
    public:

        // Constructor
       IMaterial();       
        
        // Destructor
        virtual ~IMaterial();
        
        // Virtual Methods
        virtual void PreRender() {}
        virtual void Render() {}
        virtual void AfterRender() {}
        
        // Properties
        void SetCullFace(const unsigned &face);
        unsigned GetCullFace() const;
        bool isWireFrame;
        bool IsWireFrame() const;
        const float &GetOpacity() const;
        bool IsTransparent() const;
        void SetOpacity(const float &opacity);
        void SetTransparencyFlag(bool transparency);

        // Uniforms        
        std::list<Uniform::Uniform> GlobalUniforms;
        std::list<Uniform::Uniform> ModelUniforms;
        std::map<StringID, Uniform::Uniform> UserUniforms;

        // Send Uniforms
        void SetUniformValue(std::string Uniform, int value);
        void SetUniformValue(StringID UniformID, int value); 
        void SetUniformValue(std::string Uniform, float value);
        void SetUniformValue(StringID UniformID, float value); 
        void SetUniformValue(std::string Uniform, void* value, const unsigned &elementCount = 1);
        void SetUniformValue(StringID UniformID, void* value, const unsigned &elementCount = 1);
        
        // Add Uniform
        void AddUniform(Uniform::Uniform Data);
        
        // Render WireFrame
        void StartRenderWireFrame();
        void StopRenderWireFrame();
        
        // Shadows Casting
        void EnableCastingShadows();
        void DisableCastingShadows();
        bool IsCastingShadows();
        
        // Destroy
        void Destroy();
        
        // Get Shader Program
        unsigned GetShader();
        
    protected :
        
        //Casting Shadows
        bool isCastingShadows;
        
        // Cull Face
        unsigned cullFace;
        
        // Transparency
        bool isTransparent;
        
        // Opacity
        float opacity;
        
        // Shader program        
        unsigned shaderProgram;
        
    };
    
}

#endif /* IMaterial_H */