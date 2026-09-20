//============================================================================
// Name        : IRenderDevice.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Active-render-device registry - see the comment on
//               GetActiveRenderDevice()/SetActiveRenderDevice() in
//               IRenderDevice.h - plus the base-class compute entry
//               points, which exist so a backend without compute needs
//               no stubs of its own (see IRenderDevice::SupportsCompute).
//============================================================================

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <cstring>
#include <set>

namespace p3d {

	// ---- Compute: the base class's "this backend has no compute" path ----
	//
	// See the block comment on IRenderDevice::SupportsCompute(). Every
	// entry point below is what a backend gets by NOT overriding, so the
	// only thing they can usefully do is say so clearly and return a value
	// that cannot be mistaken for success.

	void IRenderDevice::ComputeUnsupported(const char *what) const
	{
		// Deduplicated by name: these are reachable from per-frame code,
		// and a message repeated sixty times a second buries whatever
		// else the log was trying to say. Once per distinct method is
		// enough to diagnose - the caller's bug is that it never checked
		// SupportsCompute(), which does not become more true on the
		// second frame.
		static std::set<std::string> alreadyReported;
		const std::string key(what);
		if (alreadyReported.insert(key).second == false)
			return;
		echo(std::string("COMPUTE UNSUPPORTED: IRenderDevice::") + key
			+ "() called on a backend without compute support. "
			+ "Check SupportsCompute() before calling. "
			+ "(GL needs 4.3+/GL45 - macOS caps OpenGL at 4.1; "
			+ "GLES needs 3.1 - WebGL2 has no compute stage at all.)");
	}

	DeviceHandle IRenderDevice::CreateComputePipeline(const DeviceHandle program)
	{
		(void)program;
		ComputeUnsupported("CreateComputePipeline");
		// 0 is this interface's "no handle" everywhere else
		// (CreateShaderStage returns it for the unsupported geometry
		// stage, VulkanRenderDevice::CreatePipeline for a failed
		// creation), so a caller that checks its handle at all catches
		// this without needing a compute-specific convention.
		return 0;
	}

	void IRenderDevice::DestroyComputePipeline(const DeviceHandle pipeline)
	{
		// Silent by design, unlike its siblings: destroying something that
		// was never created is exactly what correct cleanup code does
		// after CreateComputePipeline() handed back 0, and making the
		// teardown path shout about a failure the caller already handled
		// would be noise.
		(void)pipeline;
	}

	void IRenderDevice::BindComputePipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		(void)cmd; (void)pipeline;
		ComputeUnsupported("BindComputePipeline");
	}

	DeviceHandle IRenderDevice::CreateStorageBuffer(const uint32 sizeBytes, const uint32 bindingPoint, const void *data)
	{
		(void)sizeBytes; (void)bindingPoint; (void)data;
		ComputeUnsupported("CreateStorageBuffer");
		return 0;
	}

	void IRenderDevice::UpdateStorageBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data)
	{
		(void)buffer; (void)offset; (void)sizeBytes; (void)data;
		ComputeUnsupported("UpdateStorageBuffer");
	}

	void IRenderDevice::ReadStorageBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, void *outData)
	{
		(void)buffer; (void)offset;
		ComputeUnsupported("ReadStorageBuffer");
		// Zero the caller's buffer rather than leaving it untouched. An
		// unsupported readback that leaves uninitialised stack memory in
		// place reads as plausible garbage, and a verification step then
		// fails with "wrong values" instead of "no compute here" - which
		// points the investigation at the shader.
		if (outData != NULL && sizeBytes > 0)
			memset(outData, 0, sizeBytes);
	}

	void IRenderDevice::BindStorageBuffer(const CommandBufferHandle cmd, const DeviceHandle buffer, const uint32 bindingPoint)
	{
		(void)cmd; (void)buffer; (void)bindingPoint;
		ComputeUnsupported("BindStorageBuffer");
	}

	void IRenderDevice::DestroyStorageBuffer(const DeviceHandle buffer)
	{
		// Silent, same reasoning as DestroyComputePipeline().
		(void)buffer;
	}

	void IRenderDevice::Dispatch(const CommandBufferHandle cmd, const uint32 groupsX, const uint32 groupsY, const uint32 groupsZ)
	{
		(void)cmd; (void)groupsX; (void)groupsY; (void)groupsZ;
		ComputeUnsupported("Dispatch");
	}

	void IRenderDevice::ComputeBarrier(const CommandBufferHandle cmd, const uint32 barrierBits)
	{
		(void)cmd; (void)barrierBits;
		ComputeUnsupported("ComputeBarrier");
	}

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
