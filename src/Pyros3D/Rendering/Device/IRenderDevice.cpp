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

	// Weak on purpose - see BorrowActiveRenderDevice()'s comment in the
	// header. The registry must not be what keeps a device alive, or a
	// device would outlive every user of it until something else was
	// published; and it must not hold a raw pointer either, or it goes
	// dangling the moment the owner dies.
	static std::weak_ptr<IRenderDevice> activeDevice;
	// A device this process does NOT own (a Context builds and destroys its
	// own). Held strongly here only so activeDevice has something to be weak
	// to; the deleter does nothing, so dropping it frees nothing.
	static std::shared_ptr<IRenderDevice> externallyOwnedActive;
	static IRenderDevice* pendingOwnershipDevice = NULL;

	IRenderDevice& GetActiveRenderDevice()
	{
		if (std::shared_ptr<IRenderDevice> device = activeDevice.lock())
			return *device;
		static GLRenderDevice fallback;
		return fallback;
	}

	void SetActiveRenderDevice(IRenderDevice* device)
	{
		if (device == NULL)
		{
			activeDevice.reset();
			externallyOwnedActive.reset();
			return;
		}
		if (std::shared_ptr<IRenderDevice> current = activeDevice.lock())
		{
			// Already published, possibly as an owned device - leave that
			// share intact rather than demoting it to a non-owning wrapper.
			if (current.get() == device)
				return;
		}
		externallyOwnedActive = std::shared_ptr<IRenderDevice>(device, [](IRenderDevice*) {});
		activeDevice = externallyOwnedActive;
	}

	void SetActiveRenderDevice(const std::shared_ptr<IRenderDevice> &device)
	{
		// Re-publishing the Context's own device (every IRenderer does this
		// right after borrowing it) must NOT drop our strong reference to it.
		// Dropping it left the weak activeDevice hanging off the borrower's
		// share alone, so the first renderer to be destroyed - opening a
		// project replaces the Scene View's - expired the registry, and
		// Texture/FrameBuffer/Shader fell back to the static GLRenderDevice.
		// In a Vulkan process that device has NULL glad pointers: instant
		// EXC_BAD_ACCESS at 0x0 inside GLRenderDevice::CreateTextureObject,
		// which is the very crash the header's TakeRenderDeviceOwnership()
		// comment describes. A Context unpublishes explicitly, with
		// SetActiveRenderDevice(NULL), and only then.
		if (externallyOwnedActive && device && externallyOwnedActive.get() == device.get())
			return;
		externallyOwnedActive.reset();
		activeDevice = device;
	}

	std::shared_ptr<IRenderDevice> BorrowActiveRenderDevice()
	{
		return activeDevice.lock();
	}

	std::shared_ptr<IRenderDevice> AdoptRenderDevice(IRenderDevice* device)
	{
		std::shared_ptr<IRenderDevice> owned(device);
		SetActiveRenderDevice(owned);
		return owned;
	}

	bool IsActiveRenderDeviceSet()
	{
		// Truthful by construction now: expired() is the device actually
		// being gone, not someone having remembered to clear a pointer.
		return !activeDevice.expired();
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
