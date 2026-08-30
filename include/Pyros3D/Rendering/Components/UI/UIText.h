//============================================================================
// Name        : UIText
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A line of text aligned inside a canvas rect
//============================================================================

#ifndef UITEXT_H
#define	UITEXT_H

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <string>

namespace p3d {

	namespace UIAlign
	{
		enum {
			Left = 0,
			Center,
			Right
		};
	}

	namespace UIVerticalAlign
	{
		enum {
			Top = 0,
			Middle,
			Bottom
		};
	}

	class PYROS3D_API UIText : public RenderingComponent {

	public:

		// size is the glyph height in canvas units. It is independent of
		// the Font's own baked pixel size: a Font is a fixed-size atlas
		// (see Font.h's MAP_SIZE), and Text scales its quads by
		// size/fontSize. Asking for a size far from the atlas's own is what
		// makes text look soft, so a caller wanting crisp 40px text should
		// bake a 40px Font rather than scale a 16px one.
		UIText(const std::shared_ptr<Font> &font, const std::string &text,
			const f32 size, const Vec4 &color = Vec4(1.f, 1.f, 1.f, 1.f));
		virtual ~UIText();

		virtual uint32 GetComponentType() const { return ComponentType::UIText; }

		void SetText(const std::string &text);
		const std::string &GetText() const { return text; }

		void SetColor(const Vec4 &color);
		// Authored, not displayed - see UIImage::GetTint for why the two are
		// separate.
		const Vec4 &GetColor() const { return color; }
		void SetDisplayColor(const Vec4 &color);

		void SetSize(const f32 size);
		f32 GetSize() const { return size; }

		// Wraps at word boundaries to the element's own rect width. Off by
		// default: a HUD readout that silently became two lines because a
		// value grew is worse than one that overflows visibly.
		// Re-bakes this label's font as a signed distance field (or back).
		// A property of the atlas rather than of the text, but exposed here
		// because the element is what an author has in front of them - see
		// Font's sdf flag for what it buys.
		void SetFontSDF(bool on);
		bool IsFontSDF() const { return font && font->IsSDF(); }

		void SetWordWrap(bool on);
		bool IsWordWrap() const { return wordWrap; }

		void SetAlignment(const uint32 horizontal, const uint32 vertical);
		uint32 GetHorizontalAlignment() const { return align; }
		uint32 GetVerticalAlignment() const { return verticalAlign; }

		const std::shared_ptr<Font> &GetFont() const { return font; }

		// Called by UICanvas once its layout pass has solved this element's
		// rect. Text is not stretched to fill the rect - it is ALIGNED
		// inside it, which is what a caption or a label actually wants; the
		// rect is the box, not the letter size.
		void OnRectSolved(const UIRectValue &rect, const Vec2 &pivot);

	private:

		void Realign();

		std::shared_ptr<Font> font;
		std::string text;
		Vec4 color;
		Vec4 displayColor;
		f32 size;
		uint32 align, verticalAlign;
		bool wordWrap;

		UIRectValue lastRect;
		Vec2 lastPivot;
		bool solved;
	};

};

#endif	/* UITEXT_H */
