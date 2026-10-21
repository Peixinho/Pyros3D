//============================================================================
// Name        : Text2D
// Description : See Text2D.h.
//============================================================================

#include <Pyros3D/Utils/Localization/Localization.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Text2D.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>

namespace p3d {

	// (the same material a canvas's text has - UIText.cpp - without the
	// canvas: drawn with the scene's translucent things, in z order)
	static std::shared_ptr<IMaterial> MakeText2DMaterial(Font* font)
	{
		const uint32 usage = ShaderUsage::TextRendering | ((font && font->IsSDF()) ? ShaderUsage::TextSDF : 0);
		GenericShaderMaterial* mat = new GenericShaderMaterial(usage);
		mat->SetTextFont(font);
		mat->SetTransparencyFlag(true);
		mat->EnableBlending();
		mat->BlendingEquation(BlendEq::Add);
		mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		mat->DisableDepthWrite();
		mat->SetCullFace(CullFace::DoubleSided);
		mat->DisableCastingShadows();
		return std::shared_ptr<IMaterial>(mat);
	}

	Text2D::Text2D(const std::shared_ptr<Font> &font, const std::string &text, const f32 size, const Vec4 &color)
		: RenderingComponent(std::make_shared<Text>(font.get(), Localization::Resolve(text), size, size, color), MakeText2DMaterial(font.get()))
	{
		this->font = font;
		this->text = text;
		this->color = color;
		this->size = size;
		this->anchor = Vec2(0.5f, 0.5f);
		DisableCullTest();
		if (color.w < 1.f)
		{
			std::vector<RenderingMesh*> &meshes = GetMeshes(0);
			for (size_t i = 0; i < meshes.size(); i++)
				if (meshes[i] && meshes[i]->Material) meshes[i]->Material->SetOpacity(color.w);
		}
		DisableCastShadows();
		Realign();
		Listen();
	}

	Text2D::~Text2D() { Localization::RemoveListener(this); }

	// "@key" text in the current language, redrawn when it changes - as UIText.
	void Text2D::Listen()
	{
		if (Localization::IsKey(text)) Localization::AddListener(this, [this]() {
			const std::string shown = Localization::Resolve(this->text);
			font->CreateText(shown);
			static_cast<Text*>(GetRenderable())->UpdateText(shown, color);
			Realign();
		});
		else Localization::RemoveListener(this);
	}

	void Text2D::SetText(const std::string &text)
	{
		if (this->text == text) return;
		this->text = text;
		Listen();
		// (the glyphs first: a character the font has not drawn yet has no size)
		const std::string shown = Localization::Resolve(text);
		font->CreateText(shown);
		static_cast<Text*>(GetRenderable())->UpdateText(shown, color);
		Realign();
	}

	void Text2D::SetColor(const Vec4 &color)
	{
		const bool tint = color.x != this->color.x || color.y != this->color.y || color.z != this->color.z;
		this->color = color;
		// Colour is per vertex in a text's mesh - red, green and blue, and
		// no more: changing it rebuilds the mesh. How see-through the text
		// is belongs to its material, so a fade costs nothing (and happens:
		// the mesh had nowhere to put an alpha, and text told to fade did
		// not).
		if (tint)
		{
			static_cast<Text*>(GetRenderable())->UpdateText(Localization::Resolve(text), color);
			Realign();
		}
		std::vector<RenderingMesh*> &meshes = GetMeshes(0);
		for (size_t i = 0; i < meshes.size(); i++)
			if (meshes[i] && meshes[i]->Material) meshes[i]->Material->SetOpacity(color.w);
	}

	void Text2D::SetSize(const f32 size)
	{
		if (this->size == size || !(size > 0.f)) return;
		this->size = size;
		static_cast<Text*>(GetRenderable())->SetCharSize(size, size);
		Realign();
	}

	void Text2D::SetAnchor(const Vec2 &anchor)
	{
		this->anchor = anchor;
		Realign();
	}

	Vec2 Text2D::GetBlockSize() const
	{
		Text* t = static_cast<Text*>(const_cast<Text2D*>(this)->GetRenderable());
		const f32 scale = size / t->GetFont()->GetFontSize();
		const f32 lines = (f32)(t->GetLineCount() > 0 ? t->GetLineCount() : 1);
		return Vec2(t->GetAdvanceWidth(), t->GetFont()->GetLineHeight() * scale * lines);
	}

	void Text2D::Realign()
	{
		std::vector<RenderingMesh*> &meshes = GetMeshes();
		if (meshes.empty()) return;
		Text* t = static_cast<Text*>(GetRenderable());
		// The block by the font's designed box, as a canvas's text is laid
		// out: the first line's baseline is y = 0 in the mesh, the block
		// runs from the ascender above it to the descender under the last.
		const f32 scale = size / t->GetFont()->GetFontSize();
		const f32 top = t->GetFont()->GetAscender() * scale;
		const f32 bottom = t->GetFont()->GetDescender() * scale
			- t->GetFont()->GetLineHeight() * scale * (f32)((t->GetLineCount() > 0 ? t->GetLineCount() : 1) - 1);
		const f32 width = t->GetAdvanceWidth();
		Matrix pivot;
		pivot.identity();
		pivot.Translate(-width * anchor.x, -(bottom + (top - bottom) * anchor.y), 0.f);
		for (size_t i = 0; i < meshes.size(); i++) meshes[i]->Pivot = pivot;
	}

};
