#version 450
// Dear ImGui's draw lists (ImGuiRenderer): its pixels to the target's clip space
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 scaleTranslate; } uDraw;
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexcoord;
layout(location = 2) in vec4 aColor;      // (sRGB, as ImGui's style is written)
layout(location = 0) out vec2 vTexcoord;
layout(location = 1) out vec4 vColor;
void main()
{
	vTexcoord = aTexcoord;
	vColor = aColor;
	gl_Position = vec4(aPosition * uDraw.scaleTranslate.xy + uDraw.scaleTranslate.zw, 0.0, 1.0);
}
