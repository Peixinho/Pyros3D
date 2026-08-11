//============================================================================
// Name        : TOOLSTAB.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : TOOLSTAB
//============================================================================

#ifndef TOOLSTAB_H
#define	TOOLSTAB_H

#include "IUInterface.h"
#include <string>

class ToolsTab : public IUInterface {
public:

	ToolsTab(bool* open) : Name("Tools"), Open(open), caller(NULL) {}

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time) {}
	virtual void Shutdown() {}

	void SetActive(IUInterface* Caller) { caller = Caller; }

	virtual void Show()
	{
		// End() pairs with Begin() unconditionally - see TabLog.h.
		if (ImGui::Begin(Name.c_str(), Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			// Show Stuff
			if (caller != NULL) caller->ShowTools();
		}
		ImGui::End();
	}

protected:
	
	IUInterface* caller;

	std::string Name;
	bool* Open;

};

#endif	/* TOOLSTAB_H */
