//============================================================================
// Name        : SSaoEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>

namespace p3d {

    SSAOEffect::SSAOEffect(const uint32& Tex1) : IEffect()
    {
        
        // Set RTT
        UseRTT(Tex1);
        
        // Use Sample
		rnm = new Texture();
        rnm->LoadTexture("rnm.png", TextureType::Texture, false);
        UseCustomTexture(rnm);

		// Create Fragment Shader
        FragmentShaderString =
                                                "uniform sampler2D uTex0;\n"
                                                "uniform sampler2D uTex1;\n"
                                                "uniform vec2 uNearFar;\n"
                                                "uniform vec2 uScreen;\n"
                                                "uniform float uStrength;\n"
                                                "uniform float uBase;\n"
                                                "uniform float uArea;\n"
                                                "uniform int uSamples;\n"
                                                "uniform float uFalloff;\n"
                                                "uniform float uRadius;\n"
                                                "uniform float uScale;\n"
                                                "varying vec2 vTexcoord;\n"
                                                "//Linear convertions z_info_local.w = vec4( znear, zfar, znear * zfar, znear - zfar );\n"
                                                "vec4 z_info_local = vec4(uNearFar.x,uNearFar.y,uNearFar.x*uNearFar.y,uNearFar.x-uNearFar.y);\n"
                                                //"vec4 z_info_local = vec4(1,500,500,1-500);\n"
                                                "float DecodeLinearDepth(float z, vec4 z_info_local)\n"
                                                "{\n"
                                                    "return z_info_local.x - z * z_info_local.w;\n"
                                                "}\n"
                                                "float DecodeNativeDepth(float native_z, vec4 z_info_local)\n"
                                                "{\n"
                                                "return native_z;"
                                                    "return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);\n"
                                                "}\n"
                                                "vec3 normal_from_depth(float depth, vec2 texcoords) {\n"
                                                    "vec2 offset1 = vec2(0.0,1.0/uScreen.y);\n"
                                                    "vec2 offset2 = vec2(1.0/uScreen.x,0.0);\n"
                                                    //"vec2 offset1 = vec2(0.0,1.0/768);\n"
                                                    //"vec2 offset2 = vec2(1.0/1024,0.0);\n"
                                                    "float scale = uScale;\n"
                                                    //"float scale = 50;\n"
                                                    "float depth1 = DecodeNativeDepth(texture2D(uTex0, texcoords + offset1).r,z_info_local);\n"
                                                    "float depth2 = DecodeNativeDepth(texture2D(uTex0, texcoords + offset2).r,z_info_local);\n"
                                                    "vec3 p1 = vec3(0,1,(depth1 - depth) * scale);\n"
                                                    "vec3 p2 = vec3(1,0,(depth2 - depth) * scale);\n"
                                                    "vec3 normal = cross(p1, p2);\n"
                                                    //"normal.z = -normal.z;\n"
                                                    "return normalize(normal);\n"
                                                "}\n"
                                                 "void main() {\n"

                                                "float total_strength = uStrength;\n"
                                                "float base = uBase;\n"
                                                "float area = uArea;\n"
                                                "float falloff = uFalloff;\n"
                                                "float radius = uRadius;\n"
                                                "int samples = uSamples;\n"
                                                "float samplesf = float(uSamples);\n"

                                                // "float total_strength = 1.0;\n"
                                                // "float base = 0.2;\n"
                                                // "float area = 0.0075;\n"
                                                // "float falloff = 0.000001;\n"
                                                // "float radius = 0.002;\n"
                                                // "int samples = 16;\n"
                                                // "float samplesf = float(samples);\n"

                                                "vec3 sample_sphere[16];\n"
                                                "sample_sphere[0] = vec3( 0.5381, 0.1856,-0.4319);\n"
                                                "sample_sphere[1] = vec3( 0.1379, 0.2486, 0.4430);\n"
                                                "sample_sphere[2] = vec3( 0.3371, 0.5679,-0.0057);\n"
                                                "sample_sphere[3] = vec3(-0.6999,-0.0451,-0.0019);\n"
                                                "sample_sphere[4] = vec3( 0.0689,-0.1598,-0.8547);\n"
                                                "sample_sphere[5] = vec3( 0.0560, 0.0069,-0.1843);\n"
                                                "sample_sphere[6] = vec3(-0.0146, 0.1402, 0.0762);\n"
                                                "sample_sphere[7] = vec3( 0.0100,-0.1924,-0.0344);\n"
                                                "sample_sphere[8] = vec3(-0.3577,-0.5301,-0.4358);\n"
                                                "sample_sphere[9] = vec3(-0.3169, 0.1063, 0.0158);\n"
                                                "sample_sphere[10] = vec3( 0.0103,-0.5869, 0.0046);\n"
                                                "sample_sphere[11] = vec3(-0.0897,-0.4940, 0.3287);\n"
                                                "sample_sphere[12] = vec3( 0.7119,-0.0154,-0.0918);\n"
                                                "sample_sphere[13] = vec3(-0.0533, 0.0596,-0.5411);\n"
                                                "sample_sphere[14] = vec3( 0.0352,-0.0631, 0.5460);\n"
                                                "sample_sphere[15] = vec3(-0.4776, 0.2847,-0.0271);\n"
                                                "vec3 random = normalize( texture2D(uTex1, vTexcoord * 4.0).rgb );\n"
                                                "float depth = DecodeNativeDepth(texture2D(uTex0, vTexcoord).r,z_info_local);\n"
                                                "vec3 position = vec3(vTexcoord, depth);\n"
                                                "vec3 normal = normal_from_depth(depth, vTexcoord);\n"
                                                "float radius_depth = radius/depth;\n"
                                                "float occlusion = 0.0;\n"
                                                "for(int i=0; i < samples; i++) {\n"
                                                    "vec3 ray = radius_depth * reflect(sample_sphere[i], random);\n"
                                                    "vec3 hemi_ray = position + sign(dot(ray,normal)) * ray;\n"
                                                    "float occ_depth = DecodeNativeDepth(texture2D(uTex0, vec2(clamp(hemi_ray.x,0.0,1.0),clamp(hemi_ray.y,0.0,1.0))).r,z_info_local);\n"
                                                    "float difference = depth - occ_depth;\n"
                                                    "occlusion += step(falloff, difference) * (1.0-smoothstep(falloff, area, difference));\n"
                                                "}\n"
                                                "float ao = 1.0 - total_strength * occlusion * (1.0 / samplesf);\n"
                                                "gl_FragColor = vec4(clamp(ao + base,0.0,1.0));\n"
                                                "}";
              
        CompileShaders();

        Uniform::Uniform strength;
        Uniform::Uniform Base;
        Uniform::Uniform Area;
        Uniform::Uniform Falloff;
        Uniform::Uniform Radius;
        Uniform::Uniform Samples;
        Uniform::Uniform Scale;
        Uniform::Uniform nearFarPlane;
        Uniform::Uniform screen;

        total_strength = 1.0;
        base = 0.2;
        area = 0.0075;
        falloff = 0.00001;
        radius = 0.5;
        samples = 16;
        scale = 50.f;

        strength.Name = "uStrength";
        strength.Type = Uniform::DataType::Float;
        strength.Usage = Uniform::PostEffects::Other;
        strength.SetValue(&total_strength);
        AddUniform(strength);

        Base.Name = "uBase";
        Base.Type = Uniform::DataType::Float;
        Base.Usage = Uniform::PostEffects::Other;
        Base.SetValue(&base);
        AddUniform(Base);

        Area.Name = "uArea";
        Area.Type = Uniform::DataType::Float;
        Area.Usage = Uniform::PostEffects::Other;
        Area.SetValue(&area);
        AddUniform(Area);

        Falloff.Name = "uFalloff";
        Falloff.Type = Uniform::DataType::Float;
        Falloff.Usage = Uniform::PostEffects::Other;
        Falloff.SetValue(&falloff);
        AddUniform(Falloff);

        Radius.Name = "uRadius";
        Radius.Type = Uniform::DataType::Float;
        Radius.Usage = Uniform::PostEffects::Other;
        Radius.SetValue(&radius);
        AddUniform(Radius);

        Samples.Name = "uSamples";
        Samples.Type = Uniform::DataType::Int;
        Samples.Usage = Uniform::PostEffects::Other;
        Samples.SetValue(&samples);
        AddUniform(Samples);

        Scale.Name = "uScale";
        Scale.Type = Uniform::DataType::Float;
        Scale.Usage = Uniform::PostEffects::Other;
        Scale.SetValue(&scale);
        AddUniform(Scale);

        nearFarPlane.Name = "uNearFar";
        nearFarPlane.Type = Uniform::DataType::Vec2;
        nearFarPlane.Usage = Uniform::PostEffects::NearFarPlane;
        AddUniform(nearFarPlane);

        screen.Name = "uScreen";
        screen.Type = Uniform::DataType::Vec2;
        screen.Usage = Uniform::PostEffects::ScreenDimensions;
        AddUniform(screen);        
    }

    SSAOEffect::~SSAOEffect()
    {
		delete rnm;
    }

};