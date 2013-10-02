//============================================================================
// Name        : ShaderLib.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ShaderLib
//============================================================================

#include "ShaderLib.h"

namespace Fishy3D
{
    
    void ShaderLib::BuildShader(const unsigned& option, Shaders* shader)
    {

        // Shader Strings
        std::string vertex;
        std::string fragment;
        
        // Flags
        bool usingNormal = false;
        bool usingTexcoords = false;
        bool usingVertexWorldPos = false;
        bool usingVertexLocalPos = false;
        bool usingReflect = false;
        bool usingTangentMatrix = false;
        
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
        fragmentShaderHeader+="vec4 diffuse;\n";
        fragmentShaderHeader+="vec4 specular;\n";
        fragmentShaderHeader+="bool diffuseIsSet = false;\n";
        fragmentShaderHeader+="bool specularIsSet = false;\n";
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
        if (option & ShaderUsage::ShadowMaterial)
        {
			// Vertex Header
            vertexShaderHeader+="varying vec4 vPos;\n";
			// Vertex Fragment
            vertexShaderBody+="vPos = uModelMatrix * vec4(aPosition,1.0);\n";
			// Fragment Header
            fragmentShaderHeader+="uniform sampler2D uShadowmaps[4];\n";
            fragmentShaderHeader+="uniform mat4 uDepthsMVP[4];\n";
            fragmentShaderHeader+="uniform vec4 uShadowFar;\n";
            fragmentShaderHeader+="varying vec4 vPos;\n";
			// Fragment Body
            fragmentShaderBody+="vec4 ShadowCoord;\n";
            fragmentShaderBody+="if (gl_FragCoord.z<uShadowFar.x) ShadowCoord = uDepthsMVP[0] * vPos;\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uShadowFar.y) ShadowCoord = uDepthsMVP[1] * vPos;\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uShadowFar.z) ShadowCoord = uDepthsMVP[2] * vPos;\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uShadowFar.w) ShadowCoord = uDepthsMVP[3] * vPos;\n";
            fragmentShaderBody+="float visibility = 1.0;\n";
            fragmentShaderBody+="float shadowSample;\n";
            fragmentShaderBody+="if (gl_FragCoord.z<uShadowFar.x) shadowSample = texture2D( uShadowmaps[0], ShadowCoord.xy).r;\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uShadowFar.y) shadowSample = texture2D( uShadowmaps[1], ShadowCoord.xy).r;\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uShadowFar.z) shadowSample = texture2D( uShadowmaps[2], ShadowCoord.xy).r;\n";
            fragmentShaderBody+="else if (gl_FragCoord.z<uShadowFar.w) shadowSample = texture2D( uShadowmaps[3], ShadowCoord.xy).r;\n";
            fragmentShaderBody+="float diff = shadowSample - ShadowCoord.z+0.001;\n";
            fragmentShaderBody+="float shadowCoef = (diff<0.0?0.5:1.0);\n";
            fragmentShaderBody+="diffuse.xyz*=vec3(shadowCoef);\n";

        }
        if (option & ShaderUsage::CastShadows)
        {
            if (!usingVertexWorldPos)
            {
                usingVertexWorldPos = true;
                // Vertex Header
                vertexShaderHeader+="varying vec4 vPosition;\n";
                // VertexBody
                vertexShaderBody+="vPosition = gl_Position;\n";
                // Fragment Header
                fragmentShaderHeader+="varying vec4 vPosition;\n";
            }
            // Fragment Body
            fragmentShaderBody+="float depth = vPosition.z / vPosition.w;\n";
            fragmentShaderBody+="depth *= 0.5 + 0.5;\n"; //Don't forget to move away from unit cube ([-1,1]) to [0,1] coordinate system
            fragmentShaderBody+="float moment1 = depth;\n";
            fragmentShaderBody+="float moment2 = depth * depth;\n";
            fragmentShaderBody+="float dx = dFdx(depth);\n";
            fragmentShaderBody+="float dy = dFdy(depth);\n";
            fragmentShaderBody+="moment2 += 0.25*(dx*dx+dy*dy);\n";
            fragmentShaderBody+="diffuse=vec4( gl_FragCoord.z,gl_FragCoord.z,gl_FragCoord.z,1.0);\n";
        }
        if (option & ShaderUsage::BumpMapping)
        {
            if (!usingNormal)
            {
                usingNormal = true;
                vertexShaderHeader+="uniform mat4 uNormalMatrix;\n";
                vertexShaderHeader+="vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uNormalMatrix * vec4(aNormal,0)).xyz);\n";
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
                vertexShaderBody+="vec3 Tangent = normalize((uNormalMatrix * vec4(aTangent,0)).xyz);\n";
                vertexShaderBody+="vec3 Binormal = normalize(cross(vNormal,Tangent));\n";
                vertexShaderBody+="vTangentMatrix = mat3(Tangent.x, Binormal.x, vNormal.x,Tangent.y, Binormal.y, vNormal.y,Tangent.z, Binormal.z, vNormal.z);\n";
                fragmentShaderHeader+="varying mat3 vTangentMatrix;\n";
                fragmentShaderBody+="mat3 TangentMatrix = vTangentMatrix;\n";
            }
            
            // Fragment Header
            fragmentShaderHeader+="uniform sampler2D uNormalmap;\n";
            // Fragment Body
            fragmentShaderBody+="vec3 Normal=normalize(texture2D(uNormalmap, Texcoord).xyz * 2.0 - 1.0);\n";
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
                vertexShaderBody+="vNormal = normalize((uNormalMatrix * (matAnimation * vec4(aNormal,0.0))).xyz);\n";
            } else {
                usingNormal = true;
                vertexShaderHeader+="uniform mat4 uNormalMatrix;\n";
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uNormalMatrix * (matAnimation * vec4(aNormal,0.0))).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = normalize(vNormal);\n";
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
                vertexShaderHeader+="uniform mat4 uNormalMatrix;\n";
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uNormalMatrix * vec4(aNormal,0.0)).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = normalize(vNormal);\n";
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
            
            // Fragment Header
            fragmentShaderHeader+="uniform samplerCube uEnvmap;\n";
            fragmentShaderHeader+="uniform float uReflectivity;\n";
            // Fragment Body
            fragmentShaderBody+="if (!diffuseIsSet) {diffuse=(textureCube(uEnvmap,vReflection))*uReflectivity; diffuseIsSet=true;} else diffuse *= (textureCube(uEnvmap,vReflection))*uReflectivity;\n";
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
                vertexShaderHeader+="uniform mat4 uNormalMatrix;\n";
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uNormalMatrix * vec4(aNormal,0.0)).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = normalize(vNormal);\n";
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
            fragmentShaderBody+="if (!specularIsSet) {specular=uSpecular; specularIsSet=true;} else specular *= uSpecular;\n";
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
            fragmentShaderBody+="if (!specularIsSet) {specular=texture2D(uSpecularmap,Texcoord); specularIsSet=true;} else specular *= texture2D(uSpecularmap,Texcoord);\n";
        }
        if (option & ShaderUsage::Diffuse || option & ShaderUsage::CellShading)
        {
            if (!usingNormal)
            {
                usingNormal = true;
                vertexShaderHeader+="uniform mat4 uNormalMatrix;\n";
                vertexShaderHeader+="varying vec3 vNormal;\n";
                vertexShaderBody+="vNormal = normalize((uNormalMatrix * vec4(aNormal,0)).xyz);\n";
                fragmentShaderHeader+="varying vec3 vNormal;\n";
                fragmentShaderBody+="vec3 Normal = normalize(vNormal);\n";
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
            fragmentShaderHeader+="vec3 LightDir = normalize(SpotLightPosition-Vertex);\n";
            fragmentShaderHeader+="float cosDirection = dot(-LightDir, normalize(SpotLightDirection));\n";
            fragmentShaderHeader+="return smoothstep(cosOutterCone, cosInnerCone, cosDirection);\n";
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
            fragmentShaderBody+="vec4 _diffuse;\n";
            fragmentShaderBody+="vec4 _specular;\n";
            fragmentShaderBody+="float _intensity;\n";
            fragmentShaderBody+="if (uUseLights!=0.0) {\n";

            fragmentShaderBody+="vec3 Vertex = vLocalPosition.xyz;\n";
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
            fragmentShaderBody+="spotEffect = DualConeSpotLight(Vertex, L.Position, L.Direction, L.Cones.x, L.Cones.y);\n";
            fragmentShaderBody+="}\n";

            fragmentShaderBody+="Vertex = normalize(Vertex);\n";
            fragmentShaderBody+="vec3 HalfVec = TangentMatrix * normalize(Vertex + LightDir);\n";
            fragmentShaderBody+="vec3 LightVec = TangentMatrix * normalize(LightDir);\n";

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
                fragmentShaderBody+="if (diffuseLight>0.0) specularLight = pow(max(dot(HalfVec, Normal),0.0),uShininess);\n";
                fragmentShaderBody+="_diffuse += LightColor * diffuseLight * attenuation * spotEffect;\n";
                fragmentShaderBody+="_specular += LightColor * specularLight * attenuation * spotEffect;\n";
                fragmentShaderBody+="diffuse.xyz = vec3((diffuse * (uKe + (uAmbientLight * uKa))) + (diffuse * (_diffuse * uKd)) + (specular *(_specular * uKs)));\n";
            }
            fragmentShaderBody+="}\n";
            fragmentShaderBody+="}\n";
            fragmentShaderBody+="}\n";
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