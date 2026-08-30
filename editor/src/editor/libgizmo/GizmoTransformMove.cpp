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

#include "GizmoTransformMove.h"

IGizmo *CreateMoveGizmo()
{
    return new CGizmoTransformMove;
}

tvector3 ptd;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CGizmoTransformMove::CGizmoTransformMove() : CGizmoTransform()
{
	m_MoveType = MOVE_NONE;
	gizmoType = 1;
}

CGizmoTransformMove::~CGizmoTransformMove()
{

}

tvector3 CGizmoTransformMove::RayTrace(tvector3& rayOrigin, tvector3& rayDir, tvector3& norm)
{
	tvector3 df,inters;
	m_plan=vector4(localTransform.GetTranslation(), norm);
	m_plan.RayInter(inters,rayOrigin,rayDir);
	ptd = inters;
	df = inters - localTransform.GetTranslation();
	df /=GetScreenFactor();
	m_LockVertex = inters;
	return df;
}

bool CGizmoTransformMove::GetOpType(MOVETYPE &type, unsigned int x, unsigned int y)
{
	tvector3 rayOrigin, rayDir, df;
	BuildRay(x, y, rayOrigin, rayDir);
	m_svgMatrix = *m_pMatrix;


	tvector3 trss(GetTransformedVector(0).Length(),
		GetTransformedVector(1).Length(),
		GetTransformedVector(2).Length());

	tmatrix mt;
    if (mLocation == LOCATE_LOCAL)
    {
		mt = localTransform;
	    mt.Inverse();
    }
    else
    {
        // world
		mt.Translation( -localTransform.V4.position);
    }

	// plan 1 : X/Z
	df = RayTrace2(rayOrigin, rayDir, GetTransformedVector(1), mt, trss, false);

	// Same mask as the draw above - an axis that is not drawn must not be
	// pickable, or the gizmo answers clicks with an axis the user cannot see.
	const bool pickX = (mMask & AXIS_X) != 0;
	const bool pickY = (mMask & AXIS_Y) != 0;
	const bool pickZ = (mMask & AXIS_Z) != 0;

	if ( pickX && ( df.x >= 0 ) && (df.x <= 1) && ( fabs(df.z) < 0.1f ) ) { type = MOVE_X; return true; }
	else if ( pickZ && ( df.z >= 0 ) && (df.z <= 1) && ( fabs(df.x) < 0.1f ) ){ type = MOVE_Z; return true; }
	else if ( pickX && pickZ && (df.x<0.5f) && (df.z<0.5f) && (df.x>0) && (df.z>0)) { type = MOVE_XZ; return true; }
	else {

		//plan 2 : X/Y
		df = RayTrace2(rayOrigin, rayDir, GetTransformedVector(2), mt, trss, false);

		if ( pickX && ( df.x >= 0 ) && (df.x <= 1) && ( fabs(df.y) < 0.1f ) ) { type = MOVE_X; return true; }
		if ( pickY && ( df.y >= 0 ) && (df.y <= 1) && ( fabs(df.x) < 0.1f ) ) { type = MOVE_Y; return true; }
		else if ( pickX && pickY && (df.x<0.5f) && (df.y<0.5f) && (df.x>0) && (df.y>0)) { type = MOVE_XY; return true; }
		else
		{
			//plan 3: Y/Z
			df = RayTrace2(rayOrigin, rayDir, GetTransformedVector(0), mt, trss, false);

			if ( pickY && ( df.y >= 0 ) && (df.y <= 1) && ( fabs(df.z) < 0.1f ) ) { type = MOVE_Y; return true; }
			else if ( pickZ && ( df.z >= 0 ) && (df.z <= 1) && ( fabs(df.y) < 0.1f ) ) { type = MOVE_Z; return true; }
			else if ( pickY && pickZ && (df.y<0.5f) && (df.z<0.5f) && (df.y>0) && (df.z>0)) { type = MOVE_YZ; return true; }

		}
	}

	type = MOVE_NONE;
	return false;
}


bool CGizmoTransformMove::OnMouseDown(unsigned int x, unsigned int y)
{
	if (m_pMatrix)
	{
		return GetOpType(m_MoveType, x, y);
	}

	m_MoveType = MOVE_NONE;
	return false;
}


void CGizmoTransformMove::OnMouseMove(unsigned int x, unsigned int y)
{
	if (m_MoveType != MOVE_NONE)
	{
		tvector3 rayOrigin,rayDir,df, inters;

		BuildRay(x, y, rayOrigin, rayDir);
		m_plan.RayInter(inters,rayOrigin,rayDir);

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

        df = inters - m_LockVertex;

		switch (m_MoveType)
		{
			case MOVE_XZ:	df = tvector3(df.Dot(axeX) , 0,df.Dot(axeZ));		break;
			case MOVE_X:	df = tvector3(df.Dot(axeX) , 0,0);							break;
			case MOVE_Z:	df = tvector3(0, 0,df.Dot(axeZ));							break;
			case MOVE_XY:	df = tvector3(df.Dot(axeX) ,df.Dot(axeY), 0);		break;
			case MOVE_YZ:	df = tvector3(0,df.Dot(axeY) ,df.Dot(axeZ));		break;
			case MOVE_Y:	df = tvector3(0,df.Dot(axeY), 0);							break;
		}

        tvector3 adf;

		if (m_bUseSnap)
		{
			SnapIt(df.x,m_MoveSnap.x);
			SnapIt(df.y,m_MoveSnap.y);
			SnapIt(df.z,m_MoveSnap.z);
		}

        adf = df.x*axeX + df.y*axeY + df.z*axeZ;

		tmatrix ilocaltransform = globalTransform;
		ilocaltransform.Inverse();
		adf.TransformVector(ilocaltransform);

		// Add the delta to the translation only. Pre-multiplying the full
		// start matrix (svg * T(delta)) rotated the delta by the object's
		// own rotation, so a rotated object - and a child of a rotated
		// parent - moved along R*delta instead of tracking the mouse.
		*m_pMatrix = m_svgMatrix;
		tvector3 newPos = m_pMatrix->GetTranslation();
		newPos += adf;
		m_pMatrix->V4.position.x = newPos.x;
		m_pMatrix->V4.position.y = newPos.y;
		m_pMatrix->V4.position.z = newPos.z;
        //if (mTransform) mTransform->Update();

        if (mEditPos)
            *mEditPos = m_pMatrix->V4.position;
	}
	else
	{
		// predict move
		if (m_pMatrix)
		{
			GetOpType(m_MoveTypePredict, x, y);
		}
	}

}

void CGizmoTransformMove::OnMouseUp(unsigned int x, unsigned int y)
{
	m_MoveType = MOVE_NONE;
}

void CGizmoTransformMove::Draw()
{
	ComputeScreenFactor();

	if (m_pMatrix)
	{

		tvector3 orig = localTransform.GetTranslation();

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

		// SetAxisMask() was honoured by the rotate gizmo only. A 2D scene
		// masks Z off (draw order is not something you drag), and without
		// this the blue Z arrow still drew - pointing straight at an
		// orthographic camera, so it rendered as a dot on the origin that
		// still swallowed clicks meant for X or Y.
		const bool useX = (mMask & AXIS_X) != 0;
		const bool useY = (mMask & AXIS_Y) != 0;
		const bool useZ = (mMask & AXIS_Z) != 0;

		if (useX && useZ) DrawQuad(orig, 0.5f*GetScreenFactor(), (m_MoveTypePredict == MOVE_XZ), axeX, axeZ,m_Proj,m_Model);
		if (useX && useY) DrawQuad(orig, 0.5f*GetScreenFactor(), (m_MoveTypePredict == MOVE_XY), axeX, axeY,m_Proj,m_Model);
		if (useY && useZ) DrawQuad(orig, 0.5f*GetScreenFactor(), (m_MoveTypePredict == MOVE_YZ), axeY, axeZ,m_Proj,m_Model);

		axeX*=GetScreenFactor();
		axeY*=GetScreenFactor();
		axeZ*=GetScreenFactor();

		// plan1
		if (useX) {
		if (m_MoveTypePredict != MOVE_X) DrawAxis(orig,axeX,axeY,axeZ,0.05f,0.83f,vector4(1,0,0,1), m_Proj, m_Model);
			else DrawAxis(orig,axeX,axeY,axeZ, 0.05f,0.83f,vector4(1,1,0,1), m_Proj, m_Model);
		}

		//plan2
		if (useY) {
		if (m_MoveTypePredict != MOVE_Y) DrawAxis(orig,axeY,axeX,axeZ, 0.05f,0.83f,vector4(0,1,0,1), m_Proj, m_Model);
			else DrawAxis(orig,axeY,axeX,axeZ, 0.05f,0.83f,vector4(1,1,0,1), m_Proj, m_Model);
		}

		//plan3
		if (useZ) {
		if (m_MoveTypePredict != MOVE_Z) DrawAxis(orig,axeZ,axeX,axeY, 0.05f,0.83f,vector4(0,0,1,1), m_Proj, m_Model);
			else DrawAxis(orig,axeZ,axeX,axeY, 0.05f,0.83f,vector4(1,1,0,1), m_Proj, m_Model);
		}
	}

}

void CGizmoTransformMove::ApplyTransform(tvector3& trans, bool bAbsolute)
{
	if (bAbsolute)
	{
		m_pMatrix->m16[12] = trans.x;
		m_pMatrix->m16[13] = trans.y;
		m_pMatrix->m16[14] = trans.z;
	}
	else
	{
		m_pMatrix->m16[12] += trans.x;
		m_pMatrix->m16[13] += trans.y;
		m_pMatrix->m16[14] += trans.z;
	}
}



