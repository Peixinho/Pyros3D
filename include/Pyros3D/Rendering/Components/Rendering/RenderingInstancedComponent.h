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
			void TakeDrawCount() { drawCount = (uint32)NumberOfInstances(); drawGrowth = instanceGrowth; drawCountTakenAt = GameObject::s_drawEpoch; }
			const Vec2 &GrowthToDraw() const { return (drawCountTakenAt == GameObject::s_drawEpoch && GameObject::s_drawEpoch != 0) ? drawGrowth : instanceGrowth; }
			uint32 drawCount = 0;
			uint32 drawCountTakenAt = 0;
			Vec2 drawGrowth = Vec2(1.f, 1.f);
			virtual void SetNumberInstances(const uint32 instances) { nrInstances = instances; }
			// Every instance drawn this much wider and taller than its mesh,
			// about the mesh's own origin (1, 1: as it is). What draws fewer
			// instances of a thing far off - a field of grass - grows the ones
			// left with it, and the field covers the ground as it did.
			void SetInstanceGrowth(const f32 width, const f32 height) { instanceGrowth = Vec2(width, height); }
			const Vec2 &GetInstanceGrowth() const { return instanceGrowth; }
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
		 	virtual ~RenderingInstancedComponent();
			void UpdateTransforms();
	};
};

#endif /* RENDERINGINSTANCEDCOMPONENT_H */
