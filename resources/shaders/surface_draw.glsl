// The surface's DrawData (set 0): the object and its material's numbers - 256 bytes (the most a DrawData may be)
layout(set = 0, binding = 0, std140) uniform DrawData
{
	mat4 model;
	vec4 normalRows[3];      // the model's inverse transpose, its 3x3, as rows; [0].w: how much sunlight it lets through (a leaf)
	vec4 baseColor;          // rgb linear, a opacity
	vec4 surface;            // x roughness, y metallic, z specular (F0 = 0.08 * z for a dielectric), w: 1 with a normal map
	vec4 emission;           // rgb linear, w: alpha cutoff (0 none)
	vec4 uvTransform;        // xy scale, zw offset
	vec4 skin;               // x: its first bone in the frame's palette (skinned: SKINNED); y: its height map's depth (parallax, 0 none)
	vec4 wind;               // a plant's sway - wind.glsl
	vec4 wet;                // x how wet (0 dry .. 1 soaked), y how much of what faces up lies under water, z a puddle's size (m)
	vec4 subsurface;         // rgb the colour light takes under the surface (linear), w how far it travels (0 none .. 1 ~5 cm)
	vec4 shape;              // an instanced set's: its instances grown and faded with distance - instancing.glsl, ShapedInstance
} uDraw;
