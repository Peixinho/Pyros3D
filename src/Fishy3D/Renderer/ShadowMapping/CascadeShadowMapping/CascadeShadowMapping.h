//============================================================================
// Name        : CascadeShadowMapping.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Class to Create the Shadow Mapping Cascades
//============================================================================

#ifndef CASCADESHADOWMAPPING_H
#define CASCADESHADOWMAPPING_H

#include "../../../Core/Math/Math.h"
#include "../../../Core/Projection/Projection.h"
#include "../../../Core/Buffers/FrameBuffer.h"
#include "../../../Components/RenderingComponents/IRenderingComponent.h"
#include <list>

namespace Fishy3D {

    // Maximum Number of Splits
    #define MAX_SPLITS 4
    // Split Weight
    #define SPLIT_WEIGHT 0.75
    // the 0.2f factor is important because we might get artifacts at
    #define CASCADE_FACTOR 0.2

    struct Frustum {

        // Sub Frustum Properties
        float Near;
        float Far;
        float Ratio;
        float Fov;
        vec3 point[8];
		Matrix CropMatrix;

        // SubFrustum Frame Buffer
        SuperSmartPointer<FrameBuffer> fbo;

        void UpdateFrustumPoints(const vec3& position, const vec3& direction)
        {
            vec3 _direction = direction*-1.f;
            vec3 right = _direction.cross(vec3::UP);

            vec3 fc = position + _direction * Far;
            vec3 nc = position + _direction * Near;

            right.normalizeSelf();
            vec3 up = right.cross(_direction).normalize();	

            // these heights and widths are half the heights and widths of
            // the near and far plane rectangles
            float near_height = tan(Fov/2.0f) * Near;
            float near_width = near_height * Ratio;
            float far_height = tan(Fov/2.0f) * Far;
            float far_width = far_height * Ratio;

            point[0] = nc - up*near_height - right*near_width;
            point[1] = nc + up*near_height - right*near_width;
            point[2] = nc + up*near_height + right*near_width;
            point[3] = nc - up*near_height + right*near_width;

            point[4] = fc - up*far_height - right*far_width;
            point[5] = fc + up*far_height - right*far_width;
            point[6] = fc + up*far_height + right*far_width;
            point[7] = fc - up*far_height + right*far_width;
        }

        // this function builds a projection matrix for rendering from the shadow's POV.
        // First, it computes the appropriate z-range and sets an orthogonal projection.
        // Then, it translates and scales it, so that it exactly captures the bounding box
        // of the current frustum slice
        Matrix CreateCropMatrix(const Matrix &viewMatrix, std::list<IRenderingComponent*> rcomps)
        {
                
            float maxX = -10000.0f;
            float maxY = -10000.0f;
            float maxZ;
            float minX =  10000.0f;
            float minY =  10000.0f;
            float minZ;

            vec4 transf;
            transf = viewMatrix * vec4(point[0].x,point[0].y,point[0].z,1.0f);
            minZ = transf.z;
            maxZ = transf.z;

            for(int i=1; i<8; i++)
            {
                transf = viewMatrix * vec4(point[i].x,point[i].y,point[i].z,1.0f);
                if(transf.z > maxZ) maxZ = transf.z;
                if(transf.z < minZ) minZ = transf.z;
            }
            
            for(std::list<IRenderingComponent*>::iterator i=rcomps.begin(); i!=rcomps.end(); i++)
            {
                vec3 pos = vec3((*i)->GetBoundingSphereCenter().x*(*i)->GetOwner()->GetWorldPosition().x,(*i)->GetBoundingSphereCenter().y*(*i)->GetOwner()->GetWorldPosition().y,(*i)->GetBoundingSphereCenter().z*(*i)->GetOwner()->GetWorldPosition().z);
                transf = viewMatrix * vec4(pos.x,pos.y,pos.z, 1.0f);
                if(transf.z + (*i)->GetBoundingSphereRadius() > maxZ) maxZ = transf.z + (*i)->GetBoundingSphereRadius();
            }

            // Make Ortho
            Matrix ortho;
            ortho = ortho.OrthoMatrixRH(-1.0,1.0,-1.0,1.0,-maxZ,-minZ);
            Matrix mvp = ortho * viewMatrix;
            
            // find the extends of the frustum slice as projected in light's homogeneous coordinates        
            for(unsigned i=0; i<8; i++)
            {
                transf = mvp*vec4(point[i].x,point[i].y,point[i].z,1.0f);

                transf.x /= transf.w;
                transf.y /= transf.w;

                if(transf.x > maxX) maxX = transf.x;
                if(transf.x < minX) minX = transf.x;
                if(transf.y > maxY) maxY = transf.y;
                if(transf.y < minY) minY = transf.y;
            }

            float scaleX = 2.0f/(maxX - minX);
            float scaleY = 2.0f/(maxY - minY);
            float offsetX = -0.5f*(maxX + minX)*scaleX;
            float offsetY = -0.5f*(maxY + minY)*scaleY;

            Matrix crop;            
            crop.ScaleX(scaleX);
            crop.ScaleY(scaleY);
            crop.TranslateX(offsetX);
            crop.TranslateY(offsetY);
//            crop = crop.Transpose();

            CropMatrix = crop * ortho;

            return GetCropMatrix();
        }

            Matrix GetCropMatrix()
        {
            return CropMatrix;
        }
    };

    class CascadeShadowMapping {

        // Allow access to protected members
        friend class CastShadows;

    public:
		
        // Constructor
        CascadeShadowMapping();

        // Init Function
        void Init(const float &Fov, const float& Ratio);
        // Split Frustum
        void UpdateSplitDist(const unsigned &splits, const float &nd, const float &fd);
        // Frustum Splits
        Frustum f[MAX_SPLITS];
        // Get Number of Splits
        unsigned GetNumberOfSplits() {return splits;}
        // Get View Matrix of Cascade - The same for all splits
        Matrix GetViewMatrix();

    private:
        unsigned splits;
	
    protected:
        Matrix ViewMatrix;
    };
};

#endif /* CASCADESHADOWMAPPING_H */