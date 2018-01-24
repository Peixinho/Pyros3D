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
            IRenderingInstancedComponent(Renderable* renderable, IMaterial* Material, const uint32 nrInstances, const f32 boundingSphere);
            IRenderingInstancedComponent(Renderable* renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 boundingSphere);
			virtual ~IRenderingInstancedComponent();
			virtual void AddBuffer(AttributeBuffer* buffer);
			virtual void RemoveBuffer(AttributeBuffer* buffer);
			virtual const uint32 NumberOfInstances() const { return nrInstances; }
			virtual void SetNumberInstances(const uint32 instances) { nrInstances = instances; }

		protected:
			uint32 nrInstances;
	};

	class PYROS3D_API RenderingInstancedComponent : public IRenderingInstancedComponent
	{
		public:
			std::vector<Matrix> transform;
			AttributeBuffer* transform_buffer;

			RenderingInstancedComponent(Renderable* renderable, IMaterial* Material, const uint32 nrInstances, const f32 &boundingSphere);
			RenderingInstancedComponent(Renderable* renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 &boundingSphere);
		 	virtual ~RenderingInstancedComponent();
			void UpdateTransforms();
	};
};

#endif /* RENDERINGINSTANCEDCOMPONENT_H */
