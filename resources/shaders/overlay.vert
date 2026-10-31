#version 450
// What is drawn over the finished picture (SceneRenderer::AddOverlay): an editor's grid, gizmos, debug lines - by the
// view without the temporal jitter, so they do not shimmer
layout(set = 0, binding = 0, std140) uniform DrawData { mat4 viewProjection; vec4 flags; } uDraw;     // flags.x: 1 hidden behind the scene
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;     // (sRGB, as the editor's colours are written)
layout(location = 0) out vec4 vColor;
void main()
{
	vColor = aColor;
	gl_Position = uDraw.viewProjection * vec4(aPosition, 1.0);
}
