//============================================================================
// Name        : PropertiesTab.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PropertiesTab
//============================================================================

#ifndef PROPERTIESTAB_H
#define	PROPERTIESTAB_H

#include "IUInterface.h"
#include <functional>
#include <string>

class PropertiesTab : public IUInterface {
public:

	PropertiesTab(bool* open) : Name("Properties"), Open(open), caller(NULL) {}

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time) {}
	virtual void Shutdown() {}

	void SetActive(IUInterface* Caller) { caller = Caller; }

	// Optional second source of content, given priority over the scene
	// selection: returns true if it drew (and therefore owns the panel this
	// frame), false to fall through to caller->ShowProperties(). Editor
	// installs one that draws the focused material document's property
	// sheet - the material's textures/settings/inspector live here rather
	// than in the Material Editor window, which keeps only its canvas.
	void SetOverrideDrawer(std::function<bool()> fn) { overrideDrawer = fn; }

	virtual void Show()
	{
		// End() pairs with Begin() unconditionally - see TabLog.h.
		if (ImGui::Begin(Name.c_str(), Open))
		{
			// Show Stuff
			const bool drawn = overrideDrawer ? overrideDrawer() : false;
			if (!drawn && caller != NULL) caller->ShowProperties();
		}
		ImGui::End();
	}

protected:

	IUInterface* caller;
	std::function<bool()> overrideDrawer;

	std::string Name;
	bool* Open;

};

#endif	/* PROPERTIESTAB_H */
