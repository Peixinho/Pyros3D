#define varying_in in
#define varying_smooth_in smooth in
#define varying_out out
#define varying_smooth_out smooth out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif

#define MAX_BONES 60
#define MAX_LIGHTS 4

// Vulkan/SPIR-V requires every attribute/varying to have a static
// layout(location=N), and every UBO/sampler a static layout(binding=N).
// GL needs neither: it matches varyings by name at link time, and UBO/
// sampler bindings are already assigned at runtime via
// glUniformBlockBinding()/glUniform1i (see
// GLRenderDevice::BindUniformBlockIfPresent and IRenderer's
// uniform-sending loops) - so adding these is never required for GL.
// It was tried unconditionally (any GLSL >= 330/420 "should" tolerate the
// syntax) and reverted: macOS's compatibility-profile GL 4.1 driver
// reserves part of its varying-interpolant budget for legacy built-ins
// (gl_TexCoord[8] and friends, since plain "#version 410" with no "core"
// suffix is a compatibility-profile shader) and throws "Implementation
// limit of 128 varying components exceeded" once an explicit VARYING
// location goes much past single digits - reproduced with a single
// declared vec4 varying at location 14 and nothing else. Since GL never
// needs these qualifiers, the safe fix is to only emit them when actually
// compiling for Vulkan (no such legacy tax there), leaving every GL
// profile's generated source byte-for-byte unchanged. Whoever compiles
// this file for Vulkan must #define VULKAN before #define VERTEX/FRAGMENT,
// the same way GL's #version line is prefixed by BuildShaderSource().
// "Loose" non-block uniforms (uModelMatrix, uOpacity, etc.) are still
// rejected by Vulkan/SPIR-V and are deliberately left as-is - turning them
// into descriptor blocks is real design work that belongs with the actual
// VulkanRenderDevice (Phase 3), not a macro bolted onto the shared GL
// shader here.
#if defined(VULKAN)
	#define IO_LOCATION(n) layout(location = n)
	#define UBO_BINDING(n) layout(std140, binding = n)
	// set=1, not the implicit set=0 UBO_BINDING resources land in -
	// VulkanRenderDevice gives every sampler its own descriptor set
	// *per pipeline* (set=1) instead of sharing one per program the way
	// UBOs do (set=0), since textures vary per-material while UBO
	// content doesn't - see the comment on VulkanRenderDevice.h's
	// ProgramRecord::samplerSetLayout for why.
	#define SAMPLER_BINDING(n) layout(set = 1, binding = n)
#else
	#define IO_LOCATION(n)
	#define UBO_BINDING(n) layout(std140)
	#define SAMPLER_BINDING(n)
#endif

// Attribute locations - unique per name, VERTEX-stage input namespace.
// aInstancedTransform is a mat4 and consumes 4 consecutive locations.
#define LOC_aPosition 0
#define LOC_aNormal 1
#define LOC_aTexcoord 2
#define LOC_aColor 3
#define LOC_aSize 4
#define LOC_aTangent 5
#define LOC_aBitangent 6
#define LOC_aBonesID 7
#define LOC_aBonesWeight 8
#define LOC_aInstancedTransform 9
// 9..12 are the transform's four columns - the next free location is 13.
#define LOC_aInstancedColor 13

// Varying locations - unique per name, shared between this file's VERTEX
// (out) and FRAGMENT (in) compilations via these same macros, so the two
// separately-compiled stages always agree (required for SPIR-V/Vulkan,
// which matches VS outputs to FS inputs by location, not name). mat3/mat4
// varyings consume 3/4 consecutive locations each.
#define LOC_vColor 0
#define LOC_vTexcoord 1
#define LOC_vNormal 2
#define LOC_vWorldPositionShadow 3
#define LOC_vWorldPosition 4
#define LOC_vTangentMatrix 5
#define LOC_vCameraPos 8
#define LOC_vTRed 9
#define LOC_vTGreen 10
#define LOC_vTBlue 11
#define LOC_vReflectionFactor 12
#define LOC_v3Texcoord 13
#define LOC_gbuffer_normals 14
#define LOC_vViewMatrix 15
#define LOC_vScreenSpaceWorldPosition 19
#define LOC_vPrvScreenSpaceWorldPosition 20
#define LOC_vClipDist 21
#define LOC_vInstanceColor 22

// Existing UBO/sampler bindings - match the fixed runtime binding points
// IRenderer.cpp already uses via glUniformBlockBinding (see
// IRenderer.cpp's BindUniformBlockIfPresent calls: GlobalMatrices=0,
// LightsBlock=1, DirectionalShadowBlock=2, PointShadowBlock=3,
// SpotShadowBlock=4), so an explicit binding here is redundant-but-
// consistent for GL42+/GL45 and gives Vulkan the same layout. Samplers
// continue from 5; the C++ side always explicitly sets each sampler's
// texture unit via glUniform1i every bind, so a compile-time default here
// is likewise redundant-but-harmless for GL, and gives Vulkan a static
// binding to reflect.
#define BIND_GlobalMatrices 0
#define BIND_LightsBlock 1
#define BIND_DirectionalShadowBlock 2
#define BIND_PointShadowBlock 3
#define BIND_SpotShadowBlock 4
#define BIND_uColormap 5
#define BIND_uFontmap 6
#define BIND_uNormalmap 7
#define BIND_uDisplacementmap 8
#define BIND_uDirectionalShadowMaps 9
#define BIND_uPointShadowMaps 10
#define BIND_uSpotShadowMaps 11
#define BIND_uEnvmap 12
#define BIND_uRefractmap 13
#define BIND_uSkyboxmap 14
#define BIND_uSpecularmap 15
// PBRMap's ORM-style texture (G=roughness, B=metalness, R unused). Lives in
// the same sampler numbering track as the rest of this block (Vulkan set=1,
// separate from the set=0 UBO bindings continuing below at 16) - see
// SAMPLER_BINDING's comment above for why the two tracks don't collide.
#define BIND_uMetallicRoughnessmap 16

// Loose-uniform-turned-UBO bindings. Each block is declared in exactly one
// stage (checked against actual usage: e.g. uCameraPos is only ever read
// in VERTEX - FRAGMENT only sees the interpolated vCameraPos varying), so
// there's no need to worry about a block being declared with a different
// member subset in each stage - same pattern as the existing single-stage
// blocks above (GlobalMatrices is VERTEX-only, LightsBlock/shadow blocks
// are FRAGMENT-only). Blocks that are always relevant regardless of which
// feature flags are active (ObjectMatrixUniforms, MaterialUniforms) are
// declared unconditionally with every member always present - avoids
// combinatorial std140-offset mismatches across this file's many #ifdef
// feature combinations; unused members in an unused code path are legal
// and free in GLSL, same as uOpacity already being unconditional today.
#define BIND_VertexFrameUniforms 16
#define BIND_VelocityFrameUniforms 17
#define BIND_ObjectMatrixUniforms 18
#define BIND_BoneMatrices 19
#define BIND_VelocityObjectUniforms 20
#define BIND_AmbientLightUniforms 21
#define BIND_MaterialUniforms 22
#define BIND_ObjectLightCounts 23

vec4 EncodeFloatRGBA( float v ) {
   vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
   enc = fract(enc);
   enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
   return enc;
}
float DecodeFloatRGBA( vec4 rgba ) {
   return dot( rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0) );
}

#if defined(EMSCRIPTEN)
    #define _highpMat3 highp mat3
    #define _highpMat4 highp mat4
    #define _highpVec3 highp vec3
    #define _highpVec4 highp vec4
#else
    #define _highpMat3 mat3
    #define _highpMat4 mat4
    #define _highpVec3 vec3
    #define _highpVec4 vec4
#endif

_highpMat3 _transpose3(in _highpMat3 inMatrix) {
    _highpVec3 i0 = inMatrix[0];
    _highpVec3 i1 = inMatrix[1];
    _highpVec3 i2 = inMatrix[2];

    _highpMat3 outMatrix = mat3(
                    vec3(i0.x, i1.x, i2.x),
                    vec3(i0.y, i1.y, i2.y),
                    vec3(i0.z, i1.z, i2.z)
                    );

    return outMatrix;
}
_highpMat4 _transpose4(in _highpMat4 inMatrix) {
    _highpVec4 i0 = inMatrix[0];
    _highpVec4 i1 = inMatrix[1];
    _highpVec4 i2 = inMatrix[2];
    _highpVec4 i3 = inMatrix[3];

    _highpMat4 outMatrix = mat4(
                    vec4(i0.x, i1.x, i2.x, i3.x),
                    vec4(i0.y, i1.y, i2.y, i3.y),
                    vec4(i0.z, i1.z, i2.z, i3.z),
                    vec4(i0.w, i1.w, i2.w, i3.w)
                    );

    return outMatrix;
}

#ifdef transpose
    mat3 transpose3(in mat3 m) { return transpose(m); }
    mat4 transpose4(in mat4 m) { return transpose(m); }
#else
    mat3 transpose3(in mat3 m) { return _transpose3(m); }
    mat4 transpose4(in mat4 m) { return _transpose4(m); }
#endif

#ifdef VERTEX

    // Per-vertex tint. The UI batcher bakes each element's tint into its
    // vertices so a run of elements sharing a texture collapses into one
    // draw - the tint cannot stay a per-object uniform if the objects are
    // no longer drawn one at a time. Mutually exclusive with
    // DEBUGRENDERING, which declares the same attribute for itself.
    #if defined(VERTEXCOLOR) && !defined(DEBUGRENDERING)
        IO_LOCATION(LOC_aColor) attribute_in vec4 aColor;
        IO_LOCATION(LOC_vColor) varying_out vec4 vColor;
    #endif

    #ifdef DEBUGRENDERING
        IO_LOCATION(LOC_aColor) attribute_in vec4 aColor;
        // aSize / gl_PointSize omitted: DebugRenderer's point path is
        // disabled, and declaring aSize without a matching vertex-input
        // binding fails CreatePipeline on Vulkan (same rule as aNormal /
        // aTexcoord). PointSize in a LINE_LIST/TRIANGLE_LIST pipeline is
        // also a frequent MoltenVK compile failure.
        IO_LOCATION(LOC_vColor) varying_out vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP) || defined(PBRMAP)
        IO_LOCATION(LOC_vTexcoord) varying_out vec2 vTexcoord;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(TEXTRENDERING) || defined(PBR)
        IO_LOCATION(LOC_vNormal) varying_out vec3 vNormal;
    #endif

    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
        IO_LOCATION(LOC_vWorldPositionShadow) varying_out vec4 vWorldPositionShadow;
    #endif

    #if defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PARALLAXMAPPING) || defined(PBR)
        IO_LOCATION(LOC_vWorldPosition) varying_out vec4 vWorldPosition;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        IO_LOCATION(LOC_aTangent) attribute_in vec3 aTangent;
        IO_LOCATION(LOC_aBitangent) attribute_in vec3 aBitangent;
        IO_LOCATION(LOC_vTangentMatrix) varying_out mat3 vTangentMatrix;
    #endif

    #ifdef SKINNING
        IO_LOCATION(LOC_aBonesID) attribute_in vec4 aBonesID;
        IO_LOCATION(LOC_aBonesWeight) attribute_in vec4 aBonesWeight;
        UBO_BINDING(BIND_BoneMatrices) uniform BoneMatrices {
            mat4 uBoneMatrix[MAX_BONES];
        };
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR) || defined(CLIPSPACE) || defined(VERTEXWIND)
        UBO_BINDING(BIND_VertexFrameUniforms) uniform VertexFrameUniforms {
            // xyz = camera, w = clip enable (1/0). vec4 avoids std140
            // vec3+float packing differences between GL drivers that left
            // the UBO undersized/mismatched and drew black on macOS GL.
            vec4 uCameraPos;
            vec4 uClipPlane0;
            // x = seconds since start, the wind's only time source. Lives
            // here rather than in MaterialUniforms because it is per frame,
            // not per material - see IRenderer's vertexFrameData.
            vec4 uTimeParams;
        };
    #endif
    #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
        IO_LOCATION(LOC_vCameraPos) varying_out vec3 vCameraPos;
    #endif
    #ifdef CLIPSPACE
        IO_LOCATION(LOC_vClipDist) varying_out float vClipDist;
    #endif

    #ifdef REFRACTION
        IO_LOCATION(LOC_vTRed) varying_out vec3 vTRed;
        IO_LOCATION(LOC_vTGreen) varying_out vec3 vTGreen;
        IO_LOCATION(LOC_vTBlue) varying_out vec3 vTBlue;
        IO_LOCATION(LOC_vReflectionFactor) varying_out float vReflectionFactor;
    #endif

    #ifdef SKYBOX
        IO_LOCATION(LOC_v3Texcoord) varying_out vec3 v3Texcoord;
    #endif

    #ifdef DEFERRED_GBUFFER
        IO_LOCATION(LOC_gbuffer_normals) varying_out vec4 gbuffer_normals;
    #endif

   #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
       IO_LOCATION(LOC_vViewMatrix) varying_out mat4 vViewMatrix;
   #endif

    // Defaults
    IO_LOCATION(LOC_aPosition) attribute_in vec3 aPosition;
    // Same Vulkan rule as aTexcoord below: every SPIR-V input must have a
    // matching VkVertexInputAttributeDescription. DebugRenderer only feeds
    // aPosition + aColor (see DebugRenderer::EnsurePipeline), so aNormal
    // must not appear in the DEBUGRENDERING variant.
    #ifndef DEBUGRENDERING
    IO_LOCATION(LOC_aNormal) attribute_in vec3 aNormal;
    #endif
    // Gated the same as its only use (vTexcoord = aTexcoord, below) - GL
    // tolerates an unconditionally-declared-but-unbound vertex attribute
    // (it's simply never sampled), but Vulkan requires every SPIR-V input
    // a compiled shader variant declares to have a matching
    // VkVertexInputAttributeDescription (VUID-VkGraphicsPipelineCreateInfo-Input-07904).
    // A mesh with no texcoord data at all (e.g. a Model asset with no UVs -
    // found via a live Decals/LOD_Example/SkeletonAnimationExample crash,
    // using a Color-only material with no texture-related flag) has no
    // "aTexcoord" entry in its vertex layout to satisfy that requirement
    // when this was unconditional.
    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP) || defined(PBRMAP)
        IO_LOCATION(LOC_aTexcoord) attribute_in vec2 aTexcoord;
    #endif
    // uProjectionMatrix/uViewMatrix change once per (frame or shadow pass),
    // not per object, so they're shared via a UBO instead of being resent
    // as individual uniforms on every draw; uModelMatrix is per-object and
    // stays a plain uniform.
    UBO_BINDING(BIND_GlobalMatrices) uniform GlobalMatrices {
        mat4 uProjectionMatrix;
        mat4 uViewMatrix;
    };
    UBO_BINDING(BIND_ObjectMatrixUniforms) uniform ObjectMatrixUniforms {
        mat4 uModelMatrix;
        // xyz = wind strength / rate / spatial frequency, w unused.
        // Wind is a material property (GenericShaderMaterial::SetWind())
        // but rides this per-object block rather than MaterialUniforms
        // because it is needed in the VERTEX stage and MaterialUniforms is
        // declared only in the fragment one - duplicating that whole block
        // here just to reach three floats would mean keeping two std140
        // layouts in step forever. IRenderer::SendModelUniforms() fills it
        // from whatever material is being drawn, which is also what lets
        // the shadow pass inherit a caster's wind (see
        // PickShadowMaterial()) so blades and their shadows sway together.
        vec4 uWind;
    };

    // Instanced
    #ifdef INSTANCED_RENDERING
        IO_LOCATION(LOC_aInstancedTransform) attribute_in mat4 aInstancedTransform;
        // Per-instance tint. Guarded separately from INSTANCED_RENDERING:
        // the buffer behind it is opt-in
        // (RenderingInstancedComponent::EnableInstanceColors()), and on
        // Vulkan an attribute a shader declares with no matching vertex
        // buffer attribute fails pipeline creation.
        #ifdef INSTANCED_COLOR
            IO_LOCATION(LOC_aInstancedColor) attribute_in vec4 aInstancedColor;
            IO_LOCATION(LOC_vInstanceColor) varying_out vec4 vInstanceColor;
        #endif
    #endif

    #ifdef VELOCITY_RENDERING
        UBO_BINDING(BIND_VelocityFrameUniforms) uniform VelocityFrameUniforms {
            mat4 uPrvProjectionMatrix;
            mat4 uPrvViewMatrix;
        };
        UBO_BINDING(BIND_VelocityObjectUniforms) uniform VelocityObjectUniforms {
            mat4 uPrvModelMatrix;
        };
        IO_LOCATION(LOC_vScreenSpaceWorldPosition) varying_smooth_out vec4 vScreenSpaceWorldPosition;
	IO_LOCATION(LOC_vPrvScreenSpaceWorldPosition) varying_smooth_out vec4 vPrvScreenSpaceWorldPosition;
    #endif

    mat4 matAnimation = mat4(1.0);

    void main() {

        vec3 Position = aPosition;
        mat4 ModelMatrix = uModelMatrix;

        #ifdef INSTANCED_RENDERING
            ModelMatrix *= aInstancedTransform;
            #ifdef INSTANCED_COLOR
                vInstanceColor = aInstancedColor;
            #endif
        #endif

        #ifdef VERTEXWIND
            // Sway anything above the mesh's own origin, leaving what is
            // below it planted - a blade card modelled around its centre
            // therefore bends from the middle up and keeps its base on the
            // ground. Amount is uWind.x, wave rate uWind.y, spatial
            // frequency uWind.z.
            //
            // The phase comes from the *instanced* model matrix's
            // translation, so every blade in a field is offset by where it
            // stands. Without that a chunk of instances sharing one draw
            // call sways in perfect lockstep, which reads as the ground
            // moving rather than the grass.
            if (uWind.x > 0.0)
            {
                float windHeight = max(Position.y, 0.0);
                vec3 windOrigin = vec3(ModelMatrix[3][0], ModelMatrix[3][1], ModelMatrix[3][2]);
                float windPhase = uTimeParams.x * uWind.y + (windOrigin.x + windOrigin.z) * uWind.z;
                // Two waves at an irrational ratio, so the field never
                // visibly resets to a single repeating gust.
                float windSway = sin(windPhase) * 0.75 + sin(windPhase * 2.37 + 1.7) * 0.25;
                Position.x += windSway * windHeight * uWind.x;
                Position.z += windSway * windHeight * uWind.x * 0.35;
            }
        #endif

        #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
            vViewMatrix = uViewMatrix;
        #endif

        #if defined(DEBUGRENDERING) || defined(VERTEXCOLOR)
            vColor = aColor;
        #endif

        #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP) || defined(PBRMAP)
            vTexcoord = aTexcoord;
        #endif

        // Skinning matrix must be built before any use (vWorldPosition /
        // gl_Position / normals). Computing it after those writes left
        // matAnimation uninitialized on some GL drivers → bind-pose mesh.
        #ifdef SKINNING
            matAnimation = uBoneMatrix[int(aBonesID.x)] * aBonesWeight.x;
            matAnimation += uBoneMatrix[int(aBonesID.y)] * aBonesWeight.y;
            matAnimation += uBoneMatrix[int(aBonesID.z)] * aBonesWeight.z;
            matAnimation += uBoneMatrix[int(aBonesID.w)] * aBonesWeight.w;
        #endif

        #if defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) ||  defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
            #ifndef SKINNING
                vWorldPosition=ModelMatrix * vec4(Position,1.0);
            #else
                vWorldPosition=ModelMatrix * (matAnimation * vec4(Position,1.0));
            #endif
        #endif

        #ifdef VELOCITY_RENDERING
                vScreenSpaceWorldPosition=uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(Position,1.0);
                vPrvScreenSpaceWorldPosition=uPrvProjectionMatrix * uPrvViewMatrix * uPrvModelMatrix * vec4(Position,1.0);
        #endif


        #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
            #ifndef SKINNING
                vWorldPositionShadow=uViewMatrix * ModelMatrix * vec4(Position,1.0);
            #else
                vWorldPositionShadow=uViewMatrix * ModelMatrix * (matAnimation * vec4(Position,1.0));
            #endif
        #endif

        #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) ||  defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
            vCameraPos = uCameraPos.xyz;
        #endif

        #ifdef SKYBOX
            v3Texcoord = Position.xyz;
        #endif

        #ifndef SKINNING
            gl_Position = uProjectionMatrix * uViewMatrix * ModelMatrix * vec4(Position,1.0);
        #else
            gl_Position = uProjectionMatrix * uViewMatrix * ModelMatrix * matAnimation * vec4(Position,1.0);
        #endif

        #ifdef TEXTRENDERING
            vNormal = aNormal;
        #endif

        #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
            #ifndef SKINNING
                vNormal = normalize((ModelMatrix * vec4(aNormal,0.0)).xyz);
            #else
                vNormal = normalize((ModelMatrix * (matAnimation * vec4(aNormal,0.0))).xyz);
            #endif
        #endif

        #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
            vec3 T = normalize(ModelMatrix * vec4(aTangent, 0)).xyz;
            vec3 B = normalize(ModelMatrix * vec4(aBitangent, 0)).xyz;
            vec3 N = normalize(ModelMatrix * vec4(aNormal, 0)).xyz;
            vTangentMatrix = mat3(T.x, B.x, N.x, T.y, B.y, N.y, T.z, B.z, N.z);
        #endif

        #ifdef REFRACTION
            float fresnelBias, fresnelScale, fresnelPower;
            vec3 etaRatio;
            fresnelBias = 0.9;
            fresnelScale=0.7;
            fresnelPower=1.0;
            etaRatio=vec3(0.943,0.949,0.945);
            vec3 I = normalize(vWorldPosition.xyz - uCameraPos.xyz);
            vTRed = refract(I,vNormal,etaRatio.x);
            vTGreen =  refract(I,vNormal,etaRatio.y);
            vTBlue = refract(I,vNormal,etaRatio.z);
            vReflectionFactor = fresnelBias + fresnelScale * pow(1.0+dot(I,vNormal),fresnelPower);
        #endif

        #ifdef DEFERRED_GBUFFER
            #ifdef SKINNING
                gbuffer_normals = uViewMatrix * ModelMatrix * (matAnimation * vec4(aNormal,0.0));
            #else
                gbuffer_normals = uViewMatrix * ModelMatrix * vec4(aNormal,0.0);
            #endif
        #endif

        #ifdef CLIPSPACE
            #if defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR) || defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) || defined(SKINNING)
                vClipDist = (uCameraPos.w > 0.5) ? dot(vWorldPosition, uClipPlane0) : 1.0;
            #else
                vClipDist = (uCameraPos.w > 0.5) ? dot(ModelMatrix * vec4(Position,1.0), uClipPlane0) : 1.0;
            #endif
            // Kept for drivers that honor user clip planes when enabled;
            // IRenderDevice::EnableClipDistance is a no-op on both backends
            // today (clip is the fragment discard below), so this write is
            // harmless and keeps the varying consistent if that changes.
            #if !defined(VULKAN) && !defined(GLES3)
                gl_ClipDistance[0] = vClipDist;
            #endif
        #endif
    }

#endif

#ifdef FRAGMENT

	// Fragment Color. When DEFERRED_GBUFFER is also active, FragData_r/g/b/pbr
	// below claim locations 0/1/2/3, so FragColor (unused by the G-buffer
	// pass, but still assigned in main() below) is pinned to location 4
	// instead - this matches what GL's implicit-location auto-assignment
	// already does today when both are declared (it picks a location that
	// doesn't collide with the explicit ones), just made static/explicit
	// so SPIR-V (which requires every output to have one) can compile it.
	// Was location 3 until FragData_pbr (PBR's metallic/roughness G-buffer
	// output) claimed it - real collision risk once a 4th color attachment
	// actually exists at that location, not just a cosmetic renumbering.
	#ifdef VELOCITY_RENDERING
		#ifdef DEFERRED_GBUFFER
			IO_LOCATION(4) out vec4 FragColor;
		#else
			IO_LOCATION(0) out vec4 FragColor;
		#endif
	#else
		#ifdef DEFERRED_GBUFFER
			IO_LOCATION(4) out vec4 FragColor;
		#else
			IO_LOCATION(0) out vec4 FragColor;
		#endif
	#endif

    #if defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)

        struct LIGHT
        {
            vec4 Color;
            vec3 Direction;
            vec3 Position;
            float Radius;
            vec2 Cones;
            float Type;
            bool HaveShadowMap;
            int ShadowMap;
            float PCFTexelSize;
            // Point lights only - PointLight::SetShadowBiasScale(). Shares
            // the slot a spot light uses for Cones.x, which ForwardRenderer
            // writes as zero for a point light and nothing reads there.
            float ShadowBiasScale;
        };
        void buildLightFromMatrix(mat4 Light, inout LIGHT L)
        {
            L.Color = Light[0];
            L.Position = vec3(Light[1][0],Light[1][1],Light[1][2]);;
            L.Direction = vec3(Light[1][3],Light[2][0],Light[2][1]);
            L.Radius = Light[2][2];
            L.Cones = vec2(Light[2][3],Light[3][0]);
            L.ShadowBiasScale = Light[2][3];
            L.Type = Light[3][1];
            L.HaveShadowMap = (Light[3][3]>=0.0? true : false);
            if (L.HaveShadowMap) {
               L.PCFTexelSize = Light[3][2];
               L.ShadowMap = int(Light[3][3]); // Only for Point and Spot Shadows (Directional have only one shadow map)
            }
        }

        float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius)
        {
            if (Radius>0.0) {
            float d = distance(Vertex,LightPosition);
            return clamp(1.0 - (1.0/Radius) * d, 0.0, 1.0);
            };
            return 1.0;
        }

        // SpotLight Cones
        float DualConeSpotLight(vec3 Vertex, vec3 SpotLightPosition, vec3 SpotLightDirection, float cosOutterCone, float cosInnerCone)
        {
            if (cosOutterCone>0.0 || cosInnerCone>0.0) {
                vec3 to_light = normalize(SpotLightPosition-Vertex);
                float angle = dot(-to_light, normalize(SpotLightDirection));
                float funcX = 1.0/(cosInnerCone-cosOutterCone);
                float funcY = -funcX * cosOutterCone;
                return clamp(angle*funcX+funcY,0.0,1.0);
            }
            return 0.0;
        }

        void CalculateLighting(vec3 LightVec, vec3 HalfVec, vec3 Normal, float Shininess, out float lightIntensity, out float specularPower)
        {
            #ifdef LIGHTING2D
                // 2D lighting: no N.L, no specular. A sprite is a flat quad
                // with one normal facing the camera, so N.L is degenerate for
                // it - a light in the sprite's own plane, which is exactly
                // where 2D authoring puts one, is at grazing incidence and
                // leaves it unlit. Measured: a PointLight at a quad's own z
                // rendered it black.
                //
                // Dropping the term here rather than in each light branch
                // means every light type inherits it: a point light becomes
                // pure radial falloff (its Attenuation() is applied by the
                // caller and is untouched), a spot keeps its cone, and a
                // directional becomes the flat wash a 2D "sun" should be.
                // Specular needs a real surface orientation to mean anything,
                // so it goes to zero rather than to some arbitrary constant.
                lightIntensity = 1.0;
                specularPower = 0.0;
            #else
                float specularLight = 0.0;
                float diffuseLight = max(dot(LightVec,Normal),0.0);
                lightIntensity = max(dot(LightVec,Normal),0.0);
                specularPower = (lightIntensity>0.0?pow(max(dot(HalfVec,Normal),0.0), Shininess):0.0);
            #endif
        }

        #ifdef PBR
            const float PBR_PI = 3.14159265359;

            float DistributionGGX(vec3 N, vec3 H, float roughness)
            {
                float a = roughness * roughness;
                float a2 = a * a;
                float NdotH = max(dot(N, H), 0.0);
                float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
                return a2 / max(PBR_PI * denom * denom, 1e-6);
            }

            float GeometrySchlickGGX(float NdotV, float roughness)
            {
                float r = (roughness + 1.0);
                float k = (r * r) / 8.0;
                return NdotV / (NdotV * (1.0 - k) + k);
            }

            float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
            {
                float NdotV = max(dot(N, V), 0.0);
                float NdotL = max(dot(N, L), 0.0);
                return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
            }

            vec3 FresnelSchlick(float cosTheta, vec3 F0)
            {
                return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
            }

            // One light's outgoing radiance contribution, Cook-Torrance
            // specular (GGX distribution + Smith geometry + Schlick Fresnel)
            // plus energy-conserving Lambertian diffuse (kD scaled by
            // (1-metallic), since metals have no diffuse response). Mirrors
            // CalculateLighting()'s call-site shape (N/V/L already available
            // as Normal/EyeVec/LightVec in every light-type branch below) so
            // it drops into the existing per-light loop unchanged.
            vec3 CalculatePBRLighting(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness)
            {
                vec3 H = normalize(V + L);
                vec3 F0 = mix(vec3(0.04), albedo, metallic);

                float NDF = DistributionGGX(N, H, roughness);
                float G = GeometrySmith(N, V, L, roughness);
                vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 numerator = NDF * G * F;
                float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4;
                vec3 specularTerm = numerator / denom;

                vec3 kS = F;
                vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

                // The engine's light colour is not irradiance. Forward's
                // classic Blinn-Phong path defines it as "the radiance a
                // white Lambertian surface reflects at N.L == 1" - see
                // `_diffuse += lightIntensity * L.Color` below, which has no
                // 1/PI anywhere - so a light colour of 1.0 already means
                // irradiance/PI. Feeding that straight into a normalised
                // BRDF, which divides by PI a second time, made every PBR
                // and every deferred surface exactly PI (3.14x) darker than
                // the identical material under the identical light in the
                // classic forward path: measured 41/255 vs 128/255 on a
                // 0.5-albedo wall under one intensity-1 white directional
                // light. Recovering the irradiance here rather than just
                // dropping the diffuse 1/PI keeps the BRDF internally
                // consistent - the specular lobe scales with the diffuse one
                // instead of ending up PI times weaker relative to it.
                vec3 irradiance = radiance * PBR_PI;

                float NdotL = max(dot(N, L), 0.0);
                return (kD * albedo / PBR_PI + specularTerm) * irradiance * NdotL;
            }
        #endif

        // uLights is rebuilt every object (each object only gets its
        // nearby lights), unlike uProjectionMatrix/uViewMatrix above, but
        // it's still shared across every fragment of that object's draw
        // call, so a UBO still saves resending the whole array as
        // individual uniforms across every shader/material switch.
        UBO_BINDING(BIND_LightsBlock) uniform LightsBlock {
            mat4 uLights[MAX_LIGHTS];
        };

    #endif

    // Defaults
    vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
    vec4 specular = vec4(0.0,0.0,0.0,1.0);
    bool diffuseIsSet = false;
    // Every material-scalar/vector uniform PyrosShader.glsl ever needs, in
    // one always-declared block (regardless of which feature flags are
    // active - see the comment on the BIND_* macros above for why). Split
    // from ObjectLightCounts below: these fields are genuinely tied to the
    // Material object (same value for every object drawn with the same
    // material), so IRenderer only re-uploads this block when the active
    // material actually changes - unlike uNumberOfLights/etc, which differ
    // per *object* (each object gets its own nearby-lights count) even
    // when consecutive objects share one material, and so cannot be gated
    // the same way without risking stale/wrong per-object light counts.
    UBO_BINDING(BIND_MaterialUniforms) uniform MaterialUniforms {
        vec4 uColor;
        vec4 uSpecular;
        float uOpacity;
        float uShininess;
        float uUseLights;
        float uDisplacementHeight;
        float uReflectivity;
        // uMetallic/uRoughness (PBR) occupy 2 of this block's 3 previously-
        // spare std140 padding floats - total block size stays 64 bytes,
        // BIND_MaterialUniforms stays 22. See IRenderer.cpp's
        // MaterialUniformsData for the byte-identical C++ mirror.
        float uMetallic;
        float uRoughness;
        // Real per-material SSR opt-in (GenericShaderMaterial::SetSSREnabled())
        // - not uReflectivity above (an unrelated, older env-map/skybox
        // reflection blend amount). Fills this block's last spare std140
        // padding float - block size stays 64 bytes, no layout change
        // needed elsewhere. Written into the G-buffer's metallicRoughness
        // blue channel below (see FragData_pbr) and read back by
        // DeferredRenderer's lastPass.glsl to gate SSR per-pixel.
        float uSSRReflective;
        // Alpha cutoff (ShaderUsage::AlphaTest). Grows this block past its
        // previously-exact 64 bytes to 80 - std140 rounds a block up to a
        // multiple of 16, so 9 floats after two vec4s lands at 80, not 68.
        // IRenderer's MaterialUniformsData mirror and its
        // CreateUniformBuffer() size both move with it; the static_assert
        // there is what catches the two drifting apart.
        float uAlphaCutoff;
    };
    // Per-object (not per-material) - see the comment above.
    UBO_BINDING(BIND_ObjectLightCounts) uniform ObjectLightCounts {
        int uNumberOfLights;
        int uNumberOfPointShadows;
        int uNumberOfSpotShadows;
    };

    #if defined(INSTANCED_RENDERING) && defined(INSTANCED_COLOR)
        IO_LOCATION(LOC_vInstanceColor) varying_in vec4 vInstanceColor;
    #endif
    #if defined(VERTEXCOLOR) && !defined(DEBUGRENDERING)
        IO_LOCATION(LOC_vColor) varying_in vec4 vColor;
    #endif

    #ifdef DEBUGRENDERING
        IO_LOCATION(LOC_vColor) varying_in vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(SPECULARMAP) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(PBRMAP)
        IO_LOCATION(LOC_vTexcoord) varying_in vec2 vTexcoord;
    #endif

    #ifdef TEXTURE
        SAMPLER_BINDING(BIND_uColormap) uniform sampler2D uColormap;
    #endif

    #ifdef TEXTRENDERING
        SAMPLER_BINDING(BIND_uFontmap) uniform sampler2D uFontmap;
    #endif

    #if defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
        IO_LOCATION(LOC_vNormal) varying_in vec3 vNormal;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        IO_LOCATION(LOC_vTangentMatrix) varying_in mat3 vTangentMatrix;
        #if defined(BUMPMAPPING)
            SAMPLER_BINDING(BIND_uNormalmap) uniform sampler2D uNormalmap;
        #endif
        #if defined(PARALLAXMAPPING)
            SAMPLER_BINDING(BIND_uDisplacementmap) uniform sampler2D uDisplacementmap;

            vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
            {
               const float minLayers = 8.0;
               const float maxLayers = 32.0;
               float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));

               float layerDepth = 1.0 / numLayers;
               float currentLayerDepth = 0.0;
               vec2 P = viewDir.xy / viewDir.z * uDisplacementHeight;
               vec2 deltaTexCoords = P / numLayers;

               vec2  currentTexCoords = texCoords;
               float currentDepthMapValue = texture_2D(uDisplacementmap, currentTexCoords).r;

               while (currentLayerDepth < currentDepthMapValue)
               {
                   currentTexCoords -= deltaTexCoords;
                   currentDepthMapValue = texture_2D(uDisplacementmap, currentTexCoords).r;
                   currentLayerDepth += layerDepth;
               }

               vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

               float afterDepth = currentDepthMapValue - currentLayerDepth;
               float beforeDepth = texture_2D(uDisplacementmap, prevTexCoords).r - currentLayerDepth + layerDepth;

               float weight = afterDepth / (afterDepth - beforeDepth);
               vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

               return finalTexCoords;
            }
        #endif
    #endif

#if defined(DIRECTIONALSHADOW)
            float PCFDIRECTIONAL(sampler2DShadow shadowMap, float width, float height, mat4 sMatrix, float scale, vec4 pos, bool MoreThanOneCascade)
            {
                vec4 coord = sMatrix * pos;
                if (MoreThanOneCascade) coord.xy = (coord.xy * 0.5) + vec2(width,height);
                float shadow = 0.0;
                float x = 0.0;
                float y = 0.0;
                for (y = -1.5 ; y <=1.5 ; y+=1.0)
                    for (x = -1.5 ; x <=1.5 ; x+=1.0)
                        shadow += texture(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0)));
                shadow /= 16.0;
                return shadow;
            }
            // Same batching idea as GlobalMatrices/LightsBlock, for the
            // (up to 4) cascade matrices; uDirectionalShadowFar keeps its
            // declared size but only element [0] is ever written or read -
            // it's a single vec4 whose 4 components are the per-cascade
            // far distances, not a real 4-element array.
            UBO_BINDING(BIND_DirectionalShadowBlock) uniform DirectionalShadowBlock {
                mat4 uDirectionalDepthsMVP[4];
                vec4 uDirectionalShadowFar[4];
            };
            SAMPLER_BINDING(BIND_uDirectionalShadowMaps) uniform sampler2DShadow uDirectionalShadowMaps;
        #endif

        #ifdef POINTSHADOW
            // See secondpassPoint.glsl's PCFPOINT for why this is a plain
            // samplerCube compared by hand rather than hardware PCF.
            //
            // `bias` is needed for the same reason: a point light's cube map
            // is an R32F *colour* attachment, so the polygon offset
            // ILightComponent::SetShadowBias configures never reaches it and
            // a lit surface sits one rounding error from shadowing itself.
            // This copy had none at all, which showed as horizontal stripes
            // across a face pointed straight at the light - striped on Vulkan
            // and Metal, clean on GL, purely because the two sides of the
            // comparison round the same tie differently per backend. See
            // PointLight::SetShadowBiasScale() for what the value means.
            float PCFPOINT(samplerCube shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, float bias, vec4 pos)
            {
                vec4 position_ls = Matrix2 * pos;
                position_ls.xyz/=position_ls.w;
                vec4 abs_position = abs(position_ls);
                float fs_z = -max(abs_position.x, max(abs_position.y, abs_position.z));
                // See secondpassPoint.glsl's identical line - fs_z is
                // negative, so this moves the reference `bias` of the way
                // toward the light, before the projection rather than after.
                fs_z *= (1.0 - bias);
                vec4 clip = Matrix1 * vec4(0.0, 0.0, fs_z, 1.0);
                // Matrix1 (uPointDepthsMVP, IRenderer.cpp) already includes
                // the device's own shadow-bias remap (device->
                // TranslateShadowBiasMatrix() * TranslateProjectionMatrix())
                // - GL's is Matrix::BIAS's Z row, the exact 0.5/0.5 remap
                // this used to hardcode here; Vulkan's is a Z-passthrough,
                // since TranslateProjectionMatrix() already remapped Z to
                // [0,1]. Re-applying *0.5+0.5 on top double-transformed
                // Vulkan's Z - the same bug class already fixed for
                // directional/spot shadows, just missed here.
                float depth = clip.z / clip.w;
                float shadow = 0.0;
                float x = 0.0;
                float y = 0.0;

                for (y = -1.5 ; y <=1.5 ; y+=1.0)
                    for (x = -1.5 ; x <=1.5 ; x+=1.0)
                        shadow += (texture(shadowMap, position_ls.xyz + vec3(vec2(x,y) * scale, 0.0)).r >= depth) ? 1.0 : 0.0;
                shadow /= 16.0;
                return shadow;
            }
            // Samplers can never be members of a uniform block, so
            // uPointShadowMaps stays a plain uniform; only the matrix array
            // moves to a UBO.
            UBO_BINDING(BIND_PointShadowBlock) uniform PointShadowBlock {
                mat4 uPointDepthsMVP[8];
            };
            SAMPLER_BINDING(BIND_uPointShadowMaps) uniform samplerCube uPointShadowMaps[4];
        #endif

        #ifdef SPOTSHADOW
            float PCFSPOT(sampler2DShadow shadowMap, mat4 sMatrix, float scale, vec4 pos)
            {
                vec4 coord = sMatrix * pos;
                coord.xyz/=coord.w;
                float shadow = 0.0;
                        shadow += texture(shadowMap, coord.xyz );
                return shadow;
            }

            SAMPLER_BINDING(BIND_uSpotShadowMaps) uniform sampler2DShadow uSpotShadowMaps[4];
            UBO_BINDING(BIND_SpotShadowBlock) uniform SpotShadowBlock {
                mat4 uSpotDepthsMVP[4];
            };
#endif

    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
        IO_LOCATION(LOC_vWorldPositionShadow) varying_in vec4 vWorldPositionShadow;
    #endif

    #if defined(SKINNING) || defined(ENVMAP) || defined(PARALLAXMAPPING) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
        IO_LOCATION(LOC_vWorldPosition) varying_in vec4 vWorldPosition;
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) ||  defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
        IO_LOCATION(LOC_vCameraPos) varying_in vec3 vCameraPos;
    #endif

    #ifdef ENVMAP
        SAMPLER_BINDING(BIND_uEnvmap) uniform samplerCube uEnvmap;
    #endif

    #ifdef REFRACTION
        SAMPLER_BINDING(BIND_uRefractmap) uniform samplerCube uRefractmap;
        IO_LOCATION(LOC_vReflectionFactor) varying_in float vReflectionFactor;
        IO_LOCATION(LOC_vTRed) varying_in vec3 vTRed;
        IO_LOCATION(LOC_vTGreen) varying_in vec3 vTGreen;
        IO_LOCATION(LOC_vTBlue) varying_in vec3 vTBlue;
    #endif

    #ifdef SKYBOX
        IO_LOCATION(LOC_v3Texcoord) varying_in vec3 v3Texcoord;
        SAMPLER_BINDING(BIND_uSkyboxmap) uniform samplerCube uSkyboxmap;
    #endif

    #ifdef SPECULARMAP
        SAMPLER_BINDING(BIND_uSpecularmap) uniform sampler2D uSpecularmap;
    #endif

    #ifdef PBRMAP
        SAMPLER_BINDING(BIND_uMetallicRoughnessmap) uniform sampler2D uMetallicRoughnessmap;
    #endif

    #ifdef DEFERRED_GBUFFER
        IO_LOCATION(LOC_gbuffer_normals) varying_in vec4 gbuffer_normals;
		layout(location = 0) out vec4 FragData_r;
		layout(location = 1) out vec4 FragData_g;
		layout(location = 2) out vec4 FragData_b;
		// PBR metallic/roughness (.r=roughness, .g=metalness - same G/B
		// convention uMetallicRoughnessmap uses, shifted since this
		// attachment only carries these two scalars). location=4's
		// FragColor comment above explains why this couldn't stay at 3.
		layout(location = 3) out vec4 FragData_pbr;
    #endif

   #if defined(DIFFUSE) || defined(CELLSHADING) || defined(DEFERRED_GBUFFER) || defined(PBR)
       UBO_BINDING(BIND_AmbientLightUniforms) uniform AmbientLightUniforms {
           vec4 uAmbientLight;
       };
   #endif

   #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
       IO_LOCATION(LOC_vViewMatrix) varying_in mat4 vViewMatrix;
   #endif

   #ifdef VELOCITY_RENDERING
	IO_LOCATION(LOC_vScreenSpaceWorldPosition) varying_smooth_in vec4 vScreenSpaceWorldPosition;
	IO_LOCATION(LOC_vPrvScreenSpaceWorldPosition) varying_smooth_in vec4 vPrvScreenSpaceWorldPosition;
   #endif

    #ifdef CLIPSPACE
        IO_LOCATION(LOC_vClipDist) varying_in float vClipDist;
    #endif

    void main() {

        #ifdef CLIPSPACE
            if (vClipDist < 0.0) discard;
        #endif

        // gbuffer_normals is a fragment-stage input (the vertex-interpolated
        // geometric normal); bump/parallax mapping below needs to replace it
        // per-fragment with the normal-mapped result before it reaches
        // FragData_b, which GLSL doesn't allow writing directly - shader
        // inputs are read-only. gbufferNormal is the mutable local that
        // actually gets written; it starts as a copy of the input so
        // non-bump-mapped materials still output the vertex normal unchanged.
        #ifdef DEFERRED_GBUFFER
            vec4 gbufferNormal = gbuffer_normals;
        #endif

        #ifdef COLOR
            if (!diffuseIsSet)
            {
                diffuse=uColor;
                diffuseIsSet=true;
            } else diffuse *= uColor;
        #endif

        #ifdef DEBUGRENDERING
            if (!diffuseIsSet)
            {
                diffuse=vColor;
                diffuseIsSet=true;
            } else diffuse *= vColor;
        #endif

        #if defined(INSTANCED_RENDERING) && defined(INSTANCED_COLOR)
            // Multiplied, not assigned: this is a per-instance *tint* over
            // whatever the material already produces, so one shared grass
            // texture can yield a field that isn't all exactly one green.
            // Alpha is included, so an instance can also be faded - and it
            // is applied before the ALPHATEST discard below, which means a
            // tint's alpha participates in the cutout rather than being
            // silently ignored.
            if (!diffuseIsSet)
            {
                diffuse=vInstanceColor;
                diffuseIsSet=true;
            } else diffuse *= vInstanceColor;
        #endif

        #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP) || defined(PBRMAP)
            vec2 Texcoord = vTexcoord;
            #if defined(PARALLAXMAPPING)
                vec3 viewDir = normalize(vTangentMatrix * (vCameraPos-vWorldPosition.xyz));
                Texcoord = ParallaxMapping(Texcoord, viewDir);
                if(Texcoord.x > 1.0 || Texcoord.y > 1.0 || Texcoord.x < 0.0 || Texcoord.y < 0.0)
                    discard;
            #endif
        #endif

        #ifdef TEXTURE
            if (!diffuseIsSet)
            {
                diffuse=texture_2D(uColormap,Texcoord);
                diffuseIsSet=true;
            } else diffuse *= texture_2D(uColormap,Texcoord);
        #endif

        #if defined(VERTEXCOLOR) && !defined(DEBUGRENDERING)
            if (!diffuseIsSet)
            {
                diffuse=vColor;
                diffuseIsSet=true;
            } else diffuse *= vColor;
        #endif

        #if defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)
            vec3 Normal;
            #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
                Normal = normalize(transpose3(vTangentMatrix) * (texture_2D(uNormalmap, Texcoord).rgb * 2.0 - 1.0));
                #if defined(DEFERRED_GBUFFER)
                    gbufferNormal.xyz = (vViewMatrix * vec4(Normal,0)).xyz;
                #endif
            #else
                Normal = vNormal;
            #endif
        #endif

        #ifdef TEXTRENDERING
            // Normal carries the per-character colour here - see Text.cpp,
            // which packs it into the normal attribute rather than adding a
            // colour stream.
            #ifdef TEXTSDF
                // The atlas holds a distance, 0.5 at the glyph edge. The
                // threshold width comes from the pixel's own rate of change,
                // which is what makes one bake stay sharp at any size: zoom
                // in and the field changes slowly across a pixel, so the edge
                // stays one pixel wide instead of blurring with the texture.
                float sdfDist = texture_2D(uFontmap,Texcoord).r;
                float sdfWidth = fwidth(sdfDist);
                // Never zero: a perfectly flat neighbourhood (a glyph scaled
                // so far down that a pixel spans the whole field) would
                // otherwise make smoothstep a hard step and alias badly.
                sdfWidth = max(sdfWidth, 0.0001);
                float textCoverage = smoothstep(0.5 - sdfWidth, 0.5 + sdfWidth, sdfDist);
            #else
                float textCoverage = texture_2D(uFontmap,Texcoord).r;
            #endif
            if (!diffuseIsSet)
            {
                diffuse=vec4(Normal*textCoverage,textCoverage);
                diffuseIsSet=true;
            } else diffuse *= vec4(Normal*textCoverage,textCoverage);
        #endif

        #if defined(ENVMAP) || defined(REFRACTION)
            vec3 Reflection = reflect((vWorldPosition.xyz - vCameraPos),normalize(vNormal));
            #ifdef ENVMAP
                diffuse.xyz = diffuse.xyz * (1.0-uReflectivity) + (texture_cube(uEnvmap,Reflection)).xyz*uReflectivity;
            #endif
            #ifdef REFRACTION
                vec4 reflectedColor = texture_cube(uRefractmap, Reflection);
                vec4 refractedColor;
                refractedColor.x = (texture_cube( uRefractmap, vTRed)).x; refractedColor.y = (texture_cube( uRefractmap, vTGreen)).y;
                refractedColor.z = (texture_cube( uRefractmap, vTBlue)).z;
                refractedColor.w = 1.0;
                if (!diffuseIsSet)
                {
                    diffuse = mix(reflectedColor, refractedColor, vReflectionFactor);
                    diffuseIsSet=true;
                } else diffuse *= mix(reflectedColor, refractedColor, vReflectionFactor);
            #endif
        #endif

        #ifdef SKYBOX
            if (!diffuseIsSet)
            {
                diffuse=texture_cube(uSkyboxmap,v3Texcoord);
                diffuseIsSet=true;
            } else diffuse *= texture_cube(uSkyboxmap,v3Texcoord);
        #endif

        #ifdef SPECULARCOLOR
            specular = uSpecular;
        #endif

        #ifdef SPECULARMAP
            specular = texture_2D(uSpecularmap,Texcoord);
        #endif

        #ifdef PBR
            float metallic = uMetallic;
            float roughness = uRoughness;
            #ifdef PBRMAP
                vec4 _pbrTex = texture_2D(uMetallicRoughnessmap, Texcoord);
                roughness = _pbrTex.g;
                metallic = _pbrTex.b;
            #endif
        #endif

        // !defined(DEFERRED_GBUFFER): a G-buffer-writing material must write
        // raw, unlit material properties (albedo/metallic/roughness/normal)
        // for the second pass to light later - running this loop here would
        // overwrite `diffuse` with an already-lit result before the
        // DEFERRED_GBUFFER write block below gets to read it. Zero-regression
        // guard tightening: no material anywhere in this codebase combines
        // DIFFUSE/CELLSHADING/PBR with DEFERRED_GBUFFER today (existing
        // G-buffer materials simply never set those flags) - this only
        // matters for a future DeferredRenderer_Gbuffer|PBR material.
        #if (defined(DIFFUSE) || defined(CELLSHADING) || defined(PBR)) && !defined(DEFERRED_GBUFFER)
            // Fragment Body
            vec4 _diffuse = uAmbientLight;
            vec4 _specular = vec4(0.0,0.0,0.0,1.0);
            #ifdef PBR
                vec3 _pbrColor = vec3(0.0);
            #endif

            vec3 Position = vWorldPosition.xyz;
            vec3 EyeVec = normalize(vCameraPos-Position);
            Normal = normalize(Normal);

            float lightIntensityCellShading;

            for (int i=0;i<MAX_LIGHTS;i++)
            {
                float lightIntensity, specularPower;
                float attenuation = 1.0;
                float spotEffect = 1.0;
                if (i<uNumberOfLights)
                {
                    mat4 Light = uLights[i];

                    vec3 LightDir;
                    vec4 LightColor;

                    LIGHT L;
                    buildLightFromMatrix(Light,L);
                    if (L.Type == 1.0)
                    {
                        LightDir = normalize(-L.Direction);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        CalculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef DIRECTIONALSHADOW
                            float DirectionalShadow = 1.0;
                            if (L.HaveShadowMap) {
                                bool MoreThanOneCascade = (uDirectionalShadowFar[0].y>0.0);
                                if (gl_FragCoord.z<uDirectionalShadowFar[0].x) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.0, uDirectionalDepthsMVP[0],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].y) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.0, uDirectionalDepthsMVP[1],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].z) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.5, uDirectionalDepthsMVP[2],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].w) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.5, uDirectionalDepthsMVP[3],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                            }

                            _diffuse += vec4(lightIntensity * L.Color.xyz * DirectionalShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * specular.xyz * DirectionalShadow, specularPower * L.Color.w * specular.w);
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * DirectionalShadow);
                        #else
                            _diffuse += lightIntensity * L.Color;
                            _specular += specularPower * L.Color * specular;
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity);
                        #endif

                        #ifdef PBR
                            #ifdef DIRECTIONALSHADOW
                                _pbrColor += CalculatePBRLighting(Normal, EyeVec, LightVec, L.Color.rgb, diffuse.rgb, metallic, roughness) * DirectionalShadow;
                            #else
                                _pbrColor += CalculatePBRLighting(Normal, EyeVec, LightVec, L.Color.rgb, diffuse.rgb, metallic, roughness);
                            #endif
                        #endif
                    }
                    else if (L.Type == 2.0)
                    {
                        LightDir = normalize(L.Position - Position);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        attenuation = Attenuation(Position, L.Position, L.Radius);

                        CalculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef POINTSHADOW
                            float PointShadow = 1.0;
                            if (attenuation>0.0 && L.HaveShadowMap)
                            {
                                PointShadow = 0.0;
                                PointShadow+=PCFPOINT(uPointShadowMaps[0],uPointDepthsMVP[(L.ShadowMap*2)],uPointDepthsMVP[(L.ShadowMap*2+1)],L.PCFTexelSize,L.ShadowBiasScale,vWorldPositionShadow);
                            }
                            _diffuse += vec4(lightIntensity * L.Color.xyz * attenuation * PointShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * attenuation * specular.xyz * PointShadow, specularPower * L.Color.w * specular.w);
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * attenuation * PointShadow);
                        #else
                            _diffuse += (lightIntensity + specularPower * _specular) * L.Color * attenuation;
                            _specular += specularPower * L.Color * specular * attenuation;
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * attenuation);
                        #endif

                        #ifdef PBR
                            #ifdef POINTSHADOW
                                _pbrColor += CalculatePBRLighting(Normal, EyeVec, LightVec, L.Color.rgb, diffuse.rgb, metallic, roughness) * attenuation * PointShadow;
                            #else
                                _pbrColor += CalculatePBRLighting(Normal, EyeVec, LightVec, L.Color.rgb, diffuse.rgb, metallic, roughness) * attenuation;
                            #endif
                        #endif
                    }
                    else if (L.Type == 3.0)
                    {
                        LightDir = normalize(L.Position - Position);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        attenuation = Attenuation(Position, L.Position, L.Radius);
                        spotEffect = 1.0 - DualConeSpotLight(Position, L.Position, L.Direction, L.Cones.x, L.Cones.y);

                        CalculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef SPOTSHADOW
                            float SpotShadow = 1.0;
                            if (spotEffect>0.0 && attenuation>0.0 && L.HaveShadowMap)
                            {
                                SpotShadow = 0.0;
                                SpotShadow+=PCFSPOT(uSpotShadowMaps[0],uSpotDepthsMVP[L.ShadowMap],L.PCFTexelSize,vWorldPositionShadow);
                            }
                            _diffuse += vec4(lightIntensity * L.Color.xyz * spotEffect * attenuation * SpotShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * spotEffect * attenuation * specular.xyz * SpotShadow, specularPower * L.Color.w * specular.w);
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * spotEffect * attenuation * SpotShadow);
                        #else
                            _diffuse += lightIntensity * L.Color * spotEffect * attenuation;
                            _specular += specularPower * L.Color * specular * spotEffect * attenuation;
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * spotEffect * attenuation);
                        #endif

                        #ifdef PBR
                            #ifdef SPOTSHADOW
                                _pbrColor += CalculatePBRLighting(Normal, EyeVec, LightVec, L.Color.rgb, diffuse.rgb, metallic, roughness) * spotEffect * attenuation * SpotShadow;
                            #else
                                _pbrColor += CalculatePBRLighting(Normal, EyeVec, LightVec, L.Color.rgb, diffuse.rgb, metallic, roughness) * spotEffect * attenuation;
                            #endif
                        #endif
                    }
                }
            }
            #if defined(DIFFUSE)
                diffuse = _diffuse * diffuse + _specular * specular;
            #elif defined(CELLSHADING)
                float factor = 3.0;
                if (lightIntensityCellShading > 0.95) factor = 3.0;
                else if (lightIntensityCellShading > 0.7) factor = 2.0;
                else if (lightIntensityCellShading > 0.5) factor = 1.0;
                else if (lightIntensityCellShading > 0.25) factor = 0.8;
                else factor = 0.5;
                diffuse = factor * diffuse;
            #elif defined(PBR)
                // Ambient placeholder (no IBL yet): dielectric-only ambient
                // response, scaled by the same per-channel ambient uniform
                // every other lighting path uses. Metals get none, matching
                // kD's (1-metallic) scaling in CalculatePBRLighting above.
                vec3 ambientPBR = diffuse.rgb * (1.0 - metallic) * uAmbientLight.rgb;
                diffuse = vec4(_pbrColor + ambientPBR, diffuse.w);
            #endif
        #endif

        // Cutout. Deliberately before everything that consumes `diffuse`
        // below - the lighting, the G-buffer writes and the CASTSHADOWS
        // depth write are all downstream, so one test here covers the
        // forward path, the deferred path and (for any shadow material
        // that samples a colormap) the shadow pass, rather than needing a
        // copy in each.
        #ifdef ALPHATEST
            if (diffuse.w < uAlphaCutoff)
                discard;
        #endif

        #ifdef CASTSHADOWS
            #if defined(GLLEGACY)
                diffuse = EncodeFloatRGBA(gl_FragCoord.z);
            #else
                // Plain depth in R. Directional and spot shadow FBOs are
                // depth-only, so their colour output is discarded either
                // way; a point light's cube map is an R32F colour target
                // (see PointLight::EnableCastShadows) and this is the value
                // it stores - the same gl_FragCoord.z its depth attachment
                // used to receive, so PCFPOINT's reference is unchanged.
                diffuse = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
            #endif
        #endif

        #ifdef DEFERRED_GBUFFER
		// The three alpha channels together carry one finished, additive
		// term that secondpassAmbient.glsl outputs as-is: the ambient
		// contribution, already multiplied by albedo and by (1-metallic)
		// here. It used to store only albedo*ambient and let that pass
		// multiply by albedo a *second* time, which made every deferred
		// surface's ambient albedo-SQUARED - 5x too dark at albedo 0.2, and
		// the main reason a deferred scene looked so much darker than the
		// same scene in forward. Doing the whole product on this side also
		// leaves the slot able to carry a genuinely post-lighting term,
		// which is what the Material Editor's custom materials add their
		// emissive to (see MaterialCodegen.cpp); a Generic material has no
		// emissive input, so it contributes nothing extra here.
		FragData_r=vec4(diffuse.xyz,diffuse.x*uAmbientLight.x);
		// RGB is the second pass's F0 *tint*, not a Blinn-Phong specular
		// colour any more (nothing ever read these three channels before -
		// secondpassPoint/Spot.glsl sampled them into a local that was then
		// dead, and directional/lastPass never sampled them at all - so this
		// repurposing costs nothing). A classic material's uSpecular is what
		// decides whether it has a highlight at all in ForwardRenderer
		// (`_specular += specularPower * L.Color * specular`), so feeding it
		// through here is what lets the deferred path reproduce that: a
		// material with no SpecularColor usage keeps specular==vec4(0) and
		// gets F0==0 (no highlight, same as forward), a white-specular one
		// gets the standard 0.04 dielectric F0. A PBR material's own F0
		// comes from albedo/metallic instead, so it writes a neutral 1.0
		// tint here and is left exactly as it was.
		#ifdef PBR
			FragData_g=vec4(1.0,1.0,1.0,diffuse.y*uAmbientLight.y);
		#else
			FragData_g=vec4(specular.xyz,diffuse.y*uAmbientLight.y);
		#endif
		FragData_b=vec4(gbufferNormal.xyz,diffuse.z*uAmbientLight.z);
		// Uses the already-resolved metallic/roughness locals (folds in
		// uMetallicRoughnessmap sampling when PBRMAP is set too), not the
		// raw uMetallic/uRoughness uniforms directly - see the #ifdef PBR
		// resolution block above. Blue channel is uSSRReflective
		// (MaterialUniforms) directly, not resolved through any texture -
		// real per-material SSR opt-in, see its declaration's comment.
		// Was always a hardcoded 0.0 here in both branches before this -
		// still 0.0/"not reflective" for any material that never calls
		// SetSSREnabled(), so this is purely additive. Alpha channel is
		// uReflectivity - the same per-material value that already blends
		// in env-map reflections above (see the uEnvmap mix() call
		// earlier in this shader) - reused here as an explicit artist-
		// facing SSR strength multiplier on top of the physically-driven
		// Fresnel/roughness falloff lastPass.glsl already does, not a
		// replacement for it. Was always a hardcoded 1.0 here before this
		// (alpha channel unused) - a material that calls SetSSREnabled()
		// but never calls SetReflectivity() now needs both, same as an
		// env-map material already needed uReflectivity set for its own
		// reflection to show at all.
		#ifdef PBR
			FragData_pbr=vec4(roughness, metallic, uSSRReflective, uReflectivity);
		#else
			// Was a hardcoded 0.5 - which is why every non-PBR material
			// looked flat and highlight-less through DeferredRenderer while
			// the identical material through ForwardRenderer showed a tight
			// uShininess-driven highlight (the deferred second pass is
			// Cook-Torrance-only and has no uShininess of its own). Standard
			// Blinn-Phong exponent -> GGX roughness mapping: a Phong lobe
			// pow(NdotH, s) and a GGX lobe of roughness a match when
			// s = 2/a^2 - 2. uShininess 50 -> ~0.196, i.e. the tight
			// highlight the forward path draws, not a 0.5 smear.
			FragData_pbr=vec4(clamp(sqrt(2.0/(max(uShininess,0.0)+2.0)),0.03,1.0), 0.0, uSSRReflective, uReflectivity);
		#endif
	#endif

	#ifdef VELOCITY_RENDERING
		vec2 a = (vScreenSpaceWorldPosition.xy / vScreenSpaceWorldPosition.w) * 0.5 + 0.5;
		vec2 b = (vPrvScreenSpaceWorldPosition.xy / vPrvScreenSpaceWorldPosition.w) * 0.5 + 0.5;
		// vec4 (not vec2): matches RGBA16F velocity target; RG16F+vec2 was
		// a no-op write on some macOS GL drivers while Vulkan was fine.
		FragColor = vec4(a - b, 0.0, 1.0);
	#elif !defined(DEFERRED_GBUFFER)
		FragColor = vec4(diffuse.xyz,diffuse.w*uOpacity);
	#endif
	// !defined(VELOCITY_RENDERING) && defined(DEFERRED_GBUFFER): no write
	// here at all, on purpose - this branch's FragColor is declared at
	// IO_LOCATION(4) above only because GL's own implicit-location
	// assignment picks something past FragData_pbr's location 3 when both
	// are declared together (see that comment), not because the G-buffer
	// MRT pass actually has a 5th attachment for it to land in. Writing to
	// it anyway compiled fine on GL (a harmlessly-discarded extra output)
	// but SPIR-V/Vulkan pipeline creation rejects a fragment shader that
	// writes a location with no matching
	// VkSubpassDescription::pColorAttachments entry outright (VUID: no
	// matching interfaces-fragmentoutput) - found via GenericShaderMaterial::
	// GetOrBuildGBufferProgram()'s very first real use of this branch
	// (nothing else in the engine had ever compiled a plain
	// GenericShaderMaterial with DEFERRED_GBUFFER for the actual
	// 4-attachment G-buffer pass before that existed).

    }

#endif
