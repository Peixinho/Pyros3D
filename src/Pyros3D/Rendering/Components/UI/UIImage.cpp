//============================================================================
// Name        : UIImage
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Textured, tinted, optionally 9-sliced quad on a canvas
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

namespace p3d {

	UIQuad::UIQuad() : Primitive()
	{
		Rebuild(1.f, 1.f, Vec2(0.5f, 0.5f), Vec4(0.f, 0.f, 0.f, 0.f), Vec2(1.f, 1.f));
	}

	void UIQuad::Rebuild(const f32 width, const f32 height, const Vec2 &pivot,
		const Vec4 &border, const Vec2 &textureSize)
	{
		// Same in-place rebuild Text::UpdateText() does - Dispose() the GPU
		// buffers, clear the CPU arrays, refill, Build(). IGeometry bumps
		// buffersRevision on Dispose, which is what makes RenderingMesh
		// throw away VAOs/pipelines still pointing at the old buffers.
		if (!Geometries.empty())
		{
			geometry->Dispose();
			geometry->index.clear();
			geometry->tVertex.clear();
			geometry->tNormal.clear();
			geometry->tTexcoord.clear();
			Geometries.clear();
		}

		// Local space: the owner GameObject sits at the pivot (that is what
		// UIRect wrote), and local y is up while canvas y is down - so the
		// rect spans downwards from +pivot.y*height.
		const f32 x0 = -pivot.x * width;
		const f32 x1 = x0 + width;
		const f32 y0 = pivot.y * height;
		const f32 y1 = y0 - height;

		// Border is clamped so a 9-slice never inverts on an element
		// smaller than its own frame - it just stops slicing, which looks
		// squashed rather than turning inside out.
		const f32 bl = (border.x + border.z) > width  ? width  * (border.x / (border.x + border.z)) : border.x;
		const f32 br = (border.x + border.z) > width  ? width  - bl : border.z;
		const f32 bt = (border.y + border.w) > height ? height * (border.y / (border.y + border.w)) : border.y;
		const f32 bb = (border.y + border.w) > height ? height - bt : border.w;

		const bool sliced = (border.x > 0.f || border.y > 0.f || border.z > 0.f || border.w > 0.f);

		// Column/row edges in local space, and the matching UV edges. v
		// runs 0 at the top so it lines up with canvas space.
		f32 px[4], py[4], u[4], v[4];
		if (sliced)
		{
			px[0] = x0; px[1] = x0 + bl; px[2] = x1 - br; px[3] = x1;
			py[0] = y0; py[1] = y0 - bt; py[2] = y1 + bb; py[3] = y1;
			const f32 tw = textureSize.x > 0.f ? textureSize.x : 1.f;
			const f32 th = textureSize.y > 0.f ? textureSize.y : 1.f;
			u[0] = 0.f; u[1] = border.x / tw; u[2] = 1.f - border.z / tw; u[3] = 1.f;
			v[0] = 0.f; v[1] = border.y / th; v[2] = 1.f - border.w / th; v[3] = 1.f;
		}
		else
		{
			px[0] = x0; px[1] = x0; px[2] = x1; px[3] = x1;
			py[0] = y0; py[1] = y0; py[2] = y1; py[3] = y1;
			u[0] = 0.f; u[1] = 0.f; u[2] = 1.f; u[3] = 1.f;
			v[0] = 0.f; v[1] = 0.f; v[2] = 1.f; v[3] = 1.f;
		}

		const Vec3 normal(0.f, 0.f, 1.f);
		const uint32 cols = sliced ? 3 : 1;
		const uint32 rows = sliced ? 3 : 1;
		uint32 quads = 0;

		for (uint32 r = 0; r < rows; r++)
		{
			for (uint32 c = 0; c < cols; c++)
			{
				const uint32 c0 = sliced ? c : 0, c1 = sliced ? c + 1 : 3;
				const uint32 r0 = sliced ? r : 0, r1 = sliced ? r + 1 : 3;

				geometry->tVertex.push_back(Vec3(px[c0], py[r1], 0.f));
				geometry->tVertex.push_back(Vec3(px[c1], py[r1], 0.f));
				geometry->tVertex.push_back(Vec3(px[c1], py[r0], 0.f));
				geometry->tVertex.push_back(Vec3(px[c0], py[r0], 0.f));

				geometry->tTexcoord.push_back(Vec2(u[c0], v[r1]));
				geometry->tTexcoord.push_back(Vec2(u[c1], v[r1]));
				geometry->tTexcoord.push_back(Vec2(u[c1], v[r0]));
				geometry->tTexcoord.push_back(Vec2(u[c0], v[r0]));

				for (uint32 i = 0; i < 4; i++) geometry->tNormal.push_back(normal);

				geometry->index.push_back(quads * 4 + 0);
				geometry->index.push_back(quads * 4 + 1);
				geometry->index.push_back(quads * 4 + 2);
				geometry->index.push_back(quads * 4 + 2);
				geometry->index.push_back(quads * 4 + 3);
				geometry->index.push_back(quads * 4 + 0);
				quads++;
			}
		}

		Build();

		minBounds = Vec3(x0, y1, 0.f);
		maxBounds = Vec3(x1, y0, 0.f);
		BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
		BoundingSphereRadius = minBounds.distance(BoundingSphereCenter);
	}

	const std::shared_ptr<Texture> &UIImage::WhiteTexture()
	{
		// Function-local static, not a file-scope one: a Texture owns a GPU
		// object, and a file-scope static would be destroyed from
		// __cxa_finalize after the render device is already gone.
		static std::shared_ptr<Texture> white;
		if (!white)
		{
			white = std::make_shared<Texture>();
			white->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, 1, 1, false);
			uchar pixel[4] = { 255, 255, 255, 255 };
			white->UpdateData(pixel);
			white->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
			white->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		}
		return white;
	}

	// Built once here so the base RenderingComponent constructor has
	// something to take - it needs the renderable and the material before
	// the body of UIImage's own constructor can run.
	static std::shared_ptr<IMaterial> MakeUIMaterial(const Vec4 &tint)
	{
		GenericShaderMaterial* mat = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Texture);
		mat->SetColor(tint);
		mat->SetColorMap(UIImage::WhiteTexture());
		// Unlit by construction: Diffuse/PBR are the flags that pull
		// lighting in, and neither is set. The rest is what makes a
		// screen-space quad behave - alpha blended, no depth interaction at
		// all, and visible from either side so a flipped parent scale still
		// draws.
		mat->EnableBlending();
		mat->BlendingEquation(BlendEq::Add);
		mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		mat->DisableDepthTest();
		mat->DisableDepthWrite();
		mat->SetTransparencyFlag(true);
		mat->SetCullFace(CullFace::DoubleSided);
		mat->DisableCastingShadows();
		return std::shared_ptr<IMaterial>(mat);
	}

	UIImage::UIImage(const Vec4 &tint)
		: RenderingComponent(std::make_shared<UIQuad>(), MakeUIMaterial(tint))
	{
		this->tint = tint;
		this->displayTint = tint;
		this->border = Vec4(0.f, 0.f, 0.f, 0.f);
		this->texture = WhiteTexture();
		builtWidth = builtHeight = 0.f;
		builtPivot = Vec2(0.5f, 0.5f);
		builtBorder = border;
		built = false;

		// Kept out of the 3D pass - see RenderLayer in RenderingComponent.h.
		SetRenderLayer(RenderLayer::UI);
		// A screen-space quad is never behind anything, so frustum culling
		// it against the scene camera is both meaningless and wrong.
		DisableCullTest();
		DisableCastShadows();
	}

	UIImage::~UIImage() {}

	void UIImage::SetTint(const Vec4 &tint)
	{
		// Authored and displayed move together here - this is the value an
		// author set, so it is also what should be on screen until something
		// (a button state) says otherwise.
		this->tint = tint;
		SetDisplayTint(tint);
	}

	void UIImage::SetDisplayTint(const Vec4 &tint)
	{
		displayTint = tint;
		std::vector<RenderingMesh*> &meshes = GetMeshes();
		for (size_t i = 0; i < meshes.size(); i++)
			if (meshes[i]->Material)
				static_cast<GenericShaderMaterial*>(meshes[i]->Material.get())->SetColor(tint);
	}

	void UIImage::SetTexture(const std::shared_ptr<Texture> &texture)
	{
		this->texture = texture ? texture : WhiteTexture();
		std::vector<RenderingMesh*> &meshes = GetMeshes();
		for (size_t i = 0; i < meshes.size(); i++)
			if (meshes[i]->Material)
				static_cast<GenericShaderMaterial*>(meshes[i]->Material.get())->SetColorMap(this->texture);
		// A 9-sliced quad's UVs are derived from the texture's pixel size,
		// so a different texture is a different mesh.
		built = false;
	}

	void UIImage::SetBorder(const Vec4 &border)
	{
		this->border = border;
		built = false;
	}

	void UIImage::OnRectSolved(const UIRectValue &rect, const Vec2 &pivot)
	{
		if (built &&
			rect.width == builtWidth && rect.height == builtHeight &&
			pivot.x == builtPivot.x && pivot.y == builtPivot.y &&
			border.x == builtBorder.x && border.y == builtBorder.y &&
			border.z == builtBorder.z && border.w == builtBorder.w)
			return;

		UIQuad* q = static_cast<UIQuad*>(GetRenderable());
		q->Rebuild(rect.width, rect.height, pivot, border,
			Vec2((f32)texture->GetWidth(), (f32)texture->GetHeight()));

		builtWidth = rect.width;
		builtHeight = rect.height;
		builtPivot = pivot;
		builtBorder = border;
		built = true;
	}

};
