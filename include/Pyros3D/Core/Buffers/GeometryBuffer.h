//============================================================================
// Name        : GeometryBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GeometryBuffer
//============================================================================

#ifndef GEOMETRYBUFFER_H
#define GEOMETRYBUFFER_H

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#include <atomic>
#include <vector>
#include <string.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <iostream>

namespace p3d {

	namespace Buffer {

		namespace Attribute {

			// Data type
			namespace Type {
				enum {
					Int = 0,
					Short,
					Float,
					Vec2,
					Vec3,
					Vec4,
					Matrix
				};
			};

			// Usage of the Data
			namespace Usage {
				enum {
					Pos = 0,
					TexCoord,
					Color,
					Normal,
					Tangent,
					Bitangent,
					Other
				};
			};

			const uint32 GetTypeSize(const uint32 type);
			const uint32 GetTypeCount(const uint32 type);

		};

		// Type of Draw
		namespace Draw {
			enum {
				Static = 0,
				Dynamic,
				Stream
			};
		};

		// Type of Buffer
		namespace Type {
			enum {
				Index = 0,
				Vertex,
				Attribute
			};
		};

		// Type of Mapping
		namespace Mapping {
			enum {
				Write = 0,
				Read,
				ReadAndWrite
			};
		};
	};

	class PYROS3D_API GeometryBuffer {

	private:

		uint32 bufferType;
		uint32 bufferDraw;

	public:

		// Data Size
		uint32 DataLength;

		// The bytes, kept on the CPU: the renderer makes its own GPU copy of them (no GPU buffer is made here), and
		// Map() gives this memory
		std::vector<uchar> CpuData;
		// Changes with them: what the renderer keeps on the GPU of a geometry is made again when it moves (a sprite
		// animating writes its quads into the same buffer each frame, its handles unchanged)
		uint32 cpuRevision = 0;
		// Every write of any buffer's bytes, counted, and the last kWrittenRing of them: which buffers. A renderer reads
		// what was written since it last looked, and knows every other geometry's bounds are as they were without
		// looking at its buffers - tens of thousands of meshes asked of their buffers every frame was a third of a
		// millisecond. (Addresses only, never followed: a buffer gone since is no one's.)
		static const uint32 kWrittenRing = 2048;
		// (atomic: a loading thread writes buffers too)
		static std::atomic<uint64> s_anyWritten;
		static std::atomic<GeometryBuffer*> s_written[kWrittenRing];
		static void NoteWritten(GeometryBuffer* b) { const uint64 i = s_anyWritten.fetch_add(1, std::memory_order_relaxed); s_written[i % kWrittenRing].store(b, std::memory_order_release); }

		// A process that draws nothing (a dedicated server): buffers know their length but keep none of
		// their bytes, which are only ever read by a renderer to make its GPU copy. A server's meshes
		// otherwise sat in memory twice more than the copy physics and scripts read (ModelGeometry's
		// arrays): once per attribute and once laid out for the GPU. Map() then gives NULL.
		static void SetHeadless(const bool on);
		static bool IsHeadless();

		GeometryBuffer();
		GeometryBuffer(const uint32 bufferType, const uint32 bufferDraw);
		~GeometryBuffer();
		// methods
		void Init(const void *GeometryData, const uint32 length);
		// (the data handed over, not copied)
		void InitOwned(std::vector<uchar> &&GeometryData);

		void *Map(const uint32 MappingType = 1);
		void Unmap();

		void Update(const void *GeometryData, const uint32 length = 0);

	};

}

#endif	/* GEOMETRYBUFFER_H */
