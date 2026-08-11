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

#ifndef GIZMOTRANSFORM_H__
#define GIZMOTRANSFORM_H__

#include "GizmoTransformRender.h"
#include "IGizmo.h"
// Using GLAD elsewhere; no need for GLEW here

class CGizmoTransform : public IGizmo , protected CGizmoTransformRender
{
public:

	CGizmoTransform()
	{
		m_pMatrix = NULL;
		m_bUseSnap = false;
        //mCamera = NULL;
        //mTransform = NULL;
        mEditPos =  mEditScale = NULL;
        mEditQT = NULL;
		mMask = AXIS_ALL;
        m_Lng = 1.f;
        mScreenHeight = mScreenWidth = 1;
        m_ScreenFactor = 1;
        mDisplayScale = 1.f;

        Initialize();
	}

	virtual ~CGizmoTransform()
	{
		Destroy();
	}



	virtual void SetEditMatrix(float *pMatrix)
	{
		m_pMatrix = (tmatrix*)pMatrix;
        //mTransform = NULL;

        mEditPos = mEditScale = NULL;
        mEditQT = NULL;
	}
    virtual void SetDisplayScale( float aScale ) { mDisplayScale = aScale; }
    /*
    virtual void SetEditTransform(ZTransform *pTransform)
    {
        mTransform = pTransform;
		if(!pTransform)
			m_pMatrix = NULL;
		else
			m_pMatrix = (tmatrix*)&mTransform->GetLocalMatrix();
        mEditPos = mEditScale = NULL;
        mEditQT = NULL;
    }
    */
    /*
    virtual void SetEditVQS(tvector3* aPosition, tquaternion *aQT, tvector3 *aScale)
    {
        mEditPos = aPosition;
        mEditQT = aQT;
        mEditScale = aScale;

        //mTransform = NULL;
        m_pMatrix = NULL;

        tmatrix p1,p2,p3;
        p1.Identity();
        p2.Identity();
        p3.Identity();
        if (aPosition)
            p1.Translation(*aPosition);
        if (aQT)
            p2.RotationQuaternion(*aQT);
        if (aScale)
            p3.Scaling(*aScale);
        mWorkingMatrix = p3 * p2 * p1;
        m_pMatrix = &mWorkingMatrix;
    }
    */
    virtual void SetScreenDimension( int screenWidth, int screenHeight, bool isPerspective, f32 l, f32 r, f32 b, f32 t)
    {
        mScreenWidth = screenWidth;
        mScreenHeight = screenHeight;
		IsPerspective = isPerspective;
		left = l;
		right = r;
		bottom = b;
		top = t;
    }
	virtual void SetCameraMatrix(const float *Model, const float *Proj)
	{
		m_Model = *(tmatrix*)Model;
		m_Proj = *(tmatrix*)Proj;

		m_invmodel=m_Model;
		m_invmodel.Inverse();

		m_invproj=m_Proj;
		m_invproj.Inverse();

        m_CamSrc = m_invmodel.V4.position;
        m_CamDir = m_invmodel.V4.dir;
        m_CamUp = m_invmodel.V4.up;
	}

	float orthoX, orthoY, orthoZ;
	float orthoDirX, orthoDirY, orthoDirZ;

	// tools
	virtual void SetOrthoMouse(float x, float y, float z, float dirX, float dirY, float dirZ)
	{
		orthoX = x;
		orthoY = y;
		orthoZ = z;
		orthoDirX = dirX;
		orthoDirY = dirY;
		orthoDirZ = dirZ;
	}
	void BuildRay(int x, int y, tvector3 &rayOrigin, tvector3 &rayDir)
	{
        float frameX = (float)mScreenWidth;
        float frameY = (float)mScreenHeight;
        tvector3 screen_space;

        // device space to normalized screen space
		if (IsPerspective)
		{
			screen_space.x = (((2.f * (float)x) / frameX) - 1) / m_Proj.m[0][0];//.right.x;
			screen_space.y = -(((2.f * (float)y) / frameY) - 1) / m_Proj.m[1][1];
			screen_space.z = -1.f;
			// screen space to world space
			rayOrigin = m_invmodel.V4.position;
			rayDir.TransformVector(screen_space, m_invmodel);
			rayDir.Normalize();
		}
		else {
			rayOrigin = tvector3(orthoX, orthoY, orthoZ);
			rayDir = tvector3(orthoDirX, orthoDirY, orthoDirZ);
		}
    }
	
	tvector3 GetVector(int vtID)
	{
		switch (vtID)
		{
		case 0: return tvector3(1,0,0);
		case 1: return tvector3(0,1,0);
		case 2: return tvector3(0,0,1);
		}
		return tvector3(0,0,0);
	}

	tvector3 GetTransformedVector(int vtID)
	{
		tvector3 vt;
		switch (vtID)
		{
		case 0: vt = tvector3(1,0,0); break;
		case 1: vt = tvector3(0,1,0); break;
		case 2: vt = tvector3(0,0,1); break;
		}
        if (mLocation == LOCATE_LOCAL)
        {
			vt.TransformVector(localTransform);
		    vt.Normalize();
		}
		return vt;
	}
	virtual void SetAxisMask(unsigned int mask)
	{
		mMask = mask;
	}
/*
	tvector3 GetTransformedVector(tvector3& vt)
	{
		tmatrix mt = *m_pMatrix;
		mt.NoTrans();
		vt.TransformPoint(mt);
		return vt;
	}
*/
	void ComputeScreenFactor()
	{
        tmatrix viewproj = m_Model * m_Proj;
		tvector4 trf = vector4( localTransform.V4.position.x, localTransform.V4.position.y, localTransform.V4.position.z, 1.f);
        trf.Transform( viewproj );
		float m = 1.0;
		if (!IsPerspective) m = Max(right, top);
	    m_ScreenFactor = mDisplayScale * m * trf.w;
	}

	tplane m_plan;
	tvector3 m_LockVertex;
	float m_Lng;

    tvector3 RayTrace2(const tvector3& rayOrigin, const tvector3& rayDir, const tvector3& norm, const tmatrix& mt, tvector3 trss, bool lockVTNorm = true)
    {
        extern tvector3 ptd;

	    tvector3 df,inters;

		m_plan=vector4(localTransform.GetTranslation(), norm);
	    m_plan.RayInter(inters,rayOrigin,rayDir);
        df.TransformPoint( inters, mt );
        
        df /=GetScreenFactor();
        /*
	    ptd = inters;
	    df = inters - m_pMatrix->GetTranslation();
	    df /=GetScreenFactor();
	    df2 = df;

	    df2.TransformPoint(mt);
	    df2 *= trss;
        */
	    m_LockVertex = df;
        if (lockVTNorm)
        {
	        m_LockVertex.Normalize();
        }
        else
        {
            m_LockVertex = inters;
        }
	    m_Lng = df.Length();

	    return df;
    }

	float GetScreenFactor()
	{
		return m_ScreenFactor;
	}


	// snap
	virtual void UseSnap(bool bUseSnap)
	{
		m_bUseSnap = bUseSnap;
	}

	virtual bool IsUsingSnap()
	{
		return m_bUseSnap;
	}
	//transform
	virtual void ApplyTransform(tvector3& trans, bool bAbsolute) = 0;

    void SetLocation(LOCATION aLocation) { mLocation = aLocation; }
    LOCATION GetLocation() { return mLocation; }

	virtual void SetLocalTransform(float *pMatrix)
	{
		localTransform = (tmatrix)pMatrix;
	}
	virtual void SetGlobalTransform(float *pMatrix)
	{
		globalTransform = (tmatrix)pMatrix;
	}

protected:
	tmatrix *m_pMatrix;
	tmatrix m_Model,m_Proj;
	tmatrix m_invmodel,m_invproj;
	tvector3 m_CamSrc,m_CamDir,m_CamUp;
	unsigned gizmoType;

	tmatrix m_svgMatrix;
	float m_ScreenFactor;
	bool m_bUseSnap;
    float mDisplayScale;
	
    LOCATION mLocation;

    tmatrix mWorkingMatrix; // for dissociated components
    tvector3 *mEditPos, *mEditScale ;
    tquaternion *mEditQT;
	//draw helpers

	unsigned int mMask;
	void SnapIt(float &pos, float &snap)
	{
		float sn = (float)fmod(pos,snap);
		if (fabs(sn)< (snap*0.25f) ) pos-=sn;
		if (fabs(sn)> (snap*0.75f) ) pos=( (pos-sn) + ((sn>0)?snap:-snap) );
	}

    int mScreenWidth, mScreenHeight;
	bool IsPerspective;
	float left;
	float right;
	float bottom;
	float top;

	// Add LocalTransform
	tmatrix localTransform, globalTransform;
};

#endif // !defined(AFX_GIZMOTRANSFORM_H__913D353E_E420_4B1C_95F3_5A0258161651__INCLUDED_)
