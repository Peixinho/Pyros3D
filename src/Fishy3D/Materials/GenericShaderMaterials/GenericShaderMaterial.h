//============================================================================
// Name        : ShaderLib.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ShaderLib
//============================================================================

#ifndef GENERICSHADERMATERIAL_H
#define GENERICSHADERMATERIAL_H

#include "../IMaterial.h"
#include "ShaderLib.h"

#include <iostream>
#include <map>

namespace Fishy3D
{     
    class GenericShaderMaterial : public IMaterial
    {
        public:
            
            GenericShaderMaterial(const unsigned &options);
            virtual ~GenericShaderMaterial();
            // Set Colors
            void SetColor(const vec4 &color);
            void SetSpecular(const vec4 &specularColor);
            // Set Textures
            void SetColorMap(const Texture &colormap);
            void SetSpecularMap(const Texture &specular);
            void SetNormalMap(const Texture &normalmap);
            void SetEnvMap(const Texture &envmap);
            void SetRefractMap(const Texture &refractmap);
            void SetSkyboxMap(const Texture &skyboxmap);
            // Lights
            void ChangeLightingProperties(const vec4 &Ke, const vec4 &Ka, const vec4 &Kd, const vec4 &Ks, const float &shininess);

			// Render
            virtual void PreRender();
            virtual void AfterRender();
            
            // Bind
            void BindTextures();
            void UnbindTextures();
        
        private:
        
            // List of Tetxures
            std::map<unsigned, Texture> Textures;
        
        protected:
            // Shaders List
            static std::map<unsigned, SuperSmartPointer<Shaders> > ShadersList;
            // Save Shader Location on Shaders List
            unsigned shaderID;

			// Lighting Properties
			vec4 Ke;
			vec4 Ka;
			vec4 Kd;
			vec4 Ks;
			float Shininess, UseLights;
    };
}

#endif /* GENERICSHADERMATERIAL_H */
