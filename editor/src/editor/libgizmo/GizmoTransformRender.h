///////////////////////////////////////////////////////////////////////////////////////////////////
// LibGizmo
// File Name : 
// Creation : 10/01/2012
// Author : Cedric Guillemet
// Description : LibGizmo
//
///Copyright (C) 2012 Cedric Guillemet
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
//of the Software, and to permit persons to whom the Software is furnished to do
///so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
///FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 



#ifndef GIZMOTRANSFORMRENDER_H__
#define GIZMOTRANSFORMRENDER_H__

#include "ZBaseMaths.h"
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <vector>
typedef tvector4 tplane;

using namespace p3d;

// Primitive modes, replacing the GL_* constants this used to pass around -
// nothing here should name an API any more.
enum {
	GIZMO_LINES = 0,
	GIZMO_LINE_STRIP,
	GIZMO_LINE_LOOP,
	GIZMO_TRIANGLES,
	GIZMO_TRIANGLE_FAN
};

class CGizmoTransformRender  
{
public:
	CGizmoTransformRender() {}
	virtual ~CGizmoTransformRender() {}

	static void Initialize();
	static void DrawCircle(const tvector3 &orig,float r,float g,float b,const tvector3 &vtx,const tvector3 &vty, const tmatrix &proj, const tmatrix &modelview);
	static void DrawCircleHalf(const tvector3 &orig,float r,float g,float b,const tvector3 &vtx,const tvector3 &vty,tplane &camPlan, const tmatrix &proj, const tmatrix &modelview);
	static void DrawAxis(const tvector3 &orig, const tvector3 &axis, const tvector3 &vtx,const tvector3 &vty, float fct,float fct2,const tvector4 &col,const tmatrix &proj, const tmatrix &modelview);
	static void DrawAxisScale(const tvector3 &orig, const tvector3 &axis, const tvector3 &vtx,const tvector3 &vty, float fct,float fct2,const tvector4 &col, const tmatrix &proj, const tmatrix &modelview);
	static void DrawCamem(const tvector3& orig,const tvector3& vtx,const tvector3& vty,float ng, const tmatrix &proj, const tmatrix &modelview);
	static void DrawQuad(const tvector3& orig, float size, bool bSelected, const tvector3& axisU, const tvector3 &axisV, const tmatrix &proj, const tmatrix &modelview);
	static void Destroy();

	// Expands `vertices` into the engine's DebugRenderer as lines or
	// triangles. This used to own a GL program, VAO and VBO and issue its own
	// draw call - the last raw-GL code in the editor, and the reason the
	// gizmo could not exist on Vulkan or Metal. DebugRenderer already does
	// exactly this job (dynamic per-frame line/triangle geometry, through
	// IRenderDevice) on every backend.
	static void DrawVertices(const unsigned mode, const std::vector<tvector3> &vertices);

	// Set once by SceneEditor. Primitives accumulate in it and are flushed by
	// its Render() call, which must therefore run after the gizmo draws.
	static void SetDebugRenderer(DebugRenderer* renderer) { debug = renderer; }

	// Replaces the shader uniforms this used to push: the model matrix
	// becomes DebugRenderer's pushMatrix, the colour a per-primitive value.
	// proj/view are already supplied to DebugRenderer::Render() by the caller.
	static void SetUniforms(const Matrix &proj, const Matrix &view, const Matrix &model);
	static void SetColor(const Vec4 &color, const f32 opacity);

private:
    static bool initialized;
    static DebugRenderer* debug;
    static Matrix currentModel;
    static Vec4 currentColor;
};

#endif // !defined(AFX_GIZMOTRANSFORMRENDER_H__549F6E7A_D46D_4B18_9E74_76B7E43A3841__INCLUDED_)
