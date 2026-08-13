//============================================================================
// Name        : DirectionalLight.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Directional Light
//============================================================================

#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>

namespace p3d {

	void Cascade::UpdateFrustumPoints(const Vec3& position, const Vec3& direction)
	{
		Vec3 _direction = direction *-1.f;

		// Vec3::UP degenerates as a right-vector reference when the camera looks
		// nearly straight up/down (_direction ~parallel to it): the cross product
		// collapses toward zero, the frustum corners become unstable, and the
		// shadow crop box clips out geometry that's actually in view. Use a
		// different reference axis in that case.
		Vec3 upReference = fabs(_direction.dotProduct(Vec3::UP)) > 0.99f ? Vec3(1.f, 0.f, 0.f) : Vec3::UP;
		Vec3 right = _direction.cross(upReference);

		Vec3 fc = position + _direction * Far;
		Vec3 nc = position + _direction * Near;

		right.normalizeSelf();
		Vec3 up = right.cross(_direction).normalize();

		// these heights and widths are half the heights and widths of
		// the near and far plane rectangles
		// Fov is stored in DEGREES (see EnableCastShadows: RADTODEG(projection.Fov)
		// + CASCADE_FACTOR) but tan() needs radians - this silently fed e.g. ~70
		// straight into tan() as if it were radians (~70 rad, many wrapped
		// multiples of 2*PI past the intended angle), giving a wrong, constant
		// frustum width/height scale that had nothing to do with the camera's
		// real FOV.
		f32 fovRad = (f32)DEGTORAD(Fov);
		f32 near_height = tan(fovRad / 2.0f) * Near;
		f32 near_width = near_height * Ratio;
		f32 far_height = tan(fovRad / 2.0f) * Far;
		f32 far_width = far_height * Ratio;

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
	Matrix Cascade::CreateCropMatrix(const Matrix &viewMatrix, std::vector<RenderingMesh*> rcomps)
	{

		f32 maxX = -10000.0f;
		f32 maxY = -10000.0f;
		f32 maxZ;
		f32 minX = 10000.0f;
		f32 minY = 10000.0f;
		f32 minZ;

		Vec4 transf;
		transf = viewMatrix * Vec4(point[0].x, point[0].y, point[0].z, 1.0f);
		minZ = transf.z;
		maxZ = transf.z;

		for (int i = 1; i < 8; i++)
		{
			transf = viewMatrix * Vec4(point[i].x, point[i].y, point[i].z, 1.0f);
			if (transf.z > maxZ) maxZ = transf.z;
			if (transf.z < minZ) minZ = transf.z;
		}

		for (std::vector<RenderingMesh*>::iterator i = rcomps.begin(); i != rcomps.end(); i++)
		{
			// Was multiplying local bounding-sphere-center by world position
			// component-wise (dimensionally nonsensical, and always (0,0,0)
			// for any mesh centered at its local origin - i.e. every
			// primitive shape - regardless of where it actually is in the
			// world). That silently broke the Z-range extension below for
			// any caster not sitting at the world origin: the crop matrix's
			// near/far only ever "saw" the camera frustum's own corners, so
			// casters like a ceiling or floor away from the origin were
			// clipped out of the shadow pass whenever the frustum-corner Z
			// range didn't happen to reach them - view-dependent, matching
			// the "shadow disappears depending on camera angle" report.
			//
			// Also was reading Geometry->GetBoundingSphereCenter/Radius() -
			// Geometry is the per-submesh IGeometry, whose own bounding-sphere
			// fields are never populated (default/zero) for these primitive
			// shapes; the real, correctly-computed aggregate bounding sphere
			// lives on the RenderingComponent itself (copied from the
			// Renderable in its constructor - RenderingComponent.cpp:79-80/
			// 133-134). Radius was silently always 0, so this Z-range
			// extension never actually accounted for a caster's real size,
			// only its center point - matches the convention already used in
			// IRenderer.cpp:97's LOD-distance calc, which reads off the
			// RenderingComponent, not the Geometry.
			RenderingComponent* rc = (*i)->renderingComponent;
			GameObject* owner = rc->GetOwner();
			Vec3 scale = owner->GetScale();
			Vec3 pos = owner->GetWorldPosition() + rc->GetBoundingSphereCenter() * scale;
			transf = viewMatrix * Vec4(pos.x, pos.y, pos.z, 1.0f);
			// Scale the radius by the largest axis so the sphere still
			// conservatively bounds the mesh under non-uniform scale.
			f32 scaledRadius = rc->GetBoundingSphereRadius() * Max(Max(scale.x, scale.y), scale.z);
			if (transf.z + scaledRadius > maxZ) maxZ = transf.z + scaledRadius;
		}

		// Make Ortho
		ortho.Ortho(-1, 1, -1, 1, -maxZ, -minZ);

		Matrix mvp = ortho.GetProjectionMatrix() * viewMatrix;

		// find the extends of the frustum slice as projected in light's homogeneous coordinates        
		for (unsigned i = 0; i < 8; i++)
		{
			transf = mvp*Vec4(point[i].x, point[i].y, point[i].z, 1.0f);

			transf.x /= transf.w;
			transf.y /= transf.w;

			if (transf.x > maxX) maxX = transf.x;
			if (transf.x < minX) minX = transf.x;
			if (transf.y > maxY) maxY = transf.y;
			if (transf.y < minY) minY = transf.y;
		}

		f32 scaleX = 2.0f / (maxX - minX);
		f32 scaleY = 2.0f / (maxY - minY);
		f32 offsetX = -0.5f*(maxX + minX)*scaleX;
		f32 offsetY = -0.5f*(maxY + minY)*scaleY;

		Matrix crop;
		crop.ScaleX(scaleX);
		crop.ScaleY(scaleY);
		crop.TranslateX(offsetX);
		crop.TranslateY(offsetY);

		CropMatrix = crop * ortho.GetProjectionMatrix();

		return CropMatrix;
	}

	Matrix Cascade::GetCropMatrix()
	{
		return CropMatrix;
	}

	void DirectionalLight::EnableCastShadows(const uint32 Width, const uint32 Height, const Projection &projection, const f32 Near, const f32 Far, const uint32 Cascades)
	{
		// Was wrapped in if (!isCastingShadows), which made this a silent
		// no-op when called on a light that was already casting - so nothing
		// could change a live shadow map's resolution, range or cascade
		// count without disabling it first. Reconfiguration is now the
		// supported path; release the old FBO/map (waiting for the GPU, see
		// ReleaseShadowResources) before building the replacements.
		if (isCastingShadows) ReleaseShadowResources();

		// Save Dimensions
		uint32 ShadowWidthFBO = Width;
		uint32 ShadowHeightFBO = Height;
		ShadowWidth = Width;
		ShadowHeight = Height;

		// Set Flag
		isCastingShadows = true;

		// Initiate FBO (releases any previously owned FBO/texture first)
		shadowsFBO.reset(new FrameBuffer());

		// Minimum 1 and Maximum is 4 Cascades
		if (Cascades <= 0) ShadowCascades = 1;
		else if (Cascades >= 4) ShadowCascades = 4;
		else ShadowCascades = Cascades;

		// Set FBO Dimensions
		if (ShadowCascades > 1)
		{
			ShadowWidthFBO = Width * 2;
			ShadowHeightFBO = Height * 2;
		}

		ShadowMap.reset(new Texture());

#if defined(GLLEGACY)

		// Create Texture, Frame Buffer and Set the Texture as Attachment
		ShadowMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, ShadowWidthFBO, ShadowHeightFBO, false);
		ShadowMap->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);

		// Initialize Frame Buffer
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, ShadowWidth, ShadowHeight);
		shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, ShadowMap.get());
#else
		// GPU Shadow Maps
		// Create Texture, Frame Buffer and Set the Texture as Attachment
		ShadowMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, ShadowWidthFBO, ShadowHeightFBO, false);
		ShadowMap->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
		// CreateEmptyTexture defaults to Linear; depth+compare samplers with
		// Linear are "unloadable" on Apple GL (and then sampler2DShadow
		// falls back to zero → shadows disappear / scene looks brighter).
		// PCF is done in the shader with multiple taps, so Nearest is correct.
		ShadowMap->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		ShadowMap->EnableCompareMode();
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, ShadowMap.get());
#endif
		// Near and Far Clip Planes
		ShadowNear = Near;
		ShadowFar = Far;

		// View Projection Matrix
		ShadowProjection = projection.m;

		for (uint32 i = 0; i < MAX_SPLITS; i++)
		{
			// projection.Fov is already in degrees (Projection::Perspective
			// stores its raw `fov` parameter with no conversion, and every
			// caller - e.g. the examples' projection.Perspective(70.f,...) -
			// passes degrees). This used to run it through RADTODEG() anyway,
			// as if it were radians, producing a bogus ~4011-degree value
			// (RADTODEG(70) = 70*180/PI) instead of ~70 - which, combined
			// with Cascade::UpdateFrustumPoints correctly converting back to
			// radians before tan(), aliased through tan()'s periodicity into
			// a wrong-but-plausible-looking, constant ~32% undersized
			// frustum half-angle. That silently made the shadow crop box
			// tighter than the camera's real frustum on every frame,
			// clipping real on-screen geometry near the edges of view.
			this->Cascades[i].Fov = projection.Fov + CASCADE_FACTOR;
			this->Cascades[i].Ratio = projection.Aspect;
		}

		// Build Frustums
		f32 lambda = SPLIT_WEIGHT;
		f32 ratio = Far / Near;
		this->Cascades[0].Near = Near;
		this->Cascades[0].Width = (f32)Width;
		this->Cascades[0].Height = (f32)Height;

		for (uint32 i = 1; i < ShadowCascades; i++)
		{
			f32 si = (f32)i / (f32)ShadowCascades;

			this->Cascades[i].Near = lambda*(Near*powf(ratio, si)) + (1.f - lambda)*(Near + (Far - Near)*si);
			this->Cascades[i - 1].Far = this->Cascades[i].Near * 1.005f;
			this->Cascades[i].Width = (f32)Width;
			this->Cascades[i].Height = (f32)Height;
		}
		// Set Far on Last Split
		this->Cascades[ShadowCascades - 1].Far = Far;
		this->Cascades[ShadowCascades - 1].Width = (f32)Width;
		this->Cascades[ShadowCascades - 1].Height = (f32)Height;
	}

	Matrix DirectionalLight::GetLightProjection(const Matrix &ShadowViewMatrix, const uint32 Cascade, const std::vector<RenderingMesh*> RCompList)
	{
		return Cascades[Cascade].CreateCropMatrix(ShadowViewMatrix, RCompList);
	}

	void DirectionalLight::UpdateCascadeFrustumPoints(const uint32 Cascade, const Vec3 &CameraPosition, const Vec3 &CameraDirection)
	{
		Cascades[Cascade].UpdateFrustumPoints(CameraPosition, CameraDirection);
	}
}