#ifdef VERTEX
    const float tiling = 4.0;
    attribute vec3 aPosition, aNormal;
    attribute vec2 aTexcoord;
    uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
    uniform vec3 uCameraPos;
    varying vec2 vTexcoord;
    varying vec4 clipSpace;
    varying vec3 toCameraVector;
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
    uniform sampler2D uReflectionMap;
    uniform sampler2D uRefractionMap;
    uniform sampler2D uRefractionMapDepth;
    uniform sampler2D uNormalmap;
    uniform sampler2D uDUDVmap;
    uniform vec2 uNearFarPlane;
    uniform float uTime;
    varying vec2 vTexcoord;
    varying vec4 clipSpace;
    varying vec3 toCameraVector;
    const float waveStrength = 0.02;
    const float waveSpeed = 0.03;

    const vec3 lightVector = vec3(-1,-1,0);
    const float shineDamper = 20.0;
    const float reflectivity = 0.6;
    const vec3 lightColour = vec3(1,1,1);

    void main()
    {
        float moveFactor = (uTime * waveSpeed);
        vec2 ndc = (clipSpace.xy/clipSpace.w) * 0.5 + 0.5;
        vec2 refractionTexCoords = ndc;
        vec2 reflectionTexCoords = vec2(ndc.x, -ndc.y);

        float depth = texture2D(uRefractionMapDepth, refractionTexCoords).r;
        float floorDistance = 2.0 * uNearFarPlane.x * uNearFarPlane.y / (uNearFarPlane.x + uNearFarPlane.y - (2.0 * depth -1.0) * (uNearFarPlane.y - uNearFarPlane.x));
        
        depth = gl_FragCoord.z;
        float waterDistance = 2.0 * uNearFarPlane.x * uNearFarPlane.y / (uNearFarPlane.x + uNearFarPlane.y - (2.0 * depth -1.0) * (uNearFarPlane.y - uNearFarPlane.x));

        float waterDepth = floorDistance - waterDistance;

        vec2 distortedTexCoords = texture2D(uDUDVmap, vec2(vTexcoord.x + moveFactor, vTexcoord.y)).rg*0.1;
        distortedTexCoords = vTexcoord + vec2(distortedTexCoords.x, distortedTexCoords.y+moveFactor);
        vec2 totalDistortion = (texture2D(uDUDVmap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength;
        totalDistortion *= clamp(waterDepth/20.0, 0.0, 1.0);

        reflectionTexCoords += totalDistortion;
        refractionTexCoords += totalDistortion;

        // Fix Edges on Screen
        reflectionTexCoords.x = clamp(reflectionTexCoords.x, 0.001, 0.999);
        reflectionTexCoords.y = clamp(reflectionTexCoords.y, -0.999, -0.001);
        refractionTexCoords.x = clamp(refractionTexCoords.x, 0.001, 0.999);
        refractionTexCoords.y = clamp(refractionTexCoords.y, 0.001, 0.999);

        vec4 reflectColor = texture2D(uReflectionMap, reflectionTexCoords);
        vec4 refractColor = texture2D(uRefractionMap, refractionTexCoords);

        vec4 normalMapColor = texture2D(uNormalmap, distortedTexCoords);

        vec3 normal = vec3(normalMapColor.r * 2.0 -1.0, normalMapColor.b, normalMapColor.g * 2.0-1.0);

        // point normals upper
        normal.y *= 3.0;

        normal = normalize(normal);

        vec3 viewVector = normalize(toCameraVector);
        float refractiveFactor = pow(dot(viewVector, normal),2.0);

        // Lighting
        vec3 reflectedLight = reflect(normalize(lightVector), normal);
        float specular = max(dot(reflectedLight, viewVector), 0.0);
        specular = pow(specular, shineDamper);
        vec3 specularHighlights = lightColour * specular * reflectivity;
        // Lighting

        gl_FragColor = mix(reflectColor, refractColor, refractiveFactor);
        gl_FragColor = mix(gl_FragColor, vec4(0.0,0.3,0.5,1.0),0.2);

        gl_FragColor.a = clamp(waterDepth/5.0, 0.0, 1.0);

        // Lighting
        gl_FragColor += vec4(specularHighlights, 0.0);
    }
#endif