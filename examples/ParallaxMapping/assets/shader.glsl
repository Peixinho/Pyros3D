#ifdef EMSCRIPTEN
precision mediump float;
#endif

#ifdef VERTEX
attribute vec3 aPosition, aNormal, aTangent, aBitangent;
attribute vec2 aTexcoord;

uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
uniform vec3 uViewPos;

varying vec3 vFragPos;
varying vec2 vTexcoords;
varying vec3 vTangentLightPos;
varying vec3 vTangentViewPos;
varying vec3 vTangentFragPos;

vec3 lightPos = vec3(-50,50,50);

void main()
{
	gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
	vFragPos = vec3(uModelMatrix * vec4(aPosition,1.0));
	vTexcoords = aTexcoord;

	vec3 T = normalize(uModelMatrix * vec4(aTangent,0)).xyz;
	vec3 B = normalize(uModelMatrix * vec4(aBitangent,0)).xyz;
	vec3 N = normalize(uModelMatrix * vec4(aNormal,0)).xyz;
	mat3 TBN = transpose(mat3(T, B, N));

	vTangentLightPos = TBN * lightPos;
	vTangentViewPos = TBN * uViewPos;
	vTangentFragPos = TBN * vFragPos;
}
#endif

#ifdef FRAGMENT
uniform sampler2D uTexturemap, uNormalmap, uDisplacementmap;
uniform float uHeightScale;

varying vec3 vFragPos;
varying vec2 vTexcoords;
varying vec3 vTangentLightPos;
varying vec3 vTangentViewPos;
varying vec3 vTangentFragPos;

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{ 
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    vec2 P = viewDir.xy / viewDir.z * uHeightScale; 
    vec2 deltaTexCoords = P / numLayers;
  
    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture2D(uDisplacementmap, currentTexCoords).r;
      
    while(currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture2D(uDisplacementmap, currentTexCoords).r;  
        currentLayerDepth += layerDepth;  
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture2D(uDisplacementmap, prevTexCoords).r - currentLayerDepth + layerDepth;
 
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
} 

void main()
{
	vec3 viewDir = normalize(vTangentViewPos - vTangentFragPos);
	vec2 texcoords = vTexcoords;
    texcoords = ParallaxMapping(texcoords,  viewDir);       
    if(texcoords.x > 1.0 || texcoords.y > 1.0 || texcoords.x < 0.0 || texcoords.y < 0.0)
    	discard;

    vec3 normal = texture2D(uNormalmap, texcoords).rgb;
    normal = normalize(normal * 2.0 - 1.0);

	vec3 diffuse = texture2D(uTexturemap, texcoords).rgb;
	vec3 ambient = 0.05 * diffuse;

	//vec3 lightDir = normalize(vTangentLightPos - vTangentFragPos);
	vec3 lightDir = normalize(vTangentLightPos);

    float diff = max(dot(lightDir, normal), 0.0);
    diffuse *= diff;

	float spec = 0.2;
	bool blinn = false;
    if(blinn)
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);  
        spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    }
    else
    {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }

    vec3 specular = vec3(0.3) * spec; // assuming bright white light color
    gl_FragColor = vec4(ambient + diffuse + specular, 1.0);
}

#endif