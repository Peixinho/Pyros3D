//============================================================================
// Name        : RenderingInstancedComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Instanced Rendering 
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>

namespace p3d {
	IRenderingInstancedComponent::IRenderingInstancedComponent(Renderable* renderable, IMaterial* Material, const uint32 nrInstances, const f32 &boundingSphere) : RenderingComponent(renderable, Material)
	{
		this->nrInstances = nrInstances;
		this->renderable = renderable;

		BoundingSphereRadius = boundingSphere;
		isInstanced = true;
	}

	IRenderingInstancedComponent::IRenderingInstancedComponent(Renderable* renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 &boundingSphere) : RenderingComponent(renderable, MaterialProperties)
	{
		this->nrInstances = nrInstances;
		this->renderable = renderable;
		
		BoundingSphereRadius = boundingSphere;
		isInstanced = true;
	}

	IRenderingInstancedComponent::~IRenderingInstancedComponent()
	{

	}

	void IRenderingInstancedComponent::AddBuffer(AttributeBuffer* buffer)
	{
		for (std::vector<IGeometry*>::iterator i = renderable->Geometries.begin(); i != renderable->Geometries.end(); i++)
		{
			(*i)->Attributes.push_back(buffer);
		}
	}

	void IRenderingInstancedComponent::RemoveBuffer(AttributeBuffer* buffer)
	{
		for (std::vector<IGeometry*>::iterator i = renderable->Geometries.begin(); i != renderable->Geometries.end(); i++)
		{
			for (std::vector<AttributeArray*>::iterator j = (*i)->Attributes.begin(); j != (*i)->Attributes.end(); j++)
			{
				if ((*j)==buffer)
				{
					(*i)->Attributes.erase(j);
					break;
				}
			}
		}
	}

	RenderingInstancedComponent::RenderingInstancedComponent(Renderable* renderable, IMaterial* Material, const uint32 nrInstances, const f32 &boundingSphere) : IRenderingInstancedComponent(renderable, Material, nrInstances, boundingSphere)
	{
		this->nrInstances = nrInstances;
		this->transform.resize(nrInstances);
		transform_buffer = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		transform_buffer->AddAttribute("aInstancedTransform", Buffer::Attribute::Type::Matrix, &transform[0], transform.size(), 1);
		transform_buffer->SendBuffer();
		AddBuffer(transform_buffer);
	}	
	RenderingInstancedComponent::RenderingInstancedComponent(Renderable* renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 &boundingSphere) : IRenderingInstancedComponent(renderable, MaterialProperties, nrInstances, boundingSphere)
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