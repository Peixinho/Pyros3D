//============================================================================
// Name        : TABLOG.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : TABLOG
//============================================================================

#ifndef TABLOG_H
#define	TABLOG_H

#include "IUInterface.h"
#include <string>

class TabLog: public IUInterface {
public:

	TabLog(const std::string &name, bool* open) : Name(name), Open(open) {}

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time) {}
	virtual void Shutdown() {}

	virtual void Show()
	{
		// End() pairs with Begin() unconditionally - Begin() returning
		// false only means the contents are clipped (collapsed window,
		// unselected dock tab), not that the window was never pushed.
		if (ImGui::Begin(Name.c_str(), Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::Text("Logging Shit");
		}
		ImGui::End();
	}

protected:

	std::string Name;
	bool* Open;

};

#endif	/* IUINTERFACE_H */
