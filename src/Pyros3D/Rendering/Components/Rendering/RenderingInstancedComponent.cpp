//============================================================================
// Name        : RenderingInstancedComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Instanced Rendering 
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <cmath>

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
		color_buffer = NULL;
	}	
	RenderingInstancedComponent::RenderingInstancedComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialProperties, const uint32 nrInstances, const f32 &boundingSphere) : IRenderingInstancedComponent(renderable, MaterialProperties, nrInstances, boundingSphere)
	{
		this->nrInstances = nrInstances;
		this->transform.resize(nrInstances);
		transform_buffer = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		transform_buffer->AddAttribute("aInstancedTransform", Buffer::Attribute::Type::Matrix, &transform[0], transform.size(), 1);
		transform_buffer->SendBuffer();
		AddBuffer(transform_buffer);
		color_buffer = NULL;
	}

	// Separate buffer rather than interleaving the tint into
	// transform_buffer, because that one is sized and uploaded as a plain
	// array of Matrix (UpdateTransforms writes transform.size()*sizeof(Matrix)
	// straight from the vector) and every existing caller depends on that.
	void RenderingInstancedComponent::EnableInstanceColors()
	{
		if (color_buffer != NULL)
			return;
		instanceColor.resize(nrInstances, Vec4(1.f, 1.f, 1.f, 1.f));
		color_buffer = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		color_buffer->AddAttribute("aInstancedColor", Buffer::Attribute::Type::Vec4, &instanceColor[0], instanceColor.size(), 1);
		color_buffer->SendBuffer();
		AddBuffer(color_buffer);
	}

	void RenderingInstancedComponent::UpdateInstanceColors()
	{
		if (color_buffer == NULL)
			return;
		color_buffer->Buffer->Update(&instanceColor[0], instanceColor.size()*sizeof(Vec4));
	}

	// Same sin-based hash the shader/script scatters have always used here,
	// kept deliberately so a field generated in C++ matches one generated in
	// Lua from the same seed - a scene can move a chunk from one to the other
	// without the grass visibly rearranging itself.
	static inline f32 ScatterHash(const f32 a, const f32 b)
	{
		f32 v = sinf(a * 12.9898f + b * 78.233f) * 43758.5453f;
		return v - floorf(v);
	}

	void RenderingInstancedComponent::ScatterInstances(const uint32 seed, const f32 cellSizeX, const f32 cellSizeZ,
	                                                   const f32 itemHeight, const f32 minScale, const f32 maxScale,
	                                                   const uint32 items, const uint32 quadsPerItem,
	                                                   const Vec4 &tintLow, const Vec4 &tintHigh)
	{
		if (items == 0 || quadsPerItem == 0)
			return;

		const f32 seedF = (f32)seed;
		const bool tint = (color_buffer != NULL);

		// Quad-major, not item-major - see the header comment. Instance
		// index = quad * items + item, so instances [0, items) are one quad
		// for every item, which is the useful half-quality version.
		for (uint32 q = 0; q < quadsPerItem; q++)
		{
			for (uint32 i = 0; i < items; i++)
			{
				const uint32 index = q * items + i;
				if (index >= transform.size())
					return;

				const f32 fi = (f32)i + 1.f;
				const f32 rx = ScatterHash(fi, seedF + 1.7f);
				const f32 rz = ScatterHash(fi, seedF + 5.3f);
				const f32 rr = ScatterHash(fi, seedF + 9.1f);

				const f32 scale = minScale + rr * (maxScale - minScale);
				// Quads of one item share a position and differ only in yaw,
				// spread evenly over a half turn so two of them make a cross.
				const f32 yaw = rr * (f32)M_PI + (f32)q * (f32)M_PI / (f32)quadsPerItem;

				Matrix m;
				m.Scale(scale, scale, scale);
				m.RotationY(yaw);
				m.Translate((rx - 0.5f) * cellSizeX, itemHeight * 0.5f * scale, (rz - 0.5f) * cellSizeZ);
				transform[index] = m;

				if (tint && index < instanceColor.size())
				{
					// Darkness correlates with scale (rr drives both), so the
					// short blades are the dark ones.
					const f32 t = rr;
					instanceColor[index] = Vec4(
						tintLow.x + (tintHigh.x - tintLow.x) * t,
						tintLow.y + (tintHigh.y - tintLow.y) * t,
						tintLow.z + (tintHigh.z - tintLow.z) * t,
						tintLow.w + (tintHigh.w - tintLow.w) * t);
				}
			}
		}

		UpdateTransforms();
		UpdateInstanceColors();
	}

	void RenderingInstancedComponent::UpdateTransforms()
	{
		transform_buffer->Buffer->Update(&transform[0], transform.size()*sizeof(Matrix));
	}

	RenderingInstancedComponent::~RenderingInstancedComponent()
	{
		RemoveBuffer(transform_buffer);
		delete transform_buffer;
		if (color_buffer != NULL)
		{
			RemoveBuffer(color_buffer);
			delete color_buffer;
		}
	}
};
