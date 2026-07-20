//============================================================================
// Name        : IRenderDevice.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Active-render-device registry - see the comment on
//               GetActiveRenderDevice()/SetActiveRenderDevice() in
//               IRenderDevice.h.
//============================================================================

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

namespace p3d {

	static IRenderDevice* activeDevice = NULL;

	IRenderDevice& GetActiveRenderDevice()
	{
		if (activeDevice != NULL)
			return *activeDevice;
		static GLRenderDevice fallback;
		return fallback;
	}

	void SetActiveRenderDevice(IRenderDevice* device)
	{
		activeDevice = device;
	}

};
