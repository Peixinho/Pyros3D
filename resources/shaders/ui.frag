#version 450
layout(set = 0, binding = 0, std140) uniform DrawData { mat4 transform; vec4 colour; vec4 flags; } uDraw;
layout(set = 1, binding = 1) uniform texture2D uTexture;      // (the picture, or the font's atlas)
layout(set = 1, binding = 2) uniform sampler uSampler;
layout(location = 0) in vec2 vTexcoord;
layout(location = 1) in vec3 vGlyphColour;
layout(location = 2) in vec4 vColor;
layout(location = 0) out vec4 outColor;
vec3 ToLinear(vec3 c) { vec3 x = max(c, vec3(0.0)); return mix(x / 12.92, pow((x + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), x)); }
void main()
{
	vec4 c = uDraw.flags.z > 0.5 ? vec4(ToLinear(vColor.rgb), vColor.a) * vec4(uDraw.colour.rgb, uDraw.colour.a) : uDraw.colour;
	if (uDraw.flags.x > 0.5)
	{
		// text: the atlas's coverage (or its distance to the glyph's edge, sharp at any size), each character's colour
		float d = texture(sampler2D(uTexture, uSampler), vTexcoord).r;
		float coverage = d;
		if (uDraw.flags.x > 1.5) { float w = max(fwidth(d), 0.0001); coverage = smoothstep(0.5 - w, 0.5 + w, d); }
		c = vec4(c.rgb * ToLinear(vGlyphColour), c.a * coverage);
	}
	else if (uDraw.flags.y > 0.5)
		c *= texture(sampler2D(uTexture, uSampler), vTexcoord);
	outColor = vec4(c.rgb, c.a);     // (the backbuffer is sRGB: the store encodes it)
}
