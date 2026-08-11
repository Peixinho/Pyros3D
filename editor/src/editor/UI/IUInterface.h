//============================================================================
// Name        : IUINTERFACE.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IUINTERFACE
//============================================================================

#ifndef IUINTERFACE_H
#define	IUINTERFACE_H

#include <imgui.h>
#include <Pyros3D/Core/Math/Math.h>

using namespace p3d;

class IUInterface {

	public:

		static bool editorDisabled;

		IUInterface() {}

		virtual void Init(const uint32 width, const uint32 height) = 0;
		virtual void OnResize(const uint32 width, const uint32 height) = 0;
		virtual void Update(const f64 time) = 0;
		virtual void Show() = 0;
		virtual void Shutdown() = 0;

		virtual void ShowProperties() {}
		virtual void ShowTools() {}
		virtual void ShowMenubarOptions() {}

};

#endif	/* IUINTERFACE_H */
