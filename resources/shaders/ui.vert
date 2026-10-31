#version 450
// The UI (a UICanvas's draw list): in the canvas's own units, projected flat over the window - no light, no depth
layout(set = 0, binding = 0, std140) uniform DrawData
{
	mat4 transform;          // canvas projection * view * the element's model
	vec4 colour;             // linear, a: opacity
	vec4 flags;              // x: 0 picture, 1 text, 2 text as a distance field; y: 1 with a texture; z: 1 the vertices' colours (sRGB); w: 1 in the world (its depth kept)
} uDraw;
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;      // (text: each character's colour, sRGB - Text.cpp packs it there)
layout(location = 2) in vec2 aTexcoord;
layout(location = 3) in vec4 aColor;      // (a batched quad's own colour - VertexColor; white where a mesh has none)
layout(location = 0) out vec2 vTexcoord;
layout(location = 1) out vec3 vGlyphColour;
layout(location = 2) out vec4 vColor;
void main()
{
	vTexcoord = aTexcoord;
	vGlyphColour = aNormal;
	vColor = aColor;
	vec4 p = uDraw.transform * vec4(aPosition, 1.0);
	// (flat UI at a fixed depth; text in the world keeps its own - flags.w - and is depth-tested against the scene)
	gl_Position = uDraw.flags.w > 0.5 ? p : vec4(p.xy, 0.5 * p.w, p.w);
}
