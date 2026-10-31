//============================================================================
// Name        : RenderTargetsTab.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : The Scene View renderer's own targets, as pictures
//               (SceneRenderer::DebugTargets: the HDR picture, the AO, the
//               motion, the bloom, the sky's tables, the clouds, the probes'
//               atlas).
//============================================================================

#ifndef RENDERTARGETSTAB_H
#define	RENDERTARGETSTAB_H

#include "IUInterface.h"
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <functional>
#include <string>

class RenderTargetsTab : public IUInterface {
public:

	RenderTargetsTab(const std::string &name, bool* open);
	virtual ~RenderTargetsTab() {}

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time) {}
	virtual void Show();
	virtual void Shutdown() {}

	// Whose targets: the renderer drawing the active Scene View (NULL: none yet)
	void SetSource(const std::function<p3d::SceneRenderer*()> &source) { this->source = source; }

private:

	std::string Name;
	bool* Open;
	float thumbSize;
	std::function<p3d::SceneRenderer*()> source;

};

#endif	/* RENDERTARGETSTAB_H */
