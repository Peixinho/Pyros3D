//============================================================================
// Name        : RenderingInstancedComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Instanced Rendering 
//============================================================================

#ifndef RENDERINGINSTANCEDCOMPONENT_H
#define	RENDERINGINSTANCEDCOMPONENT_H

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <algorithm>

namespace p3d {
	
	class PYROS3D_API  IRenderingInstancedComponent : public RenderingComponent
	{
		public:
            IRenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const uint32 nrInstances, const f32 boundingSphere);
            IRenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 boundingSphere);
			virtual ~IRenderingInstancedComponent();
			virtual void AddBuffer(AttributeBuffer* buffer);
			virtual void RemoveBuffer(AttributeBuffer* buffer);
			virtual const uint32 NumberOfInstances() const { return nrInstances; }
			// How many are DRAWN: the number as it was when the frame's lists were
			// kept (TakeDrawCount, from the renderer's PreRender when PYROS_FRAME_SPLIT
			// is on) - the component goes on being updated, by another thread, while
			// the frame is drawn. Until taken, the number itself.
			uint32 InstancesToDraw() const { return (drawCountTakenAt == GameObject::s_drawEpoch && GameObject::s_drawEpoch != 0) ? drawCount : (uint32)NumberOfInstances(); }
			void TakeDrawCount() { drawCount = (uint32)NumberOfInstances(); drawGrowth = instanceGrowth; drawKeep = instanceKeep; drawCountTakenAt = GameObject::s_drawEpoch; }
			const Vec2 &GrowthToDraw() const { return (drawCountTakenAt == GameObject::s_drawEpoch && GameObject::s_drawEpoch != 0) ? drawGrowth : instanceGrowth; }
			f32 KeepToDraw() const { return (drawCountTakenAt == GameObject::s_drawEpoch && GameObject::s_drawEpoch != 0) ? drawKeep : instanceKeep; }
			f32 drawKeep = 0.f;
			uint32 drawCount = 0;
			uint32 drawCountTakenAt = 0;
			Vec2 drawGrowth = Vec2(1.f, 1.f);
			virtual void SetNumberInstances(const uint32 instances) { mostInstances = std::max(mostInstances, std::max(nrInstances, instances)); nrInstances = instances; }
			// The most it has drawn: what a thinned block (fewer drawn far off) has in all
			uint32 MostInstances() const { return std::max(mostInstances, nrInstances); }
			uint32 mostInstances = 0;
			// Every instance drawn this much wider and taller than its mesh,
			// about the mesh's own origin (1, 1: as it is). What draws fewer
			// instances of a thing far off - a field of grass - grows the ones
			// left with it, and the field covers the ground as it did.
			void SetInstanceGrowth(const f32 width, const f32 height) { instanceGrowth = Vec2(width, height); }
			// How many instances, to the fraction (0: not said - all drawn as they are). The instance past
			// the whole part is to be drawn dissolved by the fraction, a block thinning with distance letting its
			// plants go one at a time, each fading (the renderer draws the whole part, the growth, not the fade yet).
			void SetInstanceKeep(const f32 keep) { instanceKeep = keep; }
			f32 instanceKeep = 0.f;
			const Vec2 &GetInstanceGrowth() const { return instanceGrowth; }
			// Each instance faded in and out with its own distance from the eye (metres; to <= from: none) - drawn
			// shrinking to nothing: a far forest's trees coming in past where the near ones end, gone at the horizon
			void SetDistanceFade(const f32 inFrom, const f32 inTo, const f32 outFrom, const f32 outTo) { fadeIn = Vec2(inFrom, inTo); fadeOut = Vec2(outFrom, outTo); }
			Vec2 fadeIn = Vec2(0.f, 0.f), fadeOut = Vec2(0.f, 0.f);
			// Fewer drawn the further the set is (its sphere's nearest point): all to `from` metres, `least` of them
			// from `to` on, the ones left grown to cover what the others did - its instances must be in no order (a
			// shuffled list: the first n are any n). The renderer does it, in the view it draws (to <= from: none)
			void SetDistanceThinning(const f32 from, const f32 to, const f32 least) { thinFrom = from; thinTo = to; thinLeast = least; }
			f32 thinFrom = 0.f, thinTo = 0.f, thinLeast = 1.f;
			virtual uint32 GetComponentType() const { return ComponentType::RenderingInstancedComponent; }

		protected:
			uint32 nrInstances;
			Vec2 instanceGrowth = Vec2(1.f, 1.f);
	};

	class PYROS3D_API RenderingInstancedComponent : public IRenderingInstancedComponent
	{
		public:
			std::vector<Matrix> transform;
			AttributeBuffer* transform_buffer;

			// Per-instance tint, for materials built with
			// ShaderUsage::InstancedColor. Opt-in and NULL until
			// EnableInstanceColors() is called: it costs a vec4 per
			// instance, and a shader declaring aInstancedColor with no
			// buffer behind it fails Vulkan pipeline creation, so the two
			// have to be turned on together.
			std::vector<Vec4> instanceColor;
			AttributeBuffer* color_buffer;
			// Changes whenever the instances' transforms or tints do (a renderer's copy of them knows it is stale)
			uint32 instanceRevision = 0;
			void EnableInstanceColors();
			bool HasInstanceColors() const { return color_buffer != NULL; }
			void UpdateInstanceColors();

			// Deterministic scatter for vegetation-style fields, done here
			// rather than in script because the script path costs one bound
			// call per instance - a chunk of a few thousand blades is
			// thousands of sol2 round-trips, which is fine at load and far
			// too slow to stream chunks in as a camera moves.
			//
			// Lays instances out as [every item's quad 0][every item's quad
			// 1][...] rather than interleaving an item's quads together, and
			// scatters items in hash order rather than row-major. Both are
			// load-bearing for LOD: it means any *prefix* of the buffer is
			// itself a valid, spatially even, lower-quality version of the
			// field, so distance density is just SetNumberInstances() with
			// no rebuild. Taking half draws one quad per item instead of a
			// cross; taking a quarter also thins the items themselves.
			//
			// Positions are local to the component, so the owning GameObject
			// places the chunk (see the bounding sphere - it is centred on
			// the component, not on wherever the instances happened to land).
			// Fills instance colours too when they are enabled, tinting
			// within [tintLow, tintHigh] and correlating darkness with
			// scale, the way short undergrowth reads darker.
			void ScatterInstances(const uint32 seed, const f32 cellSizeX, const f32 cellSizeZ,
			                      const f32 itemHeight, const f32 minScale, const f32 maxScale,
			                      const uint32 items, const uint32 quadsPerItem,
			                      const Vec4 &tintLow = Vec4(1.f, 1.f, 1.f, 1.f),
			                      const Vec4 &tintHigh = Vec4(1.f, 1.f, 1.f, 1.f));

			RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const uint32 nrInstances, const f32 &boundingSphere);
			RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 &boundingSphere);
			// With its instances as they are to be (and their tints, or none): made and sent once, with no
			// buffer of nothing first and no second sending - what a field of grass coming in is made with.
			RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, std::vector<Matrix> &&transforms, std::vector<Vec4> &&tints, const f32 &boundingSphere = 0);
			RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialProperties, std::vector<Matrix> &&transforms, std::vector<Vec4> &&tints, const f32 &boundingSphere = 0);
			void InitFilled(std::vector<Matrix> &&transforms, std::vector<Vec4> &&tints);
			// Its instance buffers made again, room for `capacity` instances (and their tints, or none),
			// of nothing: what the GPU writes into (a cull's drawer). No copy is kept here.
			void ResizeForGpu(const uint32 capacity, const bool tinted);
			uint32 GpuCapacity() const { return gpuCapacity; }
		private:
			uint32 gpuCapacity = 0;
		public:
		 	virtual ~RenderingInstancedComponent();
			void UpdateTransforms();
	};
};

#endif /* RENDERINGINSTANCEDCOMPONENT_H */
