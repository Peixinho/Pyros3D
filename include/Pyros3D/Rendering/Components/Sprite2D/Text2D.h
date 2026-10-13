//============================================================================
// Name        : Text2D
// Description : Words in the world: a number that floats up off what was
//               hit, a sign, a name over a head. A UIText is laid out on a
//               canvas, in the screen's units, and stays on the screen;
//               this is an object's, in the scene's units, and moves, turns
//               and scales with it like a sprite.
//
//               `size` is the height of the font's em in world units.
//               `anchor` is where in the block of text the object's origin
//               is: (0.5, 0.5) the middle - the default - (0, 0) its bottom
//               left. z is draw order, as for a sprite.
//============================================================================

#ifndef TEXT2D_H
#define TEXT2D_H

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>

namespace p3d {

	class PYROS3D_API Text2D : public RenderingComponent {

	public:

		Text2D(const std::shared_ptr<Font> &font, const std::string &text, const f32 size,
			const Vec4 &color = Vec4(1.f, 1.f, 1.f, 1.f));
		virtual ~Text2D();

		virtual uint32 GetComponentType() const { return ComponentType::Text2D; }

		void SetText(const std::string &text);
		const std::string &GetText() const { return text; }
		void SetColor(const Vec4 &color);
		const Vec4 &GetColor() const { return color; }
		void SetAlpha(const f32 a) { SetColor(Vec4(color.x, color.y, color.z, a)); }
		void SetSize(const f32 size);
		f32 GetSize() const { return size; }
		void SetAnchor(const Vec2 &anchor);
		const Vec2 &GetAnchor() const { return anchor; }
		// How wide and tall the block of text is, in world units at scale 1.
		Vec2 GetBlockSize() const;

		const std::shared_ptr<Font> &GetFont() const { return font; }
		// The path as the scene file has it (the font holds the resolved one).
		void SetFontPath(const std::string &path) { fontPath = path; }
		const std::string &GetFontPath() const { return fontPath; }

	private:

		void Realign();

		std::shared_ptr<Font> font;
		std::string fontPath;
		std::string text;
		Vec4 color;
		f32 size;
		Vec2 anchor;
	};

};

#endif	/* TEXT2D_H */
