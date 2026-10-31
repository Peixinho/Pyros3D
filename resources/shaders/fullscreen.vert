#version 450
// One triangle over the whole target, from the vertex index alone - no vertex buffer.
layout(location = 0) out vec2 vUV;
void main()
{
	vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vUV = vec2(p.x, 1.0 - p.y);          // (a texture's first row at the top)
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
