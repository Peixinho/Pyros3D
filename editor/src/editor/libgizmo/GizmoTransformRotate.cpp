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

#include "GizmoTransformRotate.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
extern tvector3 ptd;


// Builds the two in-plane vectors a rotation ring is drawn from, for the ring
// whose normal is `axis`, seen along `dir`.
//
// `right` is the ring's silhouette direction (perpendicular to both the view
// and the axis) and `frnt` points back towards the camera - so sweeping half a
// turn from `right` towards `frnt` traces exactly the near half of the ring,
// which is what DrawCircleHalf relies on to hide the back of it.
//
// Returns false when the axis points at (or straight away from) the camera.
// There the construction collapses: dir x axis is very nearly the zero vector,
// Normalize() divides by ~0, and the resulting "silhouette" is whatever float
// noise was left behind - so the half that gets drawn starts at an arbitrary
// angle and reads as an arc lying off to one side of the object rather than as
// a ring around it. A 2D scene is permanently in that case (viewed face-on,
// rotating about Z), which is why its rotation gizmo never looked like a
// circle. A ring seen face-on has no back half to hide, so the caller draws
// the whole circle instead.
static bool BuildRingBasis(const tvector3 &dir, const tvector3 &axis,
    tvector3 &right, tvector3 &frnt)
{
    right.Cross(dir, axis);

    // |dir x axis| is sin(angle between them), both being unit length.
    const float sinAngle = right.Length();

    // ~4 degrees. Nearer than that the two ends of the arc are within a couple
    // of pixels of each other anyway, so closing it into a full circle costs
    // nothing visually and removes the numerically unstable band entirely.
    if (sinAngle < 0.07f)
    {
        // Any pair of vectors perpendicular to the axis will do - the whole
        // ring is drawn, so where it starts does not matter.
        const tvector3 seed = (fabsf(axis.z) < 0.9f)
            ? tvector3(0.f, 0.f, 1.f) : tvector3(0.f, 1.f, 0.f);
        right.Cross(seed, axis);
        right.Normalize();
        frnt.Cross(right, axis);
        frnt.Normalize();
        return false;
    }

    right /= sinAngle;
    frnt.Cross(right, axis);
    frnt.Normalize();
    return true;
}

// One rotation ring. ALWAYS a complete circle: an axis seen edge-on used to be
// drawn as its near half only, which is a depth cue nobody asked for and which
// reads as a broken arc rather than as a ring you can grab - and the ring is
// grabbable all the way round, so drawing half of it was also a lie about the
// hit target. The far half is drawn dimmed underneath, so the circle is whole
// and still says which side is towards you.
static void DrawRotationRing(const tvector3 &orig, const tvector3 &dir,
    const tvector3 &axis, float r, float g, float b, float screenFactor,
    tplane &plCam, const tmatrix &proj, const tmatrix &model)
{
    tvector3 right, frnt;
    const bool edgeOn = BuildRingBasis(dir, axis, right, frnt);

    const tvector3 vx = right * screenFactor;
    const tvector3 vy = frnt * screenFactor;

    if (!edgeOn)
    {
        // Face-on: every point is the same distance away, so there is no near
        // half to emphasise.
        CGizmoTransformRender::DrawCircle(orig, r, g, b, vx, vy, proj, model);
        return;
    }

    const float dim = 0.4f;
    CGizmoTransformRender::DrawCircle(orig, r * dim, g * dim, b * dim, vx, vy, proj, model);
    CGizmoTransformRender::DrawCircleHalf(orig, r, g, b, vx, vy, plCam, proj, model);
}

IGizmo *CreateRotateGizmo()
{
    return new CGizmoTransformRotate;
}


CGizmoTransformRotate::CGizmoTransformRotate() : CGizmoTransform()
{
    m_RotateType = ROTATE_NONE;
    m_RotateTypePredict = ROTATE_NONE;
    m_Ng2 = 0;
    m_AngleSnap = 0.f;
	gizmoType = 2;
}

CGizmoTransformRotate::~CGizmoTransformRotate()
{

}

bool CGizmoTransformRotate::CheckRotatePlan(tvector3 &vNorm, float factor,
    const tvector3 &rayOrig,const tvector3 &rayDir,int id)
{
    tvector3 df, inters;
    m_Axis2 = vNorm;

	m_plan=vector4(localTransform.GetTranslation(), vNorm);
    m_plan.RayInter(inters,rayOrig,rayDir);
    ptd = inters;
	df = inters - localTransform.GetTranslation();
    df/=GetScreenFactor();

    if ( ((df.Length()/factor) >0.9f) && ( (df.Length()/factor) < 1.1f) )
    {
        m_svgMatrix = *m_pMatrix;

        m_LockVertex = df;
        m_LockVertex.Normalize();

        m_Vtx = m_LockVertex;
        m_Vty.Cross(m_LockVertex,vNorm);
        m_Vty.Normalize();
        m_Vtx *= factor;
        m_Vty *= factor;
        m_Vtz.Cross(m_Vtx,m_Vty);
        m_Ng2 = 0;
        if (id!=-1)
            m_Axis = GetVector(id);

        m_OrigScale.Scaling(GetTransformedVector(0).Length(),
            GetTransformedVector(1).Length(),
            GetTransformedVector(2).Length());

        m_InvOrigScale.Inverse(m_OrigScale);

        return true;
    }
    return false;
}

bool CGizmoTransformRotate::GetOpType(ROTATETYPE &type, unsigned int x, unsigned int y)
{
    tvector3 rayOrigin,rayDir, axis, dir;
	dir = localTransform.GetTranslation()-m_CamSrc;
	
	dir.Normalize();

    BuildRay(x, y, rayOrigin, rayDir);

        // plan 1 : X/Z
        m_Axis = GetTransformedVector(0);
        if (mMask&AXIS_X)
            if (CheckRotatePlan(m_Axis,1.0f,rayOrigin,rayDir,0)) { type = ROTATE_X; return true; }
            m_Axis = GetTransformedVector(1);
            if (mMask&AXIS_Y)
                if (CheckRotatePlan(m_Axis,1.0f,rayOrigin,rayDir,1)) { type = ROTATE_Y; return true; }
                m_Axis = GetTransformedVector(2);
                if (mMask&AXIS_Z)
					if (CheckRotatePlan(m_Axis,1.0f,rayOrigin,rayDir,2)) { type = ROTATE_Z; return true; }

					//m_Axis = GetTransformedVector(dir);
                    if (mMask&AXIS_SCREEN)
                        if (CheckRotatePlan(dir,1.2f,rayOrigin,rayDir,-1))
                        {
							tmatrix mt = localTransform;
                            mt.NoTrans();
                            mt.Inverse();
                            m_Axis = m_Axis2 = dir;
                            m_Axis.TransformPoint(mt);

                            m_Axis *=tvector3(GetTransformedVector(0).Length(),
                                GetTransformedVector(1).Length(),
                                GetTransformedVector(2).Length());

                            type = ROTATE_SCREEN;
                            return true;
                        }

        type = ROTATE_NONE;

        return false;
}

bool CGizmoTransformRotate::OnMouseDown(unsigned int x, unsigned int y)
{
    if (m_pMatrix)
    {
        return GetOpType(m_RotateType, x, y);
    }

    m_RotateType = ROTATE_NONE;
    return false;
}


void CGizmoTransformRotate::Rotate1Axe(const tvector3& rayOrigin,const tvector3& rayDir)
{
    tvector3 inters;
	m_plan=vector4(localTransform.GetTranslation(), m_Axis2);
    m_plan.RayInter(inters,rayOrigin,rayDir);
    ptd = inters;

	tvector3 df;
	df = inters - localTransform.GetTranslation();

    df.Normalize();
    m_LockVertex2 = df;

    float acosng = df.Dot(m_LockVertex);
    if ( (acosng<-0.99999f) || (acosng> 0.99999f) )
        m_Ng2 = 0.f;
    else
        m_Ng2 = (float)acos(acosng);

    if (df.Dot(m_Vty)>0)
        m_Ng2 = -m_Ng2;

    tmatrix mt,mt2;

    if (m_bUseSnap)
    {
        m_Ng2*=(360.0f/ZPI);
        SnapIt(m_Ng2,m_AngleSnap);
        m_Ng2/=(360.0f/ZPI);
    }

	if (mLocation==LOCATE_WORLD)
	{
		mt.Identity();
		mt.RotationAxis(m_Axis,m_Ng2);
		*m_pMatrix=mt;
	} else {
		// m_Axis is the rotation axis in world space (the object's local
		// axis as seen from the camera), but m_pMatrix is expressed in the
		// parent's frame. Conjugate the axis through the inverse parent
		// transform before pre-multiplying (identity for a root object),
		// otherwise a child of a rotated parent rotates about the wrong axis.
		tvector3 axis = m_Axis;
		tmatrix iparent;
		iparent = globalTransform;
		iparent.Inverse();
		axis.TransformVector(iparent);
		if (axis.Length() > 0.0001f)
			axis.Normalize();

		tmatrix svg = m_svgMatrix;
		tvector3 trf = svg.GetTranslation();
		svg.NoTrans();

		mt.RotationAxis(axis,m_Ng2);
		mt.Multiply(m_InvOrigScale);
		mt.Multiply(svg);
		mt2 = m_OrigScale;
		mt2.Multiply(mt);

		// Restore the start translation - rotating the full start matrix
		// would swing the position around the origin.
		mt2.V4.position.x = trf.x;
		mt2.V4.position.y = trf.y;
		mt2.V4.position.z = trf.z;
		*m_pMatrix=mt2;
	}

    if (m_Axis == tvector3::ZAxis)
    {
        if (mEditQT)
        {
            /*
            Dans le cadre du jeu, et vu les pb avec les quaternions,
            le 1er float du quaternion en stockage est l'angle en radian.
            le stockage reste un quaternion.
            il y a des pbs de conversion quaternion/matrix
            */
            tquaternion gth(*m_pMatrix);

            gth.Normalize();
            gth.UnitInverse();

            tquaternion qtg;
            qtg.RotationAxis(m_Axis,m_Ng2);
            *mEditQT = gth;//gth+qtg;//tquaternion(mt2);
            mEditQT->Normalize();
        }
    }
}

void CGizmoTransformRotate::OnMouseMove(unsigned int x, unsigned int y)
{
    tvector3 rayOrigin, rayDir, axis;

    BuildRay(x, y, rayOrigin, rayDir);

    if (m_RotateType != ROTATE_NONE)
    {
        {
            Rotate1Axe(rayOrigin, rayDir);
        }

        //if (mTransform) mTransform->Update();
    }
    else
    {
        // predict move
        if (m_pMatrix)
        {
            GetOpType(m_RotateTypePredict, x, y);
        }
    }
}

void CGizmoTransformRotate::OnMouseUp(unsigned int x, unsigned int y)
{
    m_RotateType = ROTATE_NONE;
}
/*
            char tmps[512];
            sprintf(tmps, "%5.2f %5.2f %5.2f %5.2f", plCam.x, plCam.y, plCam.z, plCam.w );
            MessageBoxA(NULL, tmps, tmps, MB_OK);
            */
void CGizmoTransformRotate::Draw()
{
    if (m_pMatrix)
    {

        ComputeScreenFactor();

        tvector3 right,up,dir;

		tvector3 orig = tvector3(localTransform.GetTranslation());
        
        tvector3 plnorm(m_CamSrc-orig);
            

        plnorm.Normalize();

        // A plane through the GIZMO, facing the camera - the near/far split
        // DrawCircleHalf clips against. It used to be built with w = 0, i.e. a
        // plane through the world origin, which has nothing to do with where
        // the gizmo is.
        tplane plCam = vector4(orig, plnorm);


        dir = orig-m_CamSrc;
        dir.Normalize();

        right.Cross(dir,GetTransformedVector(1));
        right.Normalize();

        up.Cross(dir,right);
        up.Normalize();

        right.Cross(dir,up);
        right.Normalize();

        tvector3 axeX(1,0,0),axeY(0,1,0),axeZ(0,0,1);

        if (mLocation == LOCATE_LOCAL)
        {
            axeX.TransformVector(localTransform);
            axeY.TransformVector(localTransform);
            axeZ.TransformVector(localTransform);
            axeX.Normalize();
            axeY.Normalize();
            axeZ.Normalize();
        }

        // Twin
        if (mMask&AXIS_TRACKBALL)
        {

            if (m_RotateTypePredict != ROTATE_TWIN)
                DrawCircle(orig, 0.2f,0.2f,0.2f,right*GetScreenFactor(),up*GetScreenFactor(),m_Proj,m_Model);
            else
                DrawCircle(orig, 1,1,0,right*GetScreenFactor(),up*GetScreenFactor(),m_Proj,m_Model);
        }

        // Screen
        if (mMask&AXIS_SCREEN)
        {
            if (m_RotateTypePredict != ROTATE_SCREEN)
                DrawCircle(orig, 1.0f,0.3f,1.0f,up*1.2f*GetScreenFactor(),right*1.2f*GetScreenFactor(),m_Proj,m_Model);
            else
                DrawCircle(orig, 1,1,0,up*1.2f*GetScreenFactor(),right*1.2f*GetScreenFactor(),m_Proj,m_Model);
        }

        // X / Y / Z. Each ring is built from the view direction and its own
        // axis rather than from `right`/`up` computed above, and each decides
        // for itself whether it is seen edge-on (draw the near half) or
        // face-on (draw the whole circle) - see BuildRingBasis.
        if (mMask&AXIS_X)
        {
            const bool hot = (m_RotateTypePredict == ROTATE_X);
            DrawRotationRing(orig, dir, axeX, 1.f, hot ? 1.f : 0.f, 0.f,
                GetScreenFactor(), plCam, m_Proj, m_Model);
        }

        if (mMask&AXIS_Y)
        {
            const bool hot = (m_RotateTypePredict == ROTATE_Y);
            DrawRotationRing(orig, dir, axeY, hot ? 1.f : 0.f, 1.f, 0.f,
                GetScreenFactor(), plCam, m_Proj, m_Model);
        }

        if (mMask&AXIS_Z)
        {
            const bool hot = (m_RotateTypePredict == ROTATE_Z);
            if (hot)
                DrawRotationRing(orig, dir, axeZ, 1.f, 1.f, 0.f,
                    GetScreenFactor(), plCam, m_Proj, m_Model);
            else
                DrawRotationRing(orig, dir, axeZ, 0.25f, 0.55f, 1.f,
                    GetScreenFactor(), plCam, m_Proj, m_Model);
        }
        // camembert
        if ( (m_RotateType != ROTATE_NONE) && (m_RotateType != ROTATE_TWIN ) )
            DrawCamem(orig,m_Vtx*GetScreenFactor(),m_Vty*GetScreenFactor(),-m_Ng2,m_Proj,m_Model);
    }


}

void CGizmoTransformRotate::ApplyTransform(tvector3& trans, bool bAbsolute)
{
    tmatrix mt;
    m_OrigScale.Scaling(GetTransformedVector(0).Length(),
        GetTransformedVector(1).Length(),
        GetTransformedVector(2).Length());

    if (bAbsolute)
    {
        tvector3 translation = m_pMatrix->GetTranslation();

        //X
        mt.RotationAxis(GetVector(0),((trans.x/360)*ZPI));
        mt.Multiply(m_OrigScale);
        *m_pMatrix=mt;
        //Y
        mt.RotationAxis(GetVector(1),((trans.y/360)*ZPI));
        mt.Multiply(m_OrigScale);
        *m_pMatrix=mt;
        //Z
        mt.RotationAxis(GetVector(2),((trans.z/360)*ZPI));
        mt.Multiply(m_OrigScale);
        *m_pMatrix=mt;

        //translate
        m_pMatrix->m16[12] = translation.x;
        m_pMatrix->m16[13] = translation.y;
        m_pMatrix->m16[14] = translation.z;
    }
    else
    {
        tmatrix mt2;
        m_InvOrigScale.Inverse(m_OrigScale);

        if (trans.x!=0)
        {
            m_svgMatrix = *m_pMatrix;
            mt.RotationAxis(GetVector(0),((trans.x/360)*ZPI));
            mt.Multiply(m_InvOrigScale);
            mt.Multiply(m_svgMatrix);
            mt2 = m_OrigScale;
            mt2.Multiply(mt);
            *m_pMatrix=mt2;
        }
        if (trans.y!=0)
        {
            m_svgMatrix = *m_pMatrix;
            mt.RotationAxis(GetVector(1),((trans.y/360)*ZPI));
            mt.Multiply(m_InvOrigScale);
            mt.Multiply(m_svgMatrix);
            mt2 = m_OrigScale;
            mt2.Multiply(mt);
            *m_pMatrix=mt2;
        }
        if (trans.z!=0)
        {
            m_svgMatrix = *m_pMatrix;
            mt.RotationAxis(GetVector(2),((trans.z/360)*ZPI));
            mt.Multiply(m_InvOrigScale);
            mt.Multiply(m_svgMatrix);
            mt2 = m_OrigScale;
            mt2.Multiply(mt);
            *m_pMatrix=mt2;
        }
    }
}
