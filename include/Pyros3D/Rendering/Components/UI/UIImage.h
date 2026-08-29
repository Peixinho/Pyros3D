//============================================================================
// Name        : UIImage
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Textured, tinted, optionally 9-sliced quad on a canvas
//============================================================================

#ifndef UIIMAGE_H
#define	UIIMAGE_H

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Other/Export.h>
#include <memory>

namespace p3d {

	// The quad itself. Regenerated whenever the solved rect changes size,
	// rather than drawn as a unit quad scaled by the owner's transform,
	// because 9-slicing is incompatible with scaling by definition: the
	// whole point of a border is that it does NOT stretch. Keeping one code
	// path for both cases means a sliced and an unsliced image behave
	// identically everywhere else.
	class PYROS3D_API UIQuad : public Primitive {

	public:

		UIQuad();

		// width/height in canvas units; pivot decides where the local
		// origin sits (matching UIRect's pivot, so the mesh lines up with
		// the position UIRect wrote). border is (left, top, right, bottom)
		// in canvas units - all zero builds a plain 2-triangle quad.
		// textureSize is what the border is measured against in UV space.
		void Rebuild(const f32 width, const f32 height, const Vec2 &pivot,
			const Vec4 &border, const Vec2 &textureSize);
	};

	class PYROS3D_API UIImage : public RenderingComponent {

	public:

		UIImage(const Vec4 &tint = Vec4(1.f, 1.f, 1.f, 1.f));
		virtual ~UIImage();

		virtual uint32 GetComponentType() const { return ComponentType::UIImage; }

		// Tint multiplies the texture. With no texture set the image is a
		// solid rectangle of this colour, which is what makes panels and
		// bars work without any art at all.
		void SetTint(const Vec4 &tint);
		// The AUTHORED tint - what serialization, the inspector and style
		// extraction all read. Deliberately not what is on screen: a
		// UIButton drives its states through SetDisplayTint below, and if
		// that wrote here then saving a scene would capture whatever frame
		// of a hover fade the button happened to be in, and "extract a style
		// from this element" would promote it.
		const Vec4 &GetTint() const { return tint; }

		// Transient, never serialized. Same split as UIRect::SetStateOffset,
		// and for the same reason.
		void SetDisplayTint(const Vec4 &tint);
		const Vec4 &GetDisplayTint() const { return displayTint; }

		// NULL clears back to the shared 1x1 white texture, so the material
		// keeps a single shader variant either way - swapping between
		// "textured" and "untextured" variants at runtime would recompile a
		// shader in the middle of a frame.
		void SetTexture(const std::shared_ptr<Texture> &texture);
		const std::shared_ptr<Texture> &GetTexture() const { return texture; }

		// 9-slice borders in canvas units: (left, top, right, bottom). All
		// zero (the default) means a plain stretched quad.
		void SetBorder(const Vec4 &border);
		const Vec4 &GetBorder() const { return border; }

		// Called by UICanvas once its layout pass has solved this element's
		// rect. Rebuilds the mesh only when the size actually changed.
		void OnRectSolved(const UIRectValue &rect, const Vec2 &pivot);

		// One 1x1 opaque white texture shared by every untextured image.
		static const std::shared_ptr<Texture> &WhiteTexture();

	private:

		std::shared_ptr<Texture> texture;
		Vec4 tint;
		Vec4 displayTint;
		Vec4 border;

		// What the mesh currently reflects, so a canvas solving every frame
		// doesn't rebuild vertex buffers every frame.
		f32 builtWidth, builtHeight;
		Vec2 builtPivot;
		Vec4 builtBorder;
		bool built;
	};

};

#endif	/* UIIMAGE_H */
