//============================================================================
// Name        : RenderingInstancedComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Instanced Rendering 
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>

namespace p3d {
    IRenderingInstancedComponent::IRenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const uint32 nrInstances, const f32 boundingSphere) : RenderingComponent(renderable, Material)
	{
		this->nrInstances = nrInstances;
		this->renderable = renderable;

		BoundingSphereRadius = boundingSphere;
		isInstanced = true;
	}

    IRenderingInstancedComponent::IRenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 boundingSphere) : RenderingComponent(renderable, MaterialProperties)
	{
		this->nrInstances = nrInstances;
		this->renderable = renderable;
		
		BoundingSphereRadius = boundingSphere;
		isInstanced = true;
	}

	IRenderingInstancedComponent::~IRenderingInstancedComponent()
	{

	}

	// Both of these keep the buffer on the component (see
	// RenderingComponent::ownAttributeBuffers) rather than pushing it onto
	// the Renderable's geometries, which are shared between every component
	// drawing the same mesh.
	void IRenderingInstancedComponent::AddBuffer(AttributeBuffer* buffer)
	{
		ownAttributeBuffers.push_back(buffer);
	}

	void IRenderingInstancedComponent::RemoveBuffer(AttributeBuffer* buffer)
	{
		for (std::vector<AttributeBuffer*>::iterator i = ownAttributeBuffers.begin(); i != ownAttributeBuffers.end(); i++)
		{
			if ((*i) == buffer)
			{
				ownAttributeBuffers.erase(i);
				break;
			}
		}
	}

	RenderingInstancedComponent::RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const uint32 nrInstances, const f32 &boundingSphere) : IRenderingInstancedComponent(renderable, Material, nrInstances, boundingSphere)
	{
		this->nrInstances = nrInstances;
		this->transform.resize(nrInstances);
		transform_buffer = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		transform_buffer->AddAttribute("aInstancedTransform", Buffer::Attribute::Type::Matrix, &transform[0], transform.size(), 1);
		transform_buffer->SendBuffer();
		AddBuffer(transform_buffer);
	}	
	RenderingInstancedComponent::RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 &boundingSphere) : IRenderingInstancedComponent(renderable, MaterialProperties, nrInstances, boundingSphere)
	{
		this->nrInstances = nrInstances;
		this->transform.resize(nrInstances);
		transform_buffer = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		transform_buffer->AddAttribute("aInstancedTransform", Buffer::Attribute::Type::Matrix, &transform[0], transform.size(), 1);
		transform_buffer->SendBuffer();
		AddBuffer(transform_buffer);
	}

	void RenderingInstancedComponent::UpdateTransforms()
	{
		transform_buffer->Buffer->Update(&transform[0], transform.size()*sizeof(Matrix));
	}

	RenderingInstancedComponent::~RenderingInstancedComponent()
	{
		RemoveBuffer(transform_buffer);
		delete transform_buffer;
	}
};
