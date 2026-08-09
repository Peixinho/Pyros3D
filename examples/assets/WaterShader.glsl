#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif

#if defined(VULKAN)
#define UBO_BINDING(n) layout(std140, binding = n)
#define SAMPLER_BINDING(n) layout(set = 1, binding = n)
#define IO_LOCATION(n) layout(location = n)
#else
#define UBO_BINDING(n) layout(std140)
#define SAMPLER_BINDING(n)
#define IO_LOCATION(n)
#endif

#ifdef VERTEX
    const float tiling = 4.0;
    IO_LOCATION(0) attribute_in vec3 aPosition;
    IO_LOCATION(1) attribute_in vec3 aNormal;
    IO_LOCATION(2) attribute_in vec2 aTexcoord;
    UBO_BINDING(40) uniform WaterVertParams {
        mat4 uProjectionMatrix;
        mat4 uViewMatrix;
        mat4 uModelMatrix;
        vec3 uCameraPos;
    };
    IO_LOCATION(0) varying_out vec2 vTexcoord;
    IO_LOCATION(1) varying_out vec4 clipSpace;
    IO_LOCATION(2) varying_out vec3 toCameraVector;
    void main()
    {
        vTexcoord = aTexcoord * tiling;
        vec4 worldPosition = uModelMatrix * vec4(aPosition, 1.0);
        clipSpace = uProjectionMatrix * uViewMatrix * worldPosition;
        gl_Position = clipSpace;
        toCameraVector = uCameraPos - worldPosition.xyz;
    }
#endif

#ifdef FRAGMENT
    SAMPLER_BINDING(0) uniform sampler2D uReflectionMap;
    SAMPLER_BINDING(1) uniform sampler2D uRefractionMap;
    SAMPLER_BINDING(2) uniform sampler2D uRefractionMapDepth;
    SAMPLER_BINDING(3) uniform sampler2D uNormalmap;
    SAMPLER_BINDING(4) uniform sampler2D uDUDVmap;
    UBO_BINDING(41) uniform WaterFragParams {
        vec2 uNearFarPlane;
        float uTime;
    };
    IO_LOCATION(0) varying_in vec2 vTexcoord;
    IO_LOCATION(1) varying_in vec4 clipSpace;
    IO_LOCATION(2) varying_in vec3 toCameraVector;
    const float waveStrength = 0.02;
    const float waveSpeed = 0.03;

    const vec3 lightVector = vec3(-1,-1,0);
    const float shineDamper = 20.0;
    const float reflectivity = 0.6;
    const vec3 lightColour = vec3(1,1,1);

	IO_LOCATION(0) out vec4 FragColor;

    void main()
    {
        float moveFactor = (uTime * waveSpeed);
        vec2 ndc = (clipSpace.xy/clipSpace.w) * 0.5 + 0.5;
        // *0.5+0.5 produces a GL-convention texcoord (v=0 at NDC y=-1, the
        // bottom). Metal's NDC Y points up like GL's, but its render targets
        // store v=0 at the top - the same mismatch IEffect.cpp's full-screen
        // quad documents - so a projective lookup computed in the shader
        // (rather than interpolated) has to flip v to hit the texel this
        // fragment actually corresponds to. Without it the reflection and
        // refraction maps were sampled vertically mirrored, so the water
        // showed a plausible-looking image that slid around as the camera
        // moved instead of staying locked to the scene it reflects.
        #if defined(METAL)
        ndc.y = 1.0 - ndc.y;
        #endif
        vec2 refractionTexCoords = ndc;
        // Reflection is rendered by a mirrored camera, so it is always the
        // opposite of the refraction coordinate - that relationship holds on
        // either backend, on top of whichever base convention ndc is in.
        vec2 reflectionTexCoords = vec2(ndc.x, 1.0 - ndc.y);

        float depth = texture_2D(uRefractionMapDepth, refractionTexCoords).r;
        float floorDistance = 2.0 * uNearFarPlane.x * uNearFarPlane.y / (uNearFarPlane.x + uNearFarPlane.y - (2.0 * depth -1.0) * (uNearFarPlane.y - uNearFarPlane.x));

        depth = gl_FragCoord.z;
        float waterDistance = 2.0 * uNearFarPlane.x * uNearFarPlane.y / (uNearFarPlane.x + uNearFarPlane.y - (2.0 * depth -1.0) * (uNearFarPlane.y - uNearFarPlane.x));

        float waterDepth = floorDistance - waterDistance;

        vec2 distortedTexCoords = texture_2D(uDUDVmap, vec2(vTexcoord.x + moveFactor, vTexcoord.y)).rg*0.1;
        distortedTexCoords = vTexcoord + vec2(distortedTexCoords.x, distortedTexCoords.y+moveFactor);
        vec2 totalDistortion = (texture_2D(uDUDVmap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength;
        totalDistortion *= clamp(waterDepth/20.0, 0.0, 1.0);

        reflectionTexCoords += totalDistortion;
        refractionTexCoords += totalDistortion;

        reflectionTexCoords.x = clamp(reflectionTexCoords.x, 0.001, 0.999);
        reflectionTexCoords.y = clamp(reflectionTexCoords.y, 0.001, 0.999);
        refractionTexCoords.x = clamp(refractionTexCoords.x, 0.001, 0.999);
        refractionTexCoords.y = clamp(refractionTexCoords.y, 0.001, 0.999);

        vec4 reflectColor = texture_2D(uReflectionMap, reflectionTexCoords);
        vec4 refractColor = texture_2D(uRefractionMap, refractionTexCoords);

        vec4 normalMapColor = texture_2D(uNormalmap, distortedTexCoords);
        vec3 normal = vec3(normalMapColor.r * 2.0 -1.0, normalMapColor.b, normalMapColor.g * 2.0-1.0);
        normal.y *= 3.0;
        normal = normalize(normal);

        vec3 viewVector = normalize(toCameraVector);
        float refractiveFactor = pow(max(dot(viewVector, normal), 0.0), 2.0);
        refractiveFactor = clamp(refractiveFactor, 0.0, 1.0);

        vec3 reflectedLight = reflect(normalize(lightVector), normal);
        float specular = max(dot(reflectedLight, viewVector), 0.0);
        specular = pow(specular, shineDamper);
        vec3 specularHighlights = lightColour * specular * reflectivity;

        FragColor = mix(reflectColor, refractColor, refractiveFactor);
        FragColor = mix(FragColor, vec4(0.0,0.3,0.5,1.0), 0.2);
        FragColor.a = max(clamp(waterDepth/5.0, 0.0, 1.0), 0.75);
        FragColor += vec4(specularHighlights, 0.0);
    }
#endif
