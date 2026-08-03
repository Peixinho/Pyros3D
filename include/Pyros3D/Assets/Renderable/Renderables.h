//============================================================================
// Name        : Renderables.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderables
//============================================================================

#ifndef RENDERABLES_H
#define RENDERABLES_H

#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Utils/ModelLoaders/IModelLoader.h>
#include <Pyros3D/Other/Export.h>
#include <map>
#include <vector>
#include <list>
#include <Pyros3D/Other/Global.h>

namespace p3d {

	// Store Each Submesh Material Properties
	struct PYROS3D_API MaterialProperties
	{
		// Material ID
		uint32 id;
		// Material Name
		std::string Name;
		// Properties
		Vec4 Color;
		Vec4 Specular;
		Vec4 Ambient;
		Vec4 Emissive;
		bool WireFrame;
		bool Twosided;
		f32 Opacity;
		f32 Shininess;
		f32 ShininessStrength;
		// textures        
		std::string colorMap;
		std::string specularMap;
		std::string normalMap;
		// flags
		bool haveColor;
		bool haveSpecular;
		bool haveAmbient;
		bool haveEmissive;
		bool haveColorMap;
		bool haveSpecularMap;
		bool haveNormalMap;
		bool haveBones;

		MaterialProperties() : haveColor(false), haveSpecular(false), haveAmbient(false), haveColorMap(false), haveEmissive(false), haveNormalMap(false), haveSpecularMap(false), Opacity(1.0f), Shininess(0.0f), ShininessStrength(0.0f), WireFrame(false), Twosided(false), haveBones(false), id(0) {}
	};

	// Vertex Attribute Struct
	struct PYROS3D_API VertexAttribute {
		// Attribute Name
		std::string Name;
		// Attribute Type
		uint32 Type;
		// Attribute Offset
		uint32 Offset;
		// Type Offset
		uint32 byteSize;
		// Attribute Data
		std::vector<uchar> Data;
		// Data Length
		uint32 DataLength;
		// Vertex Divisor
		uint32 VertexDivisor;

		VertexAttribute() {};
		// ID = -2 because 0 is the first and -1 is "not found"         
		VertexAttribute(const std::string &name, uint32 type, void* data, const uint32 &length, const uint32 vertexDivisor = 0) : Name(name), Type(type), DataLength(length), VertexDivisor(vertexDivisor)
		{
			// Copy Data
			switch (Type)
			{
			case Buffer::Attribute::Type::Float:
				byteSize = sizeof(f32);
				break;
			case Buffer::Attribute::Type::Int:
				byteSize = sizeof(int32);
				break;
			case Buffer::Attribute::Type::Short:
				byteSize = sizeof(short);
				break;
			case Buffer::Attribute::Type::Matrix:
				byteSize = sizeof(Matrix);
				break;
			case Buffer::Attribute::Type::Vec2:
				byteSize = sizeof(Vec2);
				break;
			case Buffer::Attribute::Type::Vec3:
				byteSize = sizeof(Vec3);
				break;
			case Buffer::Attribute::Type::Vec4:
				byteSize = sizeof(Vec4);
				break;
			};
			if (data != NULL)
			{
				Data.resize(DataLength*byteSize);
				memcpy(&Data[0], data, DataLength*byteSize);
			}
			else {
				Data.resize(0);
			}
		}
	};

	struct PYROS3D_API Bone;

	class PYROS3D_API AttributeArray
	{

	public:
		// Attributes List
		std::vector<VertexAttribute*> Attributes;

		// Virtual so deleting through an AttributeArray* (IGeometry::Dispose()
		// stores AttributeBuffer instances as AttributeArray*) properly runs
		// the derived destructor instead of invoking undefined behavior. Also
		// frees Attributes directly as a safety net for objects destroyed
		// without Dispose() ever being called; harmless no-op if Dispose()
		// already cleared the list.
		virtual ~AttributeArray()
		{
			for (std::vector<VertexAttribute*>::iterator k = Attributes.begin(); k != Attributes.end(); k++)
				delete *k;
		}

		void AddAttribute(const std::string &name, const uint32 &type, void* data, const uint32 &length, const uint32 vertexDivisor = 0)
		{
			VertexAttribute* v(new VertexAttribute(name, type, data, length, vertexDivisor));
			Attributes.push_back(v);
		}

		virtual void SendBuffer() {}

		virtual void Dispose()
		{
			// Loop Through each Vertex Attribute and Delete Them
			for (std::vector<VertexAttribute*>::iterator k = Attributes.begin(); k != Attributes.end(); k++)
			{
				// Delete Vertex Attribute
				delete *k;
			}
			// Clear so a repeat Dispose(), or the destructor running after
			// Dispose() already freed these, can't double-delete them.
			Attributes.clear();
		}
	};

	// Attributes Buffer
	class PYROS3D_API AttributeBuffer : public AttributeArray {
	public:
		// Buffer
		GeometryBuffer* Buffer;
		uint32 bufferType;
		uint32 bufferDraw;
		// Data
		std::vector<uchar> Data;
		// Offset
		uint32 BufferOffset;
		// Attributes Size
		uint32 attributeSize;

		AttributeBuffer() : attributeSize(0), Buffer(NULL) {}
		AttributeBuffer(const uint32 &type, const uint32 &draw) : attributeSize(0), Buffer(NULL) { bufferDraw = draw; bufferType = type; }

		// Safety net for objects deleted without Dispose() ever being called
		// (e.g. RenderingInstancedComponent deletes its transform_buffer
		// directly); harmless no-op if Dispose() already freed and NULLed
		// Buffer.
		virtual ~AttributeBuffer()
		{
			delete Buffer;
		}

		virtual void SendBuffer()
		{
			uint32 offset = 0;
			uint32 count = 0;
			for (std::vector<VertexAttribute*>::iterator k = Attributes.begin(); k != Attributes.end(); k++)
			{
				(*k)->Offset = offset;
				offset += (*k)->byteSize;
				count = (*k)->DataLength;
			}
			BufferOffset = offset;

			// Resize Data
			Data.resize(BufferOffset*count);
			// Run through attributes data            
			for (uint32 l = 0; l < count; l++)
			{
				offset = BufferOffset*l;
				// Run through all attributes
				for (std::vector<VertexAttribute*>::iterator k = Attributes.begin(); k != Attributes.end(); k++)
				{
					memcpy(&Data[offset + (*k)->Offset], &(*k)->Data[(l*(*k)->byteSize)], (*k)->byteSize);
				}
			}
			// Create Buffer
			Buffer = new GeometryBuffer(bufferType, bufferDraw);
			// Send Buffer
			Buffer->Init((Data.size()>0?&Data[0]:NULL), Data.size());
		}

		virtual void Dispose()
		{
			// Delete Vertex Attributes and clear the list (AttributeArray::Dispose()
			// does both). Must clear, not just delete: IGeometry::Dispose()
			// always calls Dispose() immediately followed by `delete` on this
			// object, which runs ~AttributeArray() - if Attributes still held
			// the now-deleted pointers, that would double-free them.
			AttributeArray::Dispose();
			// Delete GPU Buffer (created in SendBuffer(); NULL if never sent)
			delete Buffer;
			Buffer = NULL;
		}
	};

	// Geometry Interface Keeps Index and Attributes Buffer
	// Creates a Simple Material Properties
	class PYROS3D_API IGeometry {

	public:

		// Index Data
		std::vector<__INDEX_C_TYPE__> index;

		// Index Buffer - if is used
		GeometryBuffer* IndexBuffer;

		// Attributes Buffer
		std::vector<AttributeArray*> Attributes;

		// Bumped every time SendBuffers() hands a fresh set of GPU buffers to
		// this geometry. Anything that caches state derived from those
		// buffers' handles - IRenderer::BindMesh()'s per-shader VAO cache
		// above all - has to compare this against the revision it cached at
		// and rebuild when they differ. Without it, a geometry that is
		// disposed and rebuilt in place (Text::UpdateText() is the one that
		// does this today) keeps being drawn through a VAO still pointing at
		// the freed buffers: the index count updates, because it is read
		// from CPU-side data, while the vertex data does not. The visible
		// symptom is new text drawn with the previous string's glyphs.
		uint32 buffersRevision;

		IGeometry() : IndexBuffer(NULL), buffersRevision(0) {

			// Internal ID
			ID = _InternalID++;
		}

		virtual ~IGeometry() {}

		// Material Properties
		MaterialProperties materialProperties;

		// Virtual Methods
		virtual const std::vector<__INDEX_C_TYPE__> &GetIndexData() const = 0;
		virtual const std::vector<Vec3> &GetVertexData() const = 0;
		virtual const std::vector<Vec3> &GetNormalData() const = 0;
		virtual const f32 &GetBoundingSphereRadius() const { return BoundingSphereRadius; }
		virtual const Vec3 &GetBoundingSphereCenter() const { return BoundingSphereCenter; }
		virtual const Vec3 &GetBoundingMinValue() const { return minBounds; }
		virtual const Vec3 &GetBoundingMaxValue() const { return maxBounds; }

		virtual void CalculateBounding() = 0;

		virtual void SendBuffers()
		{
			// create and send index buffer
			IndexBuffer = new GeometryBuffer(Buffer::Type::Index, Buffer::Draw::Static);
			IndexBuffer->Init(&index[0], sizeof(__INDEX_C_TYPE__)*index.size());

			// send attribute buffers
			for (std::vector<AttributeArray*>::iterator i = Attributes.begin(); i != Attributes.end(); i++)
			{
				AttributeBuffer* bf = (AttributeBuffer*)(*i);
				bf->SendBuffer();
			}

			// New buffer handles - see buffersRevision's declaration.
			buffersRevision++;
		}

		virtual void Dispose()
		{
			// Delete Index Buffer
			delete IndexBuffer;
			IndexBuffer = NULL;
			// Loop Through Attributes Buffer and Delete Each One
			for (std::vector<AttributeArray*>::iterator i = Attributes.begin(); i != Attributes.end(); i++)
			{
				if ((*i)!=NULL) 
				{
					// Dipose Vertex Attributes
					(*i)->Dispose();
					// Delete Pointer
					delete *i;
				}
			}
			Attributes.clear();
		}

		uint32 GetInternalID() { return ID; }

		// Map Bone ID's
		std::map<int32, int32> MapBoneIDs;
		// Bone Offset Matrix
		std::map<int32, Matrix> BoneOffsetMatrix;

	protected:

		f32 BoundingSphereRadius;
		Vec3 BoundingSphereCenter;
		Vec3 maxBounds, minBounds;

		// Internal ID
		static uint32 _InternalID;
		uint32 ID;
	};

	// Keeps the Geometry List
	class PYROS3D_API Renderable {

	public:
		
		Renderable() {}
		virtual ~Renderable()
		{
			for (std::vector<IGeometry*>::iterator i = Geometries.begin(); i != Geometries.end(); i++)
			{
				// Dispose Buffer
				(*i)->Dispose();
				// Delete Pointer
				delete (*i);
			}
		}

		// Vector of Buffers
		std::vector<IGeometry*> Geometries;

		virtual void CalculateBounding()
		{
			// Get Bounding Boxes
			for (std::vector<IGeometry*>::iterator i = Geometries.begin(); i != Geometries.end(); i++)
			{
				if (i == Geometries.begin())
				{
					maxBounds = (*i)->GetBoundingMaxValue();
					minBounds = (*i)->GetBoundingMinValue();
				}
				else {
					if ((*i)->GetBoundingMaxValue().x > maxBounds.x) maxBounds.x = (*i)->GetBoundingMaxValue().x;
					if ((*i)->GetBoundingMaxValue().y > maxBounds.y) maxBounds.y = (*i)->GetBoundingMaxValue().y;
					if ((*i)->GetBoundingMaxValue().z > maxBounds.z) maxBounds.z = (*i)->GetBoundingMaxValue().z;

					if ((*i)->GetBoundingMinValue().x < minBounds.x) minBounds.x = (*i)->GetBoundingMinValue().x;
					if ((*i)->GetBoundingMinValue().y < minBounds.y) minBounds.y = (*i)->GetBoundingMinValue().y;
					if ((*i)->GetBoundingMinValue().z < minBounds.z) minBounds.z = (*i)->GetBoundingMinValue().z;
				}
			}
			// Set Sphere Radius and Bounds
			BoundingSphereCenter = Vec3::ZERO;
			f32 a = maxBounds.distance(BoundingSphereCenter);
			f32 b = minBounds.distance(BoundingSphereCenter);
			BoundingSphereRadius = Max(a, b);
		}

		// Bounds of the Whole Model
		virtual f32 &GetBoundingSphereRadius() { return BoundingSphereRadius; }
		virtual Vec3 &GetBoundingSphereCenter() { return BoundingSphereCenter; }
		virtual Vec3 &GetBoundingMinValue() { return minBounds; }
		virtual Vec3 &GetBoundingMaxValue() { return maxBounds; }

		// Get Skeleton
		const std::map<StringID, Bone> &GetSkeleton() const { return skeleton; }

	protected:

		// Bounds of the Whole Model
		f32 BoundingSphereRadius;
		Vec3 BoundingSphereCenter;
		Vec3 maxBounds, minBounds;

		// Skeleton
		std::map<StringID, Bone> skeleton;

	};
};

#endif /* RENDERABLES_H */
