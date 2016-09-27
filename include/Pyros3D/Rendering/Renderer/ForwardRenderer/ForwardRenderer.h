//============================================================================
// Name        : ForwardRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forward Renderer
//============================================================================

#ifndef FORWARDRENDERER_H
#define FORWARDRENDERER_H

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <algorithm>

namespace p3d {

	class PYROS3D_API ForwardRenderer : public IRenderer {

	public:

		ForwardRenderer(const uint32 Width, const uint32 Height);

		~ForwardRenderer();

		virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag = 0);

		virtual void PreRender(GameObject* Camera, SceneGraph* Scene);
		virtual void PreRender(GameObject* Camera, SceneGraph* Scene, const uint32 Tag);
		virtual void PreRender(GameObject* Camera, SceneGraph* Scene, const std::string &Tag);
		virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene);

	private:

		GenericShaderMaterial* shadowMaterial, *shadowSkinnedMaterial;
		std::vector<RenderingMesh*> rmesh;
		std::vector<IComponent*> lcomps;

	};

};

#endif /* FORWARDRENDERER_H */