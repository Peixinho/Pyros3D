//============================================================================
// Name        : ShaderLib.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ShaderLib
//============================================================================

#ifndef GENERICSHADERMATERIAL_H
#define GENERICSHADERMATERIAL_H

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Other/Export.h>
#include <iostream>
#include <map>

namespace p3d
{     
    class PYROS3D_API GenericShaderMaterial : public IMaterial
    {
        public:
            
            GenericShaderMaterial(const uint32 &options);
            virtual ~GenericShaderMaterial();
            // Set Colors
            void SetColor(const Vec4 &color);
            void SetSpecular(const Vec4 &specularColor);
            // Set Textures
            void SetColorMap(Texture* colormap);
            void SetSpecularMap(Texture* specular);
            void SetNormalMap(Texture* normalmap);
            void SetEnvMap(Texture* envmap);
            void SetReflectivity(const f32 &reflectivity);
            void SetRefractMap(Texture* refractmap);
            void SetSkyboxMap(Texture* skyboxmap);
            // Lights
            void SetLightingProperties(const Vec4 &Ke, const Vec4 &Ka, const Vec4 &Kd, const Vec4 &Ks, const f32 &shininess);
            void SetKe(const Vec4 &Ke);
            void SetKa(const Vec4 &Ka);
            void SetKd(const Vec4 &Kd);
            void SetKs(const Vec4 &Ks);
            void SetShininess(const f32 &shininess);

            // Text
            void SetTextFont(Font* font);
            
            void AddTexture(const std::string &uniformName, Texture* texture);

            // Render
            virtual void PreRender();
            virtual void AfterRender();
            
            // Bind
            void BindTextures();
            void UnbindTextures();
        
            // Shadows
            void SetPCFTexelSize(const f32 &texel);
            void SetPCFTexelCascadesSize(const f32 &texel1,const f32 &texel2 = 0.0001f,const f32 &texel3 = 0.0001f,const f32 &texel4 = 0.0001f);

        private:
        
            // List of Tetxures
            std::map<uint32, Texture*> Textures;
        
        protected:
            // Shaders List
            static std::map<uint32, Shader* > ShadersList;
            // Save Shader Location on Shaders List
            uint32 shaderID;

            // Lighting Properties
            Vec4 Ke;
            Vec4 Ka;
            Vec4 Kd;
            Vec4 Ks;
            f32 Shininess, UseLights;
            
            // Shadows
            f32 PCFTexelSize1;
            f32 PCFTexelSize2;
            f32 PCFTexelSize3;
            f32 PCFTexelSize4;

            // Environment Cube
            f32 Reflectivity;
        
            // Texture IDs
            int32 colorMapID, specularMapID, normalMapID, envMapID, skyboxMapID, refractMapID, fontMapID;
    };
}

#endif /* GENERICSHADERMATERIAL_H */
