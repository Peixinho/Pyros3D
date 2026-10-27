//============================================================================
// Name        : InstanceCullGroup.h
// Author      : Duarte Peixinho
// Description : Instanced things culled on the GPU, one instance at a time.
//
//               A group is a set of instanced components drawing the same
//               mesh with the same material - a foliage layer's blocks - and
//               two drawers of its own: one with the mesh, one with its far
//               mesh (a grass tuft's simple self). In a view that can (compute
//               and indirect draws: Vulkan, Metal, GL 4.3), the renderer hands
//               the group the blocks of it that are in the view; a dispatch
//               each goes over their instances, keeps those the block draws
//               now (the first NumberOfInstances: its distance thinning, as
//               it is), leaves out the ones outside the view, and writes each
//               one kept into the near drawer or the far one by how far that
//               instance is - and the two drawers are drawn, from what the
//               GPU counted, in place of the blocks. A layer is then two
//               draws, not one a block, and only what is in the view is drawn,
//               each plant at the detail its own distance asks for (a block
//               was all at one: a block beside the eye drew every plant of it
//               at full detail, out to its far corner).
//
//               The blocks are left as they are: any other view - a shadow
//               map, one past those the group has room for, a device with no
//               compute - draws them as ever.
//
//               A block's instance matrices are local to it; the drawers are
//               placed with the group's owner, and what the GPU writes is in
//               the owner's space. Each instance's growth (thinning's) is
//               carried in its matrix's last row - see PyrosShader.glsl.
//============================================================================

#ifndef INSTANCECULLGROUP_H
#define INSTANCECULLGROUP_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <memory>
#include <vector>

namespace p3d {

	class GameObject;
	class Renderable;
	class IMaterial;
	class RenderingMesh;
	class RenderingInstancedComponent;

	class PYROS3D_API InstanceCullGroup {
	public:
		// The device can (Vulkan, Metal; GL 4.3 when asked: PYROS_GPU_CULL=1), and it is not turned off
		// (SetEnabled, PYROS_GPU_CULL=0). PYROS_GPU_CULL_TOGGLE=<s>: on and off in turn, to compare in one
		// run (GpuCull.On); PYROS_GPU_CULL_TRACE=<s>: that second's counts read back to stderr.
		static bool Available();
		static void SetEnabled(const bool on);
		static bool GetEnabled();

		// farMesh may be NULL (one drawer); farFrom: the distance past which an instance is the far
		// mesh's. meshRadius: the mesh's bounding radius, before an instance's own scale.
		InstanceCullGroup(GameObject* owner, const std::shared_ptr<Renderable> &mesh, const std::shared_ptr<IMaterial> &material,
			const std::shared_ptr<Renderable> &farMesh, const f32 farFrom, const bool tinted, const f32 meshRadius);
		~InstanceCullGroup();

		// A block of the group (on a child of the owner).
		void Add(RenderingInstancedComponent* block);
		// Thinned plant by plant, at each one's own distance (metres, before the foliage's reach): from
		// thinFrom to thinTo those drawn fall to `least` of them and the ones left grow, and from
		// fullDistance to fadeDistance they go. A plant near its turn dissolves (the dither the shader
		// has for it) rather than going from one frame to the next. Its blocks are trimmed on the CPU
		// to what their nearest point keeps - each plant's own say is the kernel's.
		void SetThinning(const f32 thinFrom, const f32 thinTo, const f32 least, const f32 fullDistance, const f32 fadeDistance)
		{ thinning = true; thin[0] = thinFrom; thin[1] = thinTo; thin[2] = least; thin[3] = fullDistance; thin[4] = fadeDistance; }
		// The drawers out of the owner (before the group goes, while the owner is alive: the group's
		// destructor does not touch it - at a scene's end the owner is the one being taken apart).
		void Detach();

		// The renderer's, once a view, before it draws: the blocks of every group among `visible` are
		// culled on the GPU, and the groups' drawers take their place in it. Not a view the groups can
		// take (more views this frame than they have room for): left as it is. frameKey tells one frame
		// from the next (the scene's update time).
		static void CullView(std::vector<RenderingMesh*> &visible, GameObject* camera, const Matrix &projection, const f64 frameKey);
		// A drawer's bounds in the view just culled - the blocks it draws there - for whatever chooses
		// what reaches it by where it is (the forward renderer's lights). False: not a drawer.
		static bool DrawerSphere(class RenderingComponent* rc, Vec3 &centre, f32 &radius);

	private:
		bool Cull(const uint32 region, std::vector<RenderingInstancedComponent*> &blocks, const Vec4 worldPlanes[6], const Vec3 &eyeWorld);
		void EnsureCapacity(const uint32 instances);

		GameObject* owner;
		std::shared_ptr<GameObject> drawerObject;
		std::shared_ptr<RenderingInstancedComponent> nearDrawer, farDrawer;
		std::vector<RenderingInstancedComponent*> blocks;
		f32 farFrom, meshRadius;
		bool tinted;
		bool thinning = false;
		f32 thin[5] = { 0.f, 0.f, 1.f, 0.f, 0.f };
		uint32 capacity = 0;
		Vec3 viewCentre;
		f32 viewRadius = 0.f;
		// Per region (a view of a frame in flight): the dispatches' parameters, the draws' arguments. A
		// buffer each, not a part of one: writing the second view's would wait for the first's cull.
		static const uint32 kMaxRegions = 4;
		DeviceHandle params[kMaxRegions] = { 0, 0, 0, 0 }, args[kMaxRegions] = { 0, 0, 0, 0 };
		uint32 paramsBytes[kMaxRegions] = { 0, 0, 0, 0 };
	};

}

#endif /* INSTANCECULLGROUP_H */
