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
#include <string>

class PropertiesTab : public IUInterface {
public:

	PropertiesTab(bool* open) : Name("Properties"), Open(open), caller(NULL) {}

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time) {}
	virtual void Shutdown() {}

	void SetActive(IUInterface* Caller) { caller = Caller; }

	virtual void Show()
	{
		// End() pairs with Begin() unconditionally - see TabLog.h.
		if (ImGui::Begin(Name.c_str(), Open))
		{
			// Show Stuff
			if (caller != NULL) caller->ShowProperties();
		}
		ImGui::End();
	}

protected:
	
	IUInterface* caller;

	std::string Name;
	bool* Open;

};

#endif	/* PROPERTIESTAB_H */
