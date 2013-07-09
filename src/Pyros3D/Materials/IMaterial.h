//============================================================================
// Name        : IMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IMaterial Interface
//============================================================================

#ifndef IMATERIAL_H
#define IMATERIAL_H

#include "../Core/Math/Math.h"
#include "GenericShaderMaterials/ShaderLib.h"
#include "Shaders/Shaders.h"
#include "Shaders/Uniforms.h"
#include "../Ext/StringIDs/StringID.hpp"
#include "../AssetManager/Assets/Texture/Texture.h"

#include <list>

namespace p3d {    
    
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
        void SetCullFace(const uint32 &face);
        uint32 GetCullFace() const;
        bool isWireFrame;
        bool IsWireFrame() const;
        const f32 &GetOpacity() const;
        bool IsTransparent() const;
        void SetOpacity(const f32 &opacity);
        void SetTransparencyFlag(bool transparency);

        // Uniforms        
        std::list<Uniform::Uniform> GlobalUniforms;
        std::list<Uniform::Uniform> ModelUniforms;
        std::map<StringID, Uniform::Uniform> UserUniforms;

        // Send Uniforms
        void SetUniformValue(std::string Uniform, int32 value);
        void SetUniformValue(StringID UniformID, int32 value); 
        void SetUniformValue(std::string Uniform, f32 value);
        void SetUniformValue(StringID UniformID, f32 value); 
        void SetUniformValue(std::string Uniform, void* value, const uint32 &elementCount = 1);
        void SetUniformValue(StringID UniformID, void* value, const uint32 &elementCount = 1);
        
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
        uint32 GetShader();
        
    protected :
        
        //Casting Shadows
        bool isCastingShadows;
        
        // Cull Face
        uint32 cullFace;
        
        // Transparency
        bool isTransparent;
        
        // Opacity
        f32 opacity;
        
        // Shader program        
        uint32 shaderProgram;
        
    };
    
}

#endif /* IMATERIALl_H */