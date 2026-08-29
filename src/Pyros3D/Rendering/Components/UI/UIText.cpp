//============================================================================
// Name        : UIText
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A line of text aligned inside a canvas rect
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

namespace p3d {

	// Built ahead of the base RenderingComponent constructor, which needs
	// the renderable and the material before UIText's own body can run.
	static std::shared_ptr<IMaterial> MakeUITextMaterial(Font* font)
	{
		GenericShaderMaterial* mat = new GenericShaderMaterial(ShaderUsage::TextRendering);
		mat->SetTextFont(font);
		// Same screen-space contract as UIImage's material - see there.
		// Glyph atlases are single-channel coverage, so blending is not
		// optional here the way it is for an opaque quad.
		mat->SetTransparencyFlag(true);
		mat->EnableBlending();
		mat->BlendingEquation(BlendEq::Add);
		mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		mat->DisableDepthTest();
		mat->DisableDepthWrite();
		mat->SetCullFace(CullFace::DoubleSided);
		mat->DisableCastingShadows();
		return std::shared_ptr<IMaterial>(mat);
	}

	UIText::UIText(const std::shared_ptr<Font> &font, const std::string &text,
		const f32 size, const Vec4 &color)
		: RenderingComponent(std::make_shared<Text>(font.get(), text, size, size, color),
		                     MakeUITextMaterial(font.get()))
	{
		this->font = font;
		this->text = text;
		this->color = color;
		this->size = size;
		this->align = UIAlign::Left;
		this->verticalAlign = UIVerticalAlign::Top;
		this->solved = false;
		this->lastPivot = Vec2(0.5f, 0.5f);

		SetRenderLayer(RenderLayer::UI);
		DisableCullTest();
		DisableCastShadows();
	}

	UIText::~UIText() {}

	void UIText::SetText(const std::string &text)
	{
		if (this->text == text) return;
		this->text = text;
		// CreateText() first: UpdateText() reads glyph metrics straight out
		// of the atlas, so any character this Font has never baked would
		// otherwise come back as a zero-size glyph.
		font->CreateText(text);
		static_cast<Text*>(GetRenderable())->UpdateText(text, color);
		Realign();
	}

	void UIText::SetColor(const Vec4 &color)
	{
		this->color = color;
		// Text carries per-character colour in the normal attribute (see
		// Text::UpdateText), so a colour change is a mesh rebuild - and
		// UpdateText early-outs when the string is unchanged, so the text
		// has to be cleared first for the new colour to take.
		Text* t = static_cast<Text*>(GetRenderable());
		t->UpdateText(std::string(), color);
		font->CreateText(text);
		t->UpdateText(text, color);
		Realign();
	}

	void UIText::SetSize(const f32 size)
	{
		if (this->size == size) return;
		this->size = size;
		static_cast<Text*>(GetRenderable())->SetCharSize(size, size);
		Realign();
	}

	void UIText::SetAlignment(const uint32 horizontal, const uint32 vertical)
	{
		align = horizontal;
		verticalAlign = vertical;
		Realign();
	}

	void UIText::OnRectSolved(const UIRectValue &rect, const Vec2 &pivot)
	{
		lastRect = rect;
		lastPivot = pivot;
		solved = true;
		Realign();
	}

	void UIText::Realign()
	{
		if (!solved) return;
		std::vector<RenderingMesh*> &meshes = GetMeshes();
		if (meshes.empty()) return;

		Renderable* r = GetRenderable();
		const Vec3 minB = r->GetBoundingMinValue();
		const Vec3 maxB = r->GetBoundingMaxValue();

		// The rect, in the element's own local space: the owner sits at the
		// rect's pivot, and local y is up while canvas y is down.
		const f32 left = -lastPivot.x * lastRect.width;
		const f32 right = left + lastRect.width;
		const f32 top = lastPivot.y * lastRect.height;
		const f32 bottom = top - lastRect.height;

		f32 dx = 0.f, dy = 0.f;
		switch (align)
		{
		case UIAlign::Center: dx = (left + right) * 0.5f - (minB.x + maxB.x) * 0.5f; break;
		case UIAlign::Right:  dx = right - maxB.x; break;
		default:              dx = left - minB.x; break;
		}
		switch (verticalAlign)
		{
		case UIVerticalAlign::Middle: dy = (top + bottom) * 0.5f - (minB.y + maxB.y) * 0.5f; break;
		case UIVerticalAlign::Bottom: dy = bottom - minB.y; break;
		default:                      dy = top - maxB.y; break;
		}

		// Alignment rides on the mesh Pivot rather than the owner's
		// position, because the position belongs to UIRect - two writers on
		// one transform is exactly the kind of thing that produces layouts
		// that drift by a frame.
		Matrix pivotMatrix;
		pivotMatrix.identity();
		pivotMatrix.Translate(dx, dy, 0.f);
		for (size_t i = 0; i < meshes.size(); i++)
			meshes[i]->Pivot = pivotMatrix;
	}

};
