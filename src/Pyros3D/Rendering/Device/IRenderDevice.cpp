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
	static IRenderDevice* pendingOwnershipDevice = NULL;

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

	bool IsActiveRenderDeviceSet()
	{
		return activeDevice != NULL;
	}

	void RegisterRenderDeviceForOwnership(IRenderDevice* device)
	{
		// Also register as the active device - a caller doing this wants
		// Shaders.cpp/etc to pick it up too, same as a plain
		// SetActiveRenderDevice() call.
		SetActiveRenderDevice(device);
		pendingOwnershipDevice = device;
	}

	IRenderDevice* TakeRenderDeviceOwnership()
	{
		IRenderDevice* device = pendingOwnershipDevice;
		// Consumed - only the first taker gets ownership; see the header
		// comment for why a second, unrelated IRenderer construction must
		// not also try to adopt (and later delete) the same pointer.
		pendingOwnershipDevice = NULL;
		return device;
	}

};
