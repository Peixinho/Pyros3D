//============================================================================
// Name        : SpriteGeometry2D.h
// Description : The geometry a sprite and a sprite batch both draw through.
//               Private to the two of them - it lives beside their sources,
//               not in the engine's public headers.
//============================================================================

#ifndef SPRITEGEOMETRY2D_H
#define SPRITEGEOMETRY2D_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <algorithm>
#include <vector>

namespace p3d {

		// Quads that change: one sprite's, or a whole batch's run of sprites
		// sharing a material.
		//
		// It is written whenever a sprite changes frame or anything under a
		// batch moves, so it does not free and recreate its buffers each
		// time - it keeps a vertex buffer with room to spare and overwrites
		// it (the stream path ParticleSystem uses). A hundred animated
		// sprites at ten frames a second were otherwise a thousand buffer
		// allocations a second. The
		// index buffer is the same pattern for every quad, so it is filled
		// once per size and never touched again: only the COUNT drawn
		// changes, and the renderer reads that from the CPU-side list.
		class SpriteGeometry2D : public PrimitiveGeometry {
		public:
			explicit SpriteGeometry2D(const SpriteMesh2D &m) : capacity(0), vertices(NULL) { Fill(m); }
			// With bounds given rather than measured: (minX, minY, maxX, maxY).
			// A sprite's are the rectangle it COVERS, not the box of the quads
			// this frame happens to have - a trimmed frame is smaller than
			// its source, and bounds that followed it would change on every
			// frame of an animation.
			SpriteGeometry2D(const SpriteMesh2D &m, const Vec4 &rect) : capacity(0), vertices(NULL) { Fill(m, &rect); }

			void Fill(const SpriteMesh2D &m, const Vec4* rect = NULL)
			{
				const uint32 quads = (uint32)(m.vertex.size() / 4);
				if (quads > capacity || vertices == NULL)
				{
					Dispose();
					vertices = NULL;
					// Room to grow, so a batch that gains a sprite a frame -
					// bullets being fired - reallocates a handful of times
					// and not every frame.
					// (a lone sprite is one quad, or nine, and stays that size)
					capacity = quads <= 9 ? std::max<uint32>(1, quads) : std::max<uint32>(16, quads + quads / 2);

					vertices = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
					vertices->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3, NULL, 0);
					vertices->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3, NULL, 0);
					vertices->AddAttribute("aTangent", Buffer::Attribute::Type::Vec3, NULL, 0);
					vertices->AddAttribute("aBitangent", Buffer::Attribute::Type::Vec3, NULL, 0);
					vertices->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2, NULL, 0);
					vertices->AddAttribute("aColor", Buffer::Attribute::Type::Vec4, NULL, 0);
					vertices->SendBuffer();
					Attributes.push_back(vertices);

					std::vector<__INDEX_C_TYPE__> all((size_t)capacity * 6);
					for (uint32 q = 0; q < capacity; q++)
					{
						const __INDEX_C_TYPE__ b = (__INDEX_C_TYPE__)(q * 4);
						all[q * 6 + 0] = b + 0; all[q * 6 + 1] = b + 1; all[q * 6 + 2] = b + 2;
						all[q * 6 + 3] = b + 2; all[q * 6 + 4] = b + 3; all[q * 6 + 5] = b + 0;
					}
					IndexBuffer = new GeometryBuffer(Buffer::Type::Index, Buffer::Draw::Static);
					IndexBuffer->Init(&all[0], (uint32)(sizeof(__INDEX_C_TYPE__) * all.size()));
					// New buffer handles - see IGeometry::buffersRevision.
					buffersRevision++;
				}

				// Interleaved exactly as the attributes were declared. Always
				// the whole capacity, so every upload is the same length and
				// stays on the buffer's cheap overwrite path.
				packed.assign((size_t)capacity * 4 * kFloatsPerVertex, 0.f);
				for (size_t v = 0; v < m.vertex.size(); v++)
				{
					f32* o = &packed[v * kFloatsPerVertex];
					o[0] = m.vertex[v].x; o[1] = m.vertex[v].y; o[2] = m.vertex[v].z;
					o[3] = m.normal[v].x; o[4] = m.normal[v].y; o[5] = m.normal[v].z;
					const Vec3 t = v < m.tangent.size() ? m.tangent[v] : Vec3(1.f, 0.f, 0.f);
					const Vec3 b = v < m.bitangent.size() ? m.bitangent[v] : Vec3(0.f, 1.f, 0.f);
					o[6] = t.x; o[7] = t.y; o[8] = t.z;
					o[9] = b.x; o[10] = b.y; o[11] = b.z;
					o[12] = m.texcoord[v].x; o[13] = m.texcoord[v].y;
					o[14] = m.color[v].x; o[15] = m.color[v].y; o[16] = m.color[v].z; o[17] = m.color[v].w;
				}
				vertices->Buffer->Update(&packed[0], (uint32)(packed.size() * sizeof(f32)));

				index.resize((size_t)quads * 6);
				for (uint32 q = 0; q < quads; q++)
				{
					const __INDEX_C_TYPE__ b = (__INDEX_C_TYPE__)(q * 4);
					index[q * 6 + 0] = b + 0; index[q * 6 + 1] = b + 1; index[q * 6 + 2] = b + 2;
					index[q * 6 + 3] = b + 2; index[q * 6 + 4] = b + 3; index[q * 6 + 5] = b + 0;
				}
				// Kept for anything that asks a geometry where its vertices
				// are - picking, bounds, debug draw.
				tVertex = m.vertex;
				tNormal = m.normal;

				minBounds = rect ? Vec3(rect->x, rect->y, 0.f) : m.minBounds;
				maxBounds = rect ? Vec3(rect->z, rect->w, 0.f) : m.maxBounds;
				BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
				BoundingSphereRadius = maxBounds.distance(BoundingSphereCenter);
			}

		private:
			static const size_t kFloatsPerVertex = 18;
			uint32 capacity;
			// Owned by Attributes (IGeometry::Dispose frees it).
			AttributeBuffer* vertices;
			std::vector<f32> packed;
		};


}

#endif /* SPRITEGEOMETRY2D_H */
