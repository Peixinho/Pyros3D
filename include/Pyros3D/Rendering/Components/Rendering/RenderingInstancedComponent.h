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
			virtual void SetNumberInstances(const uint32 instances) { nrInstances = instances; }
			virtual uint32 GetComponentType() const { return ComponentType::RenderingInstancedComponent; }

		protected:
			uint32 nrInstances;
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

			RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const uint32 nrInstances, const f32 &boundingSphere);
			RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 &boundingSphere);
		 	virtual ~RenderingInstancedComponent();
			void UpdateTransforms();
	};
};

#endif /* RENDERINGINSTANCEDCOMPONENT_H */
