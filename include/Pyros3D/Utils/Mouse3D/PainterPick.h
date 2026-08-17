//============================================================================
// Name        : PainterPick.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Painter Pick Class
//============================================================================

#ifndef PAINTERPICK_H
#define PAINTERPICK_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>

namespace p3d {

	class PYROS3D_API PainterPick : public IRenderer {
	public:

		// Constructor
		PainterPick(const uint32 Width, const uint32 Height);

		// Destructor
		virtual ~PainterPick();

		// Resize Window
		virtual void Resize(const uint32 Width, const uint32 Height);

		// Returns the RenderingMesh under (mouseX, mouseY), or NULL. Mouse
		// coordinates are in render-target pixels with the origin at the TOP
		// -left of the viewport, the same convention as every windowing/UI
		// mouse position (the backend's own framebuffer origin is handled
		// internally - see IdAt()).
		RenderingMesh* PickObject(const f32 mouseX, const f32 mouseY, Projection projection, GameObject* Camera, SceneGraph* Scene);

		// How far from the cursor, in pixels, a hit still counts when the
		// exact pixel is empty (searched outwards, nearest first). Small
		// on-screen geometry is otherwise almost impossible to click.
		// 0 restores exact-pixel-only behaviour. Default 3.
		void SetPickRadius(const uint32 radiusInPixels) { pickRadius = radiusInPixels; }
		uint32 GetPickRadius() const { return pickRadius; }

	private:

		// Render Scene
		virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene);

		// Material for Rendering
		GenericShaderMaterial* material;

		// Frame Buffer
		FrameBuffer* fbo;

		// Texture
		Texture* texture;
		f32 mouseX, mouseY;

		// Colors
		uint32 colors;

		// See SetPickRadius()
		uint32 pickRadius;

		// List
		std::map<uint32, RenderingMesh*> MeshPickingList;

		// Decodes the colour id at (x, y), x/y in top-left-origin viewport
		// pixels, from an already-read-back copy of the pick buffer.
		// Returns 0 (nothing) when out of range.
		uint32 IdAt(const std::vector<uchar> &data, const uint32 x, const uint32 y) const;

		// Aux Methods
		Vec4 Rgba8ToVec4(const uint32 val)
		{
			return Vec4((f32)((val & 0xFF000000) >> 24), (f32)((val & 0xFF0000) >> 16), (f32)((val & 0xFF00) >> 8), (f32)(val & 0xFF)) / 255.f;
		}

		uint32 Vec4ToRgba8(Vec4 val) const
		{
			val *= 255.f;
			return ((uint32(val.x) & 0xFF) << 24) | ((uint32(val.y) & 0xFF) << 16) | ((uint32(val.z) & 0xFF) << 8) | (uint32(val.w) & 0xFF);
		}

	};
}

#endif  /* PAINTERPICK */
