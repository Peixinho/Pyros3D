
#ifndef GLSL_PARALLAXMAPPING
#define GLSL_PARALLAXMAPPING

vec2 ParallaxMapping(sampler2D DisplacementMap, float DisplacementHeight, vec2 texCoords, vec3 viewDir)
{
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
    
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    vec2 P = viewDir.xy / viewDir.z * DisplacementHeight;
    vec2 deltaTexCoords = P / numLayers;
    
    vec2  currentTexCoords = texCoords;
    float currentDepthMapValue = texture(DisplacementMap, currentTexCoords).r;
    
    #if defined(GLES2) || defined(EMSCRIPTEN)
    for(int i=0;i<100;i++)
    if (currentLayerDepth < currentDepthMapValue)
    #else
    while (currentLayerDepth < currentDepthMapValue)
    #endif
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(DisplacementMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }
    #if defined(GLES2) || defined(EMSCRIPTEN)
    else break;
    #endif
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(DisplacementMap, prevTexCoords).r - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    return finalTexCoords;
}

#endif
