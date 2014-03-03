float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius)
{
float d = distance(Vertex,LightPosition);
return clamp(1.0 - (1.0/Radius) * sqrt(d*d), 0.0, 1.0);
}
vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;

uniform float uOpacity;
uniform sampler2D tDiffuse;
uniform sampler2D tSpecular;
uniform sampler2D tNormal;
uniform sampler2D tPosition;
uniform sampler2D tDepth;
uniform vec2 uScreenDimensions;
uniform vec3 uLightPosition;
uniform float uLightRadius;
uniform vec4 uLightColor;
void main() {
vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x,gl_FragCoord.y/uScreenDimensions.y);
vec3 WorldPos = texture2D(tPosition, vec2(Texcoord.x,1.0-Texcoord.y)).xyz;
vec3 Color = texture2D(tDiffuse, vec2(Texcoord.x,1.0-Texcoord.y)).xyz;
vec3 Normal = normalize(texture2D(tNormal, vec2(Texcoord.x,1.0-Texcoord.y)).xyz);
vec3 Specular = texture2D(tSpecular, vec2(Texcoord.x,1.0-Texcoord.y)).xyz;
vec3 Depth = texture2D(tDepth, vec2(Texcoord.x,1.0-Texcoord.y)).xyz;
float lightRadius = uLightRadius;
vec3 lightPosition = uLightPosition;
vec4 lightColor = uLightColor;
vec3 l = normalize(lightPosition-WorldPos);
float n_dot_l = max(dot(Normal, l), 0.0);
float attenuation = Attenuation(WorldPos, uLightPosition, uLightRadius);
vec3 diffuseColor = n_dot_l * attenuation * lightColor.xyz;
diffuse = vec4((diffuseColor * Color),1.0);
gl_FragColor=diffuse*uOpacity;
}
