//============================================================================
// Name        : RenderTargetsTab.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Lists every live FrameBuffer and draws its attachments.
//               Same panel the demo launcher carries (DemoLauncher.cpp), so
//               a scene running under Play in the editor can be inspected
//               the same way it can when it runs standalone.
//============================================================================

#ifndef RENDERTARGETSTAB_H
#define	RENDERTARGETSTAB_H

#include "IUInterface.h"
#include <Pyros3D/Assets/Texture/Texture.h>
#include <map>
#include <string>
#include <utility>

class RenderTargetsTab : public IUInterface {
public:

	RenderTargetsTab(const std::string &name, bool* open);
	virtual ~RenderTargetsTab();

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time) { now = time; }
	virtual void Show();
	virtual void Shutdown() { ClearPreviews(); }

	// The previews below are keyed on Texture*, and those pointers die with
	// whatever created them - a closed scene document, a stopped Play, a
	// resized viewport. Anything that invalidates render targets calls this.
	void ClearPreviews();

private:

	// Depth and cube attachments cannot be handed to ImGui directly (see
	// IRenderDevice::GetImGuiTextureID). These read the texture back on the
	// CPU, normalise it, and keep a small RGBA8 copy that ImGui can draw.
	struct Preview
	{
		p3d::Texture *preview;	// owned
		f64 lastUpdate;
		uint32 width, height;
		float rangeMin, rangeMax;
		Preview() : preview(NULL), lastUpdate(-1.0), width(0), height(0), rangeMin(0.f), rangeMax(1.f) {}
	};

	// face is -1 for a plain 2D depth target, or a TextureType::Cubemap*.
	Preview *GetPreview(p3d::Texture *src, const int32 face, const bool refresh);

	std::string Name;
	bool* Open;
	std::map<std::pair<p3d::Texture*, int32>, Preview> previews;
	float thumbSize;
	float refreshSeconds;
	f64 now;

};

#endif	/* RENDERTARGETSTAB_H */
