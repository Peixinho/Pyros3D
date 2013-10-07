//============================================================================
// Name        : ShaderLib.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ShaderLib
//============================================================================

#include "ShaderLib.h"
#include "../../Core/Logs/Log.h"
namespace p3d
{
    
    void ShaderLib::BuildShader(const uint32& option, Shaders* shader)
    {

        // Shader Strings
        std::string vertex;
        std::string fragment;
        
        // Flags
        bool usingNormal = false;
        bool usingCameraPosition = false;
        bool usingTexcoords = false;
        bool usingVertexWorldPos = false;
        bool usingVertexViewPos = false;
        bool usingVertexLocalPos = false;
        bool usingReflect = false;
        bool usingTangentMatrix = false;
        bool usingPCFTexelSize = false;
        
        // Aux Strings
        std::string fragmentShaderHeader, fragmentShaderBody;
        std::string vertexShaderHeader, vertexShaderBody;
        
        // Defaults
        // Vertex Header
        vertexShaderHeader+="attribute vec3 aPosition, aNormal;\n";
        vertexShaderHeader+="attribute vec2 aTexcoord;\n";
        vertexShaderHeader+="uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;\n";
        // Vertex Body
        vertexShaderBody+="void main() {\n";
        vertexShaderBody+="gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);\n";
        
        // Fragment Header
        fragmentShaderHeader+="vec4 diffuse = vec4(0.0,0.0,0.0,1.0);\n";
        fragmentShaderHeader+="vec4 specular = vec4(0.0,0.0,0.0,1.0);\n";
        fragmentShaderHeader+="bool diffuseIsSet = false;\n";
        fragmentShaderHeader+="uniform float uOpacity;\n";
        // Fragment Body
        fragmentShaderBody+="void main() {\n";
        
        if (option & ShaderUsage::Color)
        {
            // Fragment Header
            fragmentShaderHeader+="uniform vec4 uColor;\n";
            // Fragment Body
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=uColor; diffuseIsSet=true;} else diffuse *= uColor;\n";
        }
        if (option & ShaderUsage::PhysicsDebug)
        {
            // Fragment Header
            vertexShaderHeader+="attribute vec4 aColor;\n";
            vertexShaderHeader+="varying vec4 vColor;\n";
            vertexShaderBody+="vColor = aColor;\n";
            // Fragment Body
            fragmentShaderHeader+="varying vec4 vColor;\n";
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=vColor; diffuseIsSet=true;} else diffuse *= vColor;\n";
        }        
        if (option & ShaderUsage::Texture)
        {
            if (!usingTexcoords)
            {
                usingTexcoords = true;
                // Vertex Header
                vertexShaderHeader+="varying vec2 vTexcoord;\n";
                // Vertex Body
                vertexShaderBody+="vTexcoord = aTexcoord;\n";
                // Fragment Header
                fragmentShaderHeader+="varying vec2 vTexcoord;\n";
                fragmentShaderHeader+="vec2 Texcoord = vTexcoord;\n";
            }
            fragmentShaderHeader+="uniform sampler2D uColormap;\n";
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=texture2D(uColormap,Texcoord); diffuseIsSet=true;} else diffuse *= texture2D(uColormap,Texcoord);\n";
        }
        if (option & ShaderUsage::TextRendering)
        {
            if (!usingNormal)
            {
                usingNormal = true;
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = aNormal;\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = vNormal;\n";
            }
            if (!usingTexcoords)
            {
                usingTexcoords = true;
                // Vertex Header
                vertexShaderHeader+="varying vec2 vTexcoord;\n";
                // Vertex Body
                vertexShaderBody+="vTexcoord = aTexcoord;\n";
                // Fragment Header
                fragmentShaderHeader+="varying vec2 vTexcoord;\n";
                fragmentShaderHeader+="vec2 Texcoord = vTexcoord;\n";
            }
            fragmentShaderHeader+="uniform sampler2D uFontmap;\n";
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=vec4(Normal*texture2D(uFontmap,Texcoord).a,1.0); diffuseIsSet=true;} else diffuse *= vec4(Normal*texture2D(uFontmap,Texcoord).a,1.0);\n";
        }
        if (option & ShaderUsage::DirectionalShadow)
        {
            if (!usingVertexWorldPos)
            {
                usingVertexWorldPos = true;
                vertexShaderHeader+="varying vec4 vWorldPosition;\n";
                vertexShaderBody+="vWorldPosition=uModelMatrix * vec4(aPosition,1.0);\n";
                fragmentShaderHeader+="varying vec4 vWorldPosition;\n";
            }

            if (!usingPCFTexelSize)
            {
                    fragmentShaderHeader+="uniform float uPCFTexelSize1;\n";
                    fragmentShaderHeader+="uniform float uPCFTexelSize2;\n";
                    fragmentShaderHeader+="uniform float uPCFTexelSize3;\n";
                    fragmentShaderHeader+="uniform float uPCFTexelSize4;\n";
            }
            
            // Fragment Header
            fragmentShaderHeader+="uniform mat4 uDirectionalDepthsMVP[4];\n";
            fragmentShaderHeader+="uniform vec4 uDirectionalShadowFar[4];\n";
            
            // Directional Lights

            // PCF
            fragmentShaderHeader+="float PCF(sampler2DShadow shadowMap, float width, float height, mat4 sMatrix, float scale, vec4 pos, bool MoreThanOneCascade) {\n";
            fragmentShaderHeader+="vec4 coord = sMatrix * pos;\n";
            fragmentShaderHeader+="if (MoreThanOneCascade) coord.xy = (coord.xy * 0.5) + vec2(width,height);\n";
            fragmentShaderHeader+="float shadow = 0.0;\n";
            fragmentShaderHeader+="float x,y;\n";
            fragmentShaderHeader+="for (y = -1.5 ; y <=1.5 ; y+=1.0)\n";
            fragmentShaderHeader+="for (x = -1.5 ; x <=1.5 ; x+=1.0)\n";
            fragmentShaderHeader+="shadow += shadow2D(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;\n";
            fragmentShaderHeader+="shadow /= 16.0;\n";
            fragmentShaderHeader+="return shadow;\n";
            fragmentShaderHeader+="}\n";

            fragmentShaderHeader+="uniform sampler2DShadow uDirectionalShadowMaps;\n";
            
            // Fragment Body
            fragmentShaderBody+="float DirectionalShadow = 1.0;\n";
            fragmentShaderBody+="bool MoreThanOneCascade = (uDirectionalShadowFar[0].y>0.0);\n";
            fragmentShaderBody+="if (gl_FragCoord.z<uDirectionalShadowFar[0].x) DirectionalShadow = PCF( uDirectionalShadowMaps, 0.0, 0.0, uDirectionalDepthsMVP[0],uPCFTexelSize1,vWorldPosition, MoreThanOneCascade);\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uDirectionalShadowFar[0].y) DirectionalShadow = PCF( uDirectionalShadowMaps, 0.5,0.0, uDirectionalDepthsMVP[1],uPCFTexelSize2,vWorldPosition, MoreThanOneCascade);\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uDirectionalShadowFar[0].z) DirectionalShadow = PCF( uDirectionalShadowMaps, 0.0, 0.5, uDirectionalDepthsMVP[2],uPCFTexelSize3,vWorldPosition, MoreThanOneCascade);\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uDirectionalShadowFar[0].w) DirectionalShadow = PCF( uDirectionalShadowMaps, 0.5,0.5, uDirectionalDepthsMVP[3],uPCFTexelSize4,vWorldPosition, MoreThanOneCascade);\n";
            fragmentShaderBody+="diffuse.xyz*=vec3(DirectionalShadow+0.5);\n";

        }
        if (option & ShaderUsage::PointShadow)
        {
            if (!usingVertexWorldPos)
            {
                usingVertexWorldPos = true;
                vertexShaderHeader+="varying vec4 vWorldPosition;\n";
                vertexShaderBody+="vWorldPosition=uModelMatrix * vec4(aPosition,1.0);\n";
                fragmentShaderHeader+="varying vec4 vWorldPosition;\n";
            }
            
            if (!usingPCFTexelSize)
            {
                fragmentShaderHeader+="uniform float uPCFTexelSize1;\n";
            }

            fragmentShaderHeader+="#extension GL_EXT_gpu_shader4 : require\n";
            
            // PCF
            fragmentShaderHeader+="float PCF(samplerCubeShadow shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, vec4 pos) {\n";
            fragmentShaderHeader+="vec4 position_ls = Matrix2 * vWorldPosition;\n";
            fragmentShaderHeader+="position_ls.xyz/=position_ls.w;\n";
            fragmentShaderHeader+="vec4 abs_position = abs(position_ls);\n";
            fragmentShaderHeader+="float fs_z = -max(abs_position.x, max(abs_position.y, abs_position.z));\n";
            fragmentShaderHeader+="vec4 clip = Matrix1 * vec4(0.0, 0.0, fs_z, 1.0);\n";
            fragmentShaderHeader+="float depth = (clip.z / clip.w) * 0.5 + 0.5;\n";
            fragmentShaderHeader+="float shadow = 0.0;\n";
            fragmentShaderHeader+="float x,y;\n";
            fragmentShaderHeader+="for (y = -1.5 ; y <=1.5 ; y+=1.0)\n";
            fragmentShaderHeader+="for (x = -1.5 ; x <=1.5 ; x+=1.0)\n";
            fragmentShaderHeader+="shadow += shadowCube(shadowMap, vec4(position_ls.xyz, depth) + vec4(vec2(x,y) * scale,0.0,0.0)).x;\n";
            fragmentShaderHeader+="shadow /= 16.0;\n";
            fragmentShaderHeader+="return shadow;\n";
            fragmentShaderHeader+="}\n";
            
            
            // Fragment Header
            fragmentShaderHeader+="uniform mat4 uPointDepthsMVP[8];\n";
            fragmentShaderHeader+="uniform int uNumberOfPointShadows;\n"; 
            
            // Fragment Header
            fragmentShaderHeader+="uniform samplerCubeShadow uPointShadowMaps[4];\n";
            
            // shadow map test
            fragmentShaderBody+="float PointShadow = 0.0;\n";
            fragmentShaderBody+="for (int i=0;i<4;i++)\n";
            fragmentShaderBody+="if (i<uNumberOfPointShadows) {\n";
            fragmentShaderBody+="PointShadow+=PCF(uPointShadowMaps[i],uPointDepthsMVP[(i*2)],uPointDepthsMVP[(i*2+1)],uPCFTexelSize1,vWorldPosition);\n";
            fragmentShaderBody+="}\n";
            fragmentShaderBody+="diffuse.xyz*=vec3(PointShadow+0.5);\n";
        }
        if (option & ShaderUsage::SpotShadow)
        {
            if (!usingVertexWorldPos)
            {
                usingVertexWorldPos = true;
                vertexShaderHeader+="varying vec4 vWorldPosition;\n";
                vertexShaderBody+="vWorldPosition=uModelMatrix * vec4(aPosition,1.0);\n";
                fragmentShaderHeader+="varying vec4 vWorldPosition;\n";
            }
            
            if (!usingPCFTexelSize)
            {
                fragmentShaderHeader+="uniform float uPCFTexelSize1;\n";
            }

            // PCF
            fragmentShaderHeader+="float PCF(sampler2DShadow shadowMap, mat4 sMatrix, float scale, vec4 pos) {\n";
            fragmentShaderHeader+="vec4 coord = sMatrix * pos;\n";
            fragmentShaderHeader+="coord.xyz/=coord.w;\n";
            fragmentShaderHeader+="float shadow = 0.0;\n";
            fragmentShaderHeader+="float x,y;\n";
            fragmentShaderHeader+="for (y = -1.5 ; y <=1.5 ; y+=1.0)\n";
            fragmentShaderHeader+="for (x = -1.5 ; x <=1.5 ; x+=1.0)\n";
            fragmentShaderHeader+="shadow += shadow2D(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;\n";
            fragmentShaderHeader+="shadow /= 16.0;\n";
            fragmentShaderHeader+="return shadow;\n";
            fragmentShaderHeader+="}\n";
            
            // Fragment Header
            fragmentShaderHeader+="uniform sampler2DShadow uSpotShadowMaps[4];\n";
            fragmentShaderHeader+="uniform mat4 uSpotDepthsMVP[4];\n";
            fragmentShaderHeader+="uniform int uNumberOfSpotShadows;\n"; 
            
            // Fragment Body
            fragmentShaderBody+="float SpotShadow = 0.0;\n";
            fragmentShaderBody+="for (int i=0;i<4;i++)\n";
            fragmentShaderBody+="if (i<2) {\n";
            fragmentShaderBody+="SpotShadow+=PCF(uSpotShadowMaps[i],uSpotDepthsMVP[i],uPCFTexelSize1,vWorldPosition);\n";
            fragmentShaderBody+="}\n";
            fragmentShaderBody+="diffuse.xyz*=vec3(SpotShadow+0.5);\n";
        }
        if (option & ShaderUsage::CastShadows)
        {
            fragmentShaderBody+="diffuse.x=gl_FragCoord.z;\n";
        }
        if (option & ShaderUsage::BumpMapping)
        {
            if (!usingNormal)
            {
                usingNormal = true;
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uViewMatrix * uModelMatrix * vec4(aNormal,0)).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
            }
            if (!usingTexcoords)
            {
                usingTexcoords = true;
                // Vertex Header
                vertexShaderHeader+="varying vec2 vTexcoord;\n";
                // Vertex Body
                vertexShaderBody+="vTexcoord = aTexcoord;\n";
                // Fragment Header
                fragmentShaderHeader+="varying vec2 vTexcoord;\n";
                fragmentShaderHeader+="vec2 Texcoord = vTexcoord;\n";
            }
            
            if (!usingTangentMatrix)
            {
                usingTangentMatrix = true;
                // Vertex Header
                vertexShaderHeader+="attribute vec3 aTangent, aBitangent;\n";
                vertexShaderHeader+="varying mat3 vTangentMatrix;\n";
                // Vertex Body
                vertexShaderBody+="vec3 Tangent = normalize((uViewMatrix * uModelMatrix * vec4(aTangent,0)).xyz);\n";
                vertexShaderBody+="vec3 Binormal = normalize(cross(vNormal,Tangent));\n";
                vertexShaderBody+="vTangentMatrix = mat3(Tangent.x, Binormal.x, vNormal.x,Tangent.y, Binormal.y, vNormal.y,Tangent.z, Binormal.z, vNormal.z);\n";
                fragmentShaderHeader+="varying mat3 vTangentMatrix;\n";
                fragmentShaderBody+="mat3 TangentMatrix = vTangentMatrix;\n";
            }
            
            // Fragment Header
            fragmentShaderHeader+="uniform sampler2D uNormalmap;\n";
            // Fragment Body
            fragmentShaderBody+="vec3 Normal=(texture2D(uNormalmap, Texcoord).xyz * 2.0 - 1.0);\n";
        }
        if (option & ShaderUsage::Skinning)
        {
            vertexShaderHeader+="attribute vec4 aBonesID, aBonesWeight;\n";
            vertexShaderHeader+="uniform mat4 uBoneMatrix[50];\n";
            vertexShaderBody+="mat4 matAnimation = uBoneMatrix[int(aBonesID.x)] * aBonesWeight.x;\n";
            vertexShaderBody+="matAnimation += uBoneMatrix[int(aBonesID.y)] * aBonesWeight.y;\n";
            vertexShaderBody+="matAnimation += uBoneMatrix[int(aBonesID.z)] * aBonesWeight.z;\n";
            vertexShaderBody+="matAnimation += uBoneMatrix[int(aBonesID.w)] * aBonesWeight.w;\n";
            // Overwrites the gl_Position
            vertexShaderBody+="gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * (matAnimation * vec4(aPosition,1.0));\n";
            
            if (usingNormal)
            {
                vertexShaderBody+="vNormal = normalize((uViewMatrix * uModelMatrix * (matAnimation * vec4(aNormal,0.0))).xyz);\n";
            } else {
                usingNormal = true;
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uViewMatrix * uModelMatrix * (matAnimation * vec4(aNormal,0.0))).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = vNormal;\n";
            }
            if (usingVertexLocalPos)
            {
                vertexShaderBody+="vLocalPosition=uViewMatrix * uModelMatrix * (matAnimation * vec4(aPosition,1.0));\n";
            } else {
                usingVertexLocalPos = true;
                vertexShaderHeader+="varying vec4 vLocalPosition;\n";
                vertexShaderBody+="vLocalPosition=uViewMatrix * uModelMatrix * (matAnimation * vec4(aPosition,1.0));\n";
                fragmentShaderHeader+="varying vec4 vLocalPosition;\n";
            }
        }
        if (option & ShaderUsage::EnvMap)
        {
            if (!usingCameraPosition)
            {
                usingCameraPosition = true;
                // Vertex Header
                vertexShaderHeader+="uniform vec3 uCameraPos;\n";
                vertexShaderHeader+="varying vec3 vCameraPos;\n";
                vertexShaderBody+="vCameraPos = uCameraPos;\n";
                // Fragment Header
                fragmentShaderHeader+="varying vec3 vCameraPos;\n";
            }
            if (!usingVertexLocalPos)
            {
                usingVertexLocalPos = true;
                vertexShaderHeader+="varying vec4 vLocalPosition;\n";
                vertexShaderBody+="vLocalPosition=uViewMatrix * uModelMatrix * vec4(aPosition,1.0);\n";
                fragmentShaderHeader+="varying vec4 vLocalPosition;\n";
            }
            if (!usingNormal)
            {
                usingNormal = true;
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uViewMatrix * uModelMatrix * vec4(aNormal,0.0)).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = vNormal;\n";
            }
            if (!usingReflect)
            {
                usingReflect = true;
                
                // Vertex Header
                vertexShaderHeader+="varying vec3 vReflection;\n";
                // Vertex Body
                vertexShaderBody+="vReflection = reflect(vLocalPosition.xyz - (uViewMatrix * vec4(uCameraPos,1.0)).xyz,vNormal);\n";
                
                // Fragment Header
                fragmentShaderHeader+="varying vec3 vReflection;\n";
            }
            
            // Fragment Header
            fragmentShaderHeader+="uniform samplerCube uEnvmap;\n";
            fragmentShaderHeader+="uniform float uReflectivity;\n";
            
            // Fragment Body
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=(textureCube(uEnvmap,vReflection))*uReflectivity; diffuseIsSet=true;} else diffuse *= (textureCube(uEnvmap,vReflection))*uReflectivity;\n";
            fragmentShaderBody+="diffuse=(textureCube(uEnvmap,vReflection));\n";
        }
        if (option & ShaderUsage::Skybox)
        {
            usingTexcoords = true;
            // Vertex Header
            vertexShaderHeader+="varying vec3 v3Texcoord;\n";
            // Vertex Body
            vertexShaderBody+="v3Texcoord = aPosition.xyz;\n";
            
            // Fragment Header
            fragmentShaderHeader+="varying vec3 v3Texcoord;\n";
            fragmentShaderHeader+="uniform samplerCube uSkybox;\n";
            // Fragment Body
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=textureCube(uSkybox,v3Texcoord); diffuseIsSet=true;} else diffuse *= textureCube(uSkybox,v3Texcoord);\n";
        }
        if (option & ShaderUsage::Refraction)
        {
            if (!usingCameraPosition)
            {
                usingCameraPosition = true;
                // Vertex Header
                vertexShaderHeader+="uniform vec3 uCameraPos;\n";
                vertexShaderHeader+="varying vec3 vCameraPos;\n";
                vertexShaderBody+="vCameraPos = uCameraPos;\n";
                // Fragment Header
                fragmentShaderHeader+="varying vec3 vCameraPos;\n";
            }
            if (!usingVertexLocalPos)
            {
                usingVertexLocalPos = true;
                vertexShaderHeader+="varying vec4 vLocalPosition;\n";
                vertexShaderBody+="vLocalPosition=uViewMatrix * uModelMatrix * vec4(aPosition,1.0);\n";
                fragmentShaderHeader+="varying vec4 vLocalPosition;\n";
            }
            if (!usingNormal)
            {
                usingNormal = true;
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uViewMatrix * uModelMatrix * vec4(aNormal,0.0)).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = vNormal;\n";
            }
            if (!usingReflect)
            {
                usingReflect = true;
                
                // Vertex Header
                vertexShaderHeader+="varying vec3 vReflection;\n";
                // Vertex Body
                vertexShaderBody+="vReflection = reflect((vLocalPosition.xyz - uCameraPos),vNormal);\n";
                
                // Fragment Header
                fragmentShaderHeader+="varying vec3 vReflection;\n";
            }
            
            vertexShaderHeader+="varying vec3 vTRed, vTGreen, vTBlue;\n";
            vertexShaderHeader+="varying float vReflectionFactor;\n";
            vertexShaderBody+="float fresnelBias, fresnelScale, fresnelPower;\n";
            vertexShaderBody+="vec3 etaRatio;\n";
            vertexShaderBody+="fresnelBias = 0.9; fresnelScale=0.7; fresnelPower=1.0; etaRatio=vec3(0.943,0.949,0.945);\n";
            vertexShaderBody+="vec3 I = normalize(vLocalPosition.xyz - uCameraPos);\n";
            vertexShaderBody+="vTRed = refract(I,vNormal,etaRatio.x);vTGreen =  refract(I,vNormal,etaRatio.y);vTBlue = refract(I,vNormal,etaRatio.z);\n";
            vertexShaderBody+="vReflectionFactor = fresnelBias + fresnelScale * pow(1.0+dot(I,vNormal),fresnelPower);";
            
            fragmentShaderHeader+="uniform samplerCube uRefractmap;\n";
            fragmentShaderHeader+="varying float vReflectionFactor;\n";
            fragmentShaderHeader+="varying vec3 vTRed, vTGreen, vTBlue;\n";
            fragmentShaderBody+="vec4 reflectedColor = textureCube( uRefractmap, vReflection);\n";
            fragmentShaderBody+="vec4 refractedColor;\n";
            fragmentShaderBody+="refractedColor.x = (textureCube( uRefractmap, vTRed)).x;\n";
            fragmentShaderBody+="refractedColor.y = (textureCube( uRefractmap, vTGreen)).y;\n";
            fragmentShaderBody+="refractedColor.z = (textureCube( uRefractmap, vTBlue)).z;\n";
            fragmentShaderBody+="refractedColor.w = 1.0;\n";
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=mix(reflectedColor, refractedColor, vReflectionFactor); diffuseIsSet=true;} else diffuse *= mix(reflectedColor, refractedColor, vReflectionFactor);\n";
        }
        if (option & ShaderUsage::SpecularColor)
        {
            fragmentShaderHeader+="uniform vec4 uSpecular;\n";
            fragmentShaderBody+="specular = uSpecular;\n";
        }
        if (option & ShaderUsage::SpecularMap)
        {
            if (!usingTexcoords)
            {
                usingTexcoords = true;
                // Vertex Header
                vertexShaderHeader+="varying vec2 vTexcoord;\n";
                // Vertex Body
                vertexShaderBody+="vTexcoord = aTexcoord;\n";
                // Fragment Header
                fragmentShaderHeader+="varying vec2 vTexcoord;\n";
                fragmentShaderHeader+="vec2 Texcoord = vTexcoord;\n";
            }
            fragmentShaderHeader+="uniform sampler2D uSpecularmap;\n";
            fragmentShaderBody+="specular = texture2D(uSpecularmap,Texcoord);\n";
        }
        if (option & ShaderUsage::Diffuse || option & ShaderUsage::CellShading)
        {
            if (!usingNormal)
            {
                usingNormal = true;
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uViewMatrix * uModelMatrix * vec4(aNormal,0)).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = vNormal;\n";
            }
            
            if (!usingVertexLocalPos)
            {
                usingVertexLocalPos = true;
                vertexShaderHeader+="varying vec4 vLocalPosition;\n";
                vertexShaderBody+="vLocalPosition=uViewMatrix * uModelMatrix * vec4(aPosition,1.0);\n";
                fragmentShaderHeader+="varying vec4 vLocalPosition;\n";
            }
            
            // Fragment Header
            
            // Directional Light
            fragmentShaderHeader+="struct DirectionalLight {\n";
            fragmentShaderHeader+="vec4 Color;\n";
            fragmentShaderHeader+="vec3 Direction;\n";
            fragmentShaderHeader+="};\n";
            fragmentShaderHeader+="void buildDirectionalLightFromMatrix(mat4 Light, inout DirectionalLight L) {\n";
            fragmentShaderHeader+="L.Color = Light[0];\n";
            fragmentShaderHeader+="L.Direction = vec3(Light[1][3],Light[2][0],Light[2][1]);\n";
            fragmentShaderHeader+="}\n";
            
            // Point Light
            fragmentShaderHeader+="struct PointLight {\n";
            fragmentShaderHeader+="vec4 Color;\n";
            fragmentShaderHeader+="vec3 Position;\n";
            fragmentShaderHeader+="float Radius;\n";
            fragmentShaderHeader+="};\n";
            fragmentShaderHeader+="void buildPointLightFromMatrix(mat4 Light, inout PointLight L) {\n";
            fragmentShaderHeader+="L.Color = Light[0];\n";
            fragmentShaderHeader+="L.Position = vec3(Light[1][0],Light[1][1],Light[1][2]);\n";
            fragmentShaderHeader+="L.Radius = Light[2][2];\n";
            fragmentShaderHeader+="}\n";
            
            // Spot Light
            fragmentShaderHeader+="struct SpotLight {\n";
            fragmentShaderHeader+="vec4 Color;\n";
            fragmentShaderHeader+="vec3 Direction;\n";
            fragmentShaderHeader+="vec3 Position;\n";
            fragmentShaderHeader+="float Radius;\n";
            fragmentShaderHeader+="vec2 Cones;\n";
            fragmentShaderHeader+="};\n";
            fragmentShaderHeader+="void buildSpotLightFromMatrix(mat4 Light, inout SpotLight L) {\n";
            fragmentShaderHeader+="L.Color = Light[0];\n";
            fragmentShaderHeader+="L.Position = vec3(Light[1][0],Light[1][1],Light[1][2]);\n";;
            fragmentShaderHeader+="L.Direction = vec3(Light[1][3],Light[2][0],Light[2][1]);\n";
            fragmentShaderHeader+="L.Radius = Light[2][2];\n";
            fragmentShaderHeader+="L.Cones = vec2(Light[3][1],Light[3][2]);\n";
            fragmentShaderHeader+="}\n";
            
            // Number Of Lights
            fragmentShaderHeader+="const int MAX_LIGHTS = 4;\n";
            
            // Attenuation Calculations
            fragmentShaderHeader+="float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius)\n";
            fragmentShaderHeader+="{\n";
            fragmentShaderHeader+="float uRadius = 1.0;\n";
            fragmentShaderHeader+="float d = distance(Vertex,LightPosition);\n";
            fragmentShaderHeader+="return clamp(1.0 - (1.0/Radius) * sqrt(d*d), 0.0, 1.0);\n";
            fragmentShaderHeader+="}\n";
            
            // SpotLight Cones
            fragmentShaderHeader+="float DualConeSpotLight(vec3 Vertex, vec3 SpotLightPosition, vec3 SpotLightDirection, float cosOutterCone, float cosInnerCone)\n";
            fragmentShaderHeader+="{\n";
            fragmentShaderHeader+="vec3 to_light = normalize(SpotLightPosition-Vertex);\n";
            fragmentShaderHeader+="float angle = dot(-to_light, normalize(SpotLightDirection));\n";
            fragmentShaderHeader+="float funcX = 1.0/(cosInnerCone-cosOutterCone);\n";
            fragmentShaderHeader+="float funcY = -funcX * cosOutterCone;\n";
            fragmentShaderHeader+="return clamp(angle*funcX+funcY,0.0,1.0);\n";
            fragmentShaderHeader+="}\n";
              
            // Uniforms
            fragmentShaderHeader+="uniform mat4 uLights[MAX_LIGHTS];\n";
            fragmentShaderHeader+="uniform int uNumberOfLights;\n";
            fragmentShaderHeader+="uniform vec4 uAmbientLight;\n";
            fragmentShaderHeader+="uniform vec4 uKa, uKe, uKd, uKs;\n";
            fragmentShaderHeader+="uniform float uShininess;\n";
            fragmentShaderHeader+="uniform float uUseLights;\n";
            
            if (!usingTangentMatrix)
                fragmentShaderHeader+="mat3 TangentMatrix = mat3(1.0);\n";
            
            // Fragment Body
            fragmentShaderBody+="vec4 _diffuse = vec4(0.0,0.0,0.0,1.0);\n";
            fragmentShaderBody+="vec4 _specular = vec4(0.0,0.0,0.0,1.0);\n";
            fragmentShaderBody+="vec4 finalColor = vec4(0.0,0.0,0.0,1.0);\n";
            
            fragmentShaderBody+="float _intensity = 0.0;\n";
            
            fragmentShaderBody+="if (uUseLights!=0.0) {\n";
            
            fragmentShaderBody+="vec3 Vertex = TangentMatrix * vLocalPosition.xyz;\n";
            fragmentShaderBody+="vec3 EyeVec = TangentMatrix * normalize(-Vertex);\n";
            
            fragmentShaderBody+="for (int i=0;i<MAX_LIGHTS;i++)\n";
            fragmentShaderBody+="{\n";
            fragmentShaderBody+="float attenuation = 1.0;\n";
            fragmentShaderBody+="float spotEffect = 1.0;\n";
            fragmentShaderBody+="if (i<uNumberOfLights) {\n";
            fragmentShaderBody+="mat4 Light = uLights[i];\n";
            
            fragmentShaderBody+="vec3 LightDir;\n";
            fragmentShaderBody+="vec4 LightColor;\n";
            
            // Directional Lights
            fragmentShaderBody+="if (Light[3][3]==1.0) {\n";
            fragmentShaderBody+="DirectionalLight L;\n";
            fragmentShaderBody+="buildDirectionalLightFromMatrix(Light,L);\n";
            fragmentShaderBody+="LightDir = normalize(L.Direction);\n";
            fragmentShaderBody+="LightColor = L.Color;\n";
            fragmentShaderBody+="}\n";
            // Point Lights
            fragmentShaderBody+="if (Light[3][3]==2.0) {\n";
            fragmentShaderBody+="PointLight L;\n";
            fragmentShaderBody+="buildPointLightFromMatrix(Light,L);\n";
            fragmentShaderBody+="LightDir = normalize(L.Position - Vertex);\n";
            fragmentShaderBody+="LightColor = L.Color;\n";
            fragmentShaderBody+="attenuation = Attenuation(Vertex, L.Position, L.Radius);\n";
            fragmentShaderBody+="}\n";
            // Spot Lights
            fragmentShaderBody+="if (Light[3][3]==3.0) {\n";
            fragmentShaderBody+="SpotLight L;\n";
            fragmentShaderBody+="buildSpotLightFromMatrix(Light,L);\n";
            fragmentShaderBody+="LightDir = normalize(L.Position - Vertex);\n";
            fragmentShaderBody+="LightColor = L.Color;\n";
            fragmentShaderBody+="attenuation = Attenuation(Vertex, L.Position, L.Radius);\n";
            fragmentShaderBody+="spotEffect = 1.0 - DualConeSpotLight(Vertex, L.Position, L.Direction, L.Cones.x, L.Cones.y);\n";
            fragmentShaderBody+="}\n";
            
            fragmentShaderBody+="vec3 HalfVec = TangentMatrix * normalize(EyeVec + LightDir);\n";
            fragmentShaderBody+="vec3 LightVec = TangentMatrix * LightDir;\n";
            
            fragmentShaderBody+="float specularLight = 0.0;\n";
            fragmentShaderBody+="float diffuseLight = max(dot(LightVec,Normal),0.0);\n";
            fragmentShaderBody+="_intensity += max(dot(LightVec,Normal),0.0);\n";
            
            if (option & ShaderUsage::CellShading)
            {
                fragmentShaderBody+="float factor = 3.0;\n";
                fragmentShaderBody+="if (_intensity > 0.95) factor = 3.0;\n";
                fragmentShaderBody+="else if (_intensity > 0.7) factor = 2.0;\n";
                fragmentShaderBody+="else if (_intensity > 0.5) factor = 1.0;\n";
                fragmentShaderBody+="else if (_intensity > 0.25) factor = 0.8;\n";
                fragmentShaderBody+="else factor = 0.5;\n";
                fragmentShaderBody+="diffuse.xyz = factor * vec3((diffuse * (uKe + (uAmbientLight * uKa))) + (diffuse * (_diffuse * uKd)));\n";
            } else {
                fragmentShaderBody+="if (diffuseLight>0.0) specularLight = pow(max(dot(HalfVec,Normal),0.0),uShininess);\n";
                fragmentShaderBody+="_diffuse += LightColor * diffuseLight * attenuation * spotEffect;\n";
                fragmentShaderBody+="_specular += LightColor * specularLight * attenuation * spotEffect;\n";
                fragmentShaderBody+="finalColor.xyz += vec3((diffuse * (_diffuse * uKd)) + (specular *(_specular * uKs)));\n";
            }
            fragmentShaderBody+="}\n";
            fragmentShaderBody+="}\n";
            fragmentShaderBody+="}\n";
            fragmentShaderBody+="diffuse.xyz = (diffuse * (uKe + (uAmbientLight * uKa))).xyz + finalColor.xyz;\n";
        }
        // Closing Shaders
        vertexShaderBody+="}\n";
        
        fragmentShaderBody+="gl_FragColor=diffuse*uOpacity;\n";
        fragmentShaderBody+="}\n";
        
        vertex = vertexShaderHeader; vertex+=vertexShaderBody;
        fragment = fragmentShaderHeader; fragment+=fragmentShaderBody;
        
        shader->vertexShader->loadShaderText(vertex);
        shader->fragmentShader->loadShaderText(fragment);
        
        shader->vertexShader->compileShader(&shader->shaderProgram);
        shader->fragmentShader->compileShader(&shader->shaderProgram);
    }

}