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
		this->displayColor = color;
		this->size = size;
		this->align = UIAlign::Left;
		this->verticalAlign = UIVerticalAlign::Top;
		this->wordWrap = false;
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
		SetDisplayColor(color);
	}

	void UIText::SetDisplayColor(const Vec4 &color)
	{
		displayColor = color;
		// Text carries colour per vertex, so this rebuilds the mesh; the
		// early-out in UpdateText tests the colour as well as the string.
		static_cast<Text*>(GetRenderable())->UpdateText(text, color);
		Realign();
	}

	void UIText::SetSize(const f32 size)
	{
		if (this->size == size) return;
		this->size = size;
		static_cast<Text*>(GetRenderable())->SetCharSize(size, size);
		Realign();
	}

	void UIText::SetWordWrap(bool on)
	{
		if (wordWrap == on) return;
		wordWrap = on;
		// Zero turns wrapping off in Text; the rect width turns it on. The
		// width is re-asserted on every solve below, so this only has to get
		// the current frame right.
		static_cast<Text*>(GetRenderable())->SetWrapWidth(on ? lastRect.width : 0.f);
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
		// Wrapping is a function of the rect, so a resized element re-wraps.
		// SetWrapWidth early-outs when the width has not moved, so this is
		// free on the frames where nothing changed.
		if (wordWrap)
			static_cast<Text*>(GetRenderable())->SetWrapWidth(rect.width);
		Realign();
	}

	void UIText::Realign()
	{
		if (!solved) return;
		std::vector<RenderingMesh*> &meshes = GetMeshes();
		if (meshes.empty()) return;

		Text* t = static_cast<Text*>(GetRenderable());

		// The rect, in the element's own local space: the owner sits at the
		// rect's pivot, and local y is up while canvas y is down.
		const f32 left = -lastPivot.x * lastRect.width;
		const f32 right = left + lastRect.width;
		const f32 top = lastPivot.y * lastRect.height;
		const f32 bottom = top - lastRect.height;

		// Aligned by the font's designed box, not by the ink. Text's mesh
		// puts the first line's baseline at y = 0, so the block runs from
		// +ascender down through however many lines there are to
		// -descender - independent of whether this particular string
		// happens to contain a capital or a descender, which is what stops
		// neighbouring labels sitting at different heights.
		const f32 scale = size / t->GetFont()->GetFontSize();
		const f32 blockTop = t->GetFont()->GetAscender() * scale;
		const f32 blockBottom = -(t->GetFont()->GetDescender() * -1.f * scale)
			- t->GetFont()->GetLineHeight() * scale * (f32)(t->GetLineCount() - 1);
		const f32 blockWidth = t->GetAdvanceWidth();

		f32 dx = 0.f, dy = 0.f;
		switch (align)
		{
		case UIAlign::Center: dx = (left + right) * 0.5f - blockWidth * 0.5f; break;
		case UIAlign::Right:  dx = right - blockWidth; break;
		default:              dx = left; break;
		}
		switch (verticalAlign)
		{
		case UIVerticalAlign::Middle: dy = (top + bottom) * 0.5f - (blockTop + blockBottom) * 0.5f; break;
		case UIVerticalAlign::Bottom: dy = bottom - blockBottom; break;
		default:                      dy = top - blockTop; break;
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
