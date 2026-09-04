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


#include "GizmoTransformRender.h"
using namespace p3d;

bool CGizmoTransformRender::initialized = false;
DebugRenderer* CGizmoTransformRender::debug = NULL;
Matrix CGizmoTransformRender::currentModel;
Vec4 CGizmoTransformRender::currentColor = Vec4(1, 1, 1, 1);

void CGizmoTransformRender::Initialize()
{
	// Nothing to build any more. The gizmo used to compile its own GL program
	// and allocate a VAO/VBO here; it now feeds DebugRenderer, which owns all
	// of that behind IRenderDevice and works on GL, Vulkan and Metal alike.
	initialized = true;
}

void CGizmoTransformRender::Destroy()
{
	// Do not clear the static `debug` pointer. It is owned by SceneEditor
	// (SetDebugRenderer) and shared by every gizmo instance. UseTranslation/
	// Rotation/ScaleManipulator delete the old gizmo when switching tools;
	// nulling `debug` here made DrawVertices() no-op for the replacement.
	initialized = false;
}

void CGizmoTransformRender::SetUniforms(const Matrix &proj, const Matrix &view, const Matrix &model)
{
	// proj/view reach the GPU through DebugRenderer::Render(), which the
	// caller drives with the same scene camera - only the model transform is
	// per-primitive here.
	(void)proj; (void)view;
	currentModel = model;
}

void CGizmoTransformRender::SetColor(const Vec4 &color, const f32 opacity)
{
	currentColor = Vec4(color.x, color.y, color.z, color.w * opacity);
}

void CGizmoTransformRender::DrawVertices(const unsigned mode, const std::vector<tvector3> &vertices)
{
	if (debug == NULL || vertices.empty()) return;

	// tvector3 is x/y/z floats (see ZBaseMaths.h) but is not p3d::Vec3.
	struct Local {
		static Vec3 V(const tvector3 &v) { return Vec3(v.x, v.y, v.z); }
	};

	debug->pushMatrix(currentModel);

	switch (mode)
	{
	case GIZMO_LINES:
		for (size_t i = 0; i + 1 < vertices.size(); i += 2)
			debug->drawLine(Local::V(vertices[i]), Local::V(vertices[i + 1]), currentColor);
		break;

	case GIZMO_LINE_STRIP:
		for (size_t i = 0; i + 1 < vertices.size(); i++)
			debug->drawLine(Local::V(vertices[i]), Local::V(vertices[i + 1]), currentColor);
		break;

	case GIZMO_LINE_LOOP:
		for (size_t i = 0; i + 1 < vertices.size(); i++)
			debug->drawLine(Local::V(vertices[i]), Local::V(vertices[i + 1]), currentColor);
		// the closing segment a LINE_LOOP implies
		debug->drawLine(Local::V(vertices.back()), Local::V(vertices.front()), currentColor);
		break;

	case GIZMO_TRIANGLES:
		for (size_t i = 0; i + 2 < vertices.size(); i += 3)
			debug->drawTriangle(Local::V(vertices[i]), Local::V(vertices[i + 1]), Local::V(vertices[i + 2]), currentColor);
		break;

	case GIZMO_TRIANGLE_FAN:
		for (size_t i = 1; i + 1 < vertices.size(); i++)
			debug->drawTriangle(Local::V(vertices[0]), Local::V(vertices[i]), Local::V(vertices[i + 1]), currentColor);
		break;

	default:
		break;
	}

	debug->popMatrix();
}

void CGizmoTransformRender::DrawCircle(const tvector3 &orig,float r,float g,float b,const tvector3 &vtx,const tvector3 &vty, const tmatrix &proj, const tmatrix &modelview)
{



	Vec4 Color;
	Color.x=r; 
	Color.y=g;
	Color.z=b;
	Color.w=1;

	std::vector<tvector3> v;

	for (int i = 0; i < 50 ; i++)
	{
		tvector3 vt;
		vt = vtx * cos((2*ZPI/50)*i);
		vt += vty * sin((2*ZPI/50)*i);
		vt += orig;
		v.push_back(vt);
	}

	Matrix projectionMatrix, viewMatrix, modelMatrix;

	memcpy(&projectionMatrix.m[0],proj.m16,16*sizeof(float));
	memcpy(&viewMatrix.m[0],modelview.m16,16*sizeof(float));
	modelMatrix = Matrix();

    f32 opacity = 1.f;
	SetUniforms(projectionMatrix, viewMatrix, modelMatrix);
	SetColor(Color, opacity);

	DrawVertices(GIZMO_LINE_LOOP, v);


}


void CGizmoTransformRender::DrawCircleHalf(const tvector3 &orig,float r,float g,float b,const tvector3 &vtx,const tvector3 &vty,tplane &camPlan, const tmatrix &proj, const tmatrix &modelview)
{



	Vec4 Color;
	Color.x=r; 
	Color.y=g;
	Color.z=b;
	Color.w=1;

	std::vector<tvector3> v;

	for (int i = 0; i < 30 ; i++)
	{
		tvector3 vt;
		vt = vtx * cos((ZPI/30)*i);
		vt += vty * sin((ZPI/30)*i);
		vt +=orig;
		// Keep the half in front of the camera plane. DotNormal() is the dot
		// of the plane's normal with the point and ignores the plane's
		// distance entirely, so as a test it answered a question about the
		// world origin - and it was used as a bool, making "exactly on the
		// plane" the only rejection. The epsilon keeps the two points that
		// legitimately sit on the plane (the ends of the arc).
		if (camPlan.SignedDistanceTo(vt) >= -0.0001f)
			v.push_back(vt);
	}

	Matrix projectionMatrix, viewMatrix, modelMatrix;

	memcpy(&projectionMatrix.m[0],proj.m16,16*sizeof(float));
	memcpy(&viewMatrix.m[0],modelview.m16,16*sizeof(float));
	modelMatrix = Matrix();

    f32 opacity = 1.f;
	SetUniforms(projectionMatrix, viewMatrix, modelMatrix);
	SetColor(Color, opacity);

	DrawVertices(GIZMO_LINE_STRIP, v);


}

void CGizmoTransformRender::DrawAxis(const tvector3 &orig, const tvector3 &axis, const tvector3 &vtx,const tvector3 &vty, float fct,float fct2,const tvector4 &col, const tmatrix &proj, const tmatrix &modelview)
{



	Vec4 Color;
	Color.x=col.x; 
	Color.y=col.y;
	Color.z=col.z;
	Color.w=col.w;
    
    std::vector<tvector3> vLines;

    vLines.push_back(orig);
    vLines.push_back(tvector3(orig.x+axis.x,orig.y+axis.y,orig.z+axis.z));
	
	std::vector<tvector3> vTriangleFan;


	for (int i=0;i<=30;i++)
	{
		tvector3 pt;
		pt = vtx * cos(((2*ZPI)/30.0f)*i)*fct;
		pt+= vty * sin(((2*ZPI)/30.0f)*i)*fct;
		pt+=axis*fct2;
		pt+=orig;
		vTriangleFan.push_back(pt);

		
		pt = vtx * cos(((2*ZPI)/30.0f)*(i+1))*fct;
		pt+= vty * sin(((2*ZPI)/30.0f)*(i+1))*fct;
		pt+=axis*fct2;
		pt+=orig;

		vTriangleFan.push_back(pt);
		vTriangleFan.push_back(tvector3(orig.x+axis.x,orig.y+axis.y,orig.z+axis.z));
	}

	Matrix projectionMatrix, viewMatrix, modelMatrix;

	memcpy(&projectionMatrix.m[0],proj.m16,16*sizeof(float));
	memcpy(&viewMatrix.m[0],modelview.m16,16*sizeof(float));
	modelMatrix = Matrix();

    f32 opacity = 1.f;
	SetUniforms(projectionMatrix, viewMatrix, modelMatrix);
	SetColor(Color, opacity);

	DrawVertices(GIZMO_LINES, vLines);

	DrawVertices(GIZMO_TRIANGLE_FAN, vTriangleFan);




}
void CGizmoTransformRender::DrawAxisScale(const tvector3 &orig, const tvector3 &axis, const tvector3 &vtx,const tvector3 &vty, float fct,float fct2,const tvector4 &col, const tmatrix &proj, const tmatrix &modelview)
{

    

	Vec4 Color;
	Color.x=col.x; 
	Color.y=col.y;
	Color.z=col.z;
	Color.w=col.w;
    
    std::vector<tvector3> vLines;

    vLines.push_back(orig);
    vLines.push_back(tvector3(orig.x+axis.x,orig.y+axis.y,orig.z+axis.z));
	
	std::vector<tvector3> vQuadsFan;

	{
		tvector3 a,b,c,d,e,f,g,h;
		a=vtx*fct;
		b=vty*fct;
		c=-vtx*fct;
		d=-vty*fct;
		a+=axis*fct2;
		b+=axis*fct2;
		c+=axis*fct2;
		d+=axis*fct2;

		e=vtx*fct;
		f=vty*fct;
		g=-vtx*fct;
		h=-vty*fct;
		e+=axis*fct2;
		f+=axis*fct2;
		g+=axis*fct2;
		h+=axis*fct2;

		tmatrix m;
		tvector3 n = axis;
		m.RotationAxis(n, 45.f*3.14/180.f);
		
		a.TransformVector(m);
		b.TransformVector(m);
		c.TransformVector(m);
		d.TransformVector(m);
		e.TransformVector(m);
		f.TransformVector(m);
		g.TransformVector(m);
		h.TransformVector(m);

		a+=orig-axis*0.075f;
		b+=orig-axis*0.075f;
		c+=orig-axis*0.075f;
		d+=orig-axis*0.075f;

		e+=orig+axis*fct*1.5f-axis*0.075f;
		f+=orig+axis*fct*1.5f-axis*0.075f;
		g+=orig+axis*fct*1.5f-axis*0.075f;
		h+=orig+axis*fct*1.5f-axis*0.075f;

		vQuadsFan.push_back(tvector3(a.x,a.y,a.z));
		vQuadsFan.push_back(tvector3(b.x,b.y,b.z));
		vQuadsFan.push_back(tvector3(c.x,c.y,c.z));
		vQuadsFan.push_back(tvector3(c.x,c.y,c.z));
		vQuadsFan.push_back(tvector3(d.x,d.y,d.z));
		vQuadsFan.push_back(tvector3(a.x,a.y,a.z));

		vQuadsFan.push_back(tvector3(e.x,e.y,e.z));
		vQuadsFan.push_back(tvector3(f.x,f.y,f.z));
		vQuadsFan.push_back(tvector3(g.x,g.y,g.z));
		vQuadsFan.push_back(tvector3(g.x,g.y,g.z));
		vQuadsFan.push_back(tvector3(h.x,h.y,h.z));
		vQuadsFan.push_back(tvector3(e.x,e.y,e.z));

		vQuadsFan.push_back(tvector3(a.x,a.y,a.z));
		vQuadsFan.push_back(tvector3(e.x,e.y,e.z));
		vQuadsFan.push_back(tvector3(f.x,f.y,f.z));
		vQuadsFan.push_back(tvector3(f.x,f.y,f.z));
		vQuadsFan.push_back(tvector3(b.x,b.y,b.z));
		vQuadsFan.push_back(tvector3(a.x,a.y,a.z));

		vQuadsFan.push_back(tvector3(c.x,c.y,c.z));
		vQuadsFan.push_back(tvector3(d.x,d.y,d.z));
		vQuadsFan.push_back(tvector3(h.x,h.y,h.z));
		vQuadsFan.push_back(tvector3(h.x,h.y,h.z));
		vQuadsFan.push_back(tvector3(g.x,g.y,g.z));
		vQuadsFan.push_back(tvector3(c.x,c.y,c.z));

		vQuadsFan.push_back(tvector3(b.x,b.y,b.z));
		vQuadsFan.push_back(tvector3(c.x,c.y,c.z));
		vQuadsFan.push_back(tvector3(g.x,g.y,g.z));
		vQuadsFan.push_back(tvector3(g.x,g.y,g.z));
		vQuadsFan.push_back(tvector3(f.x,f.y,f.z));
		vQuadsFan.push_back(tvector3(b.x,b.y,b.z));

		vQuadsFan.push_back(tvector3(a.x,a.y,a.z));
		vQuadsFan.push_back(tvector3(d.x,d.y,d.z));
		vQuadsFan.push_back(tvector3(h.x,h.y,h.z));
		vQuadsFan.push_back(tvector3(h.x,h.y,h.z));
		vQuadsFan.push_back(tvector3(e.x,e.y,e.z));
		vQuadsFan.push_back(tvector3(a.x,a.y,a.z));
	}

	Matrix projectionMatrix, viewMatrix, modelMatrix;

	memcpy(&projectionMatrix.m[0],proj.m16,16*sizeof(float));
	memcpy(&viewMatrix.m[0],modelview.m16,16*sizeof(float));
	modelMatrix = Matrix();

    f32 opacity = 1.f;
	SetUniforms(projectionMatrix, viewMatrix, modelMatrix);
	SetColor(Color, opacity);

	DrawVertices(GIZMO_LINES, vLines);

	DrawVertices(GIZMO_TRIANGLES, vQuadsFan);





}

void CGizmoTransformRender::DrawCamem(const tvector3& orig,const tvector3& vtx,const tvector3& vty,float ng, const tmatrix &proj, const tmatrix &modelview)
{


	int i = 0 ;



	Vec4 Color = Vec4(1,1,0.0f,.5f);
    
    std::vector<tvector3> vTriangleFan;

    vTriangleFan.push_back(orig);

	for (i = 0 ; i <= 50 ; i++)
	{
		tvector3 vt;
		vt = vtx * cos(((ng)/50)*i);
		vt += vty * sin(((ng)/50)*i);
		vt+=orig;
		vTriangleFan.push_back(vt);
	}

	Vec4 Color2 = Vec4(1,1,0.2f,1);

	std::vector<tvector3> vLineLoop;

	vLineLoop.push_back(orig);
	
	for ( i = 0 ; i <= 50 ; i++)
	{
		tvector3 vt;
		vt = vtx * cos(((ng)/50)*i);
		vt += vty * sin(((ng)/50)*i);
		vt+=orig;
		vLineLoop.push_back(vt);
	}

	Matrix projectionMatrix, viewMatrix, modelMatrix;

	memcpy(&projectionMatrix.m[0],proj.m16,16*sizeof(float));
	memcpy(&viewMatrix.m[0],modelview.m16,16*sizeof(float));
	modelMatrix = Matrix();

    f32 opacity = .5f;
	SetUniforms(projectionMatrix, viewMatrix, modelMatrix);
	SetColor(Color, opacity);

	DrawVertices(GIZMO_TRIANGLE_FAN, vTriangleFan);


    opacity = 1.f;
	SetColor(Color2, opacity);

	DrawVertices(GIZMO_LINE_LOOP, vLineLoop);




}

void CGizmoTransformRender::DrawQuad(const tvector3& orig, float size, bool bSelected, const tvector3& axisU, const tvector3 &axisV, const tmatrix &proj, const tmatrix &modelview)
{



	tvector3 pts[4];
	pts[0] = orig;
	pts[1] = orig + (axisU * size);
	pts[2] = orig + (axisU + axisV)*size;
	pts[3] = orig + (axisV * size);

	Vec4 Color;

	if (!bSelected)
	{
		Color = Vec4(0.8,0.8,0.8,0.6f);
	} else {
		Color = Vec4(1,1,0,0.6f);
	}

	std::vector<tvector3> vQuad;
	
	vQuad.push_back(pts[0]);
	vQuad.push_back(pts[1]);
	vQuad.push_back(pts[2]);
	vQuad.push_back(pts[2]);
	vQuad.push_back(pts[3]);
	vQuad.push_back(pts[0]);

	std::vector<tvector3> vLineStrip;

	vLineStrip.push_back(pts[0]);
	vLineStrip.push_back(pts[1]);
	vLineStrip.push_back(pts[2]);
	vLineStrip.push_back(pts[3]);

	Matrix projectionMatrix, viewMatrix, modelMatrix;

	memcpy(&projectionMatrix.m[0],proj.m16,16*sizeof(float));
	memcpy(&viewMatrix.m[0],modelview.m16,16*sizeof(float));
	modelMatrix = Matrix();

    f32 opacity = .6f;
	SetUniforms(projectionMatrix, viewMatrix, modelMatrix);
	SetColor(Color, opacity);


	DrawVertices(GIZMO_TRIANGLES, vQuad);

	DrawVertices(GIZMO_LINE_STRIP, vLineStrip);



}
