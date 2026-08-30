//============================================================================
// Name        : UIBatcher.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Merges a canvas's draw list into as few draws as it can
//============================================================================

#ifndef UIBATCHER_H
#define UIBATCHER_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <vector>

namespace p3d {

	// PrimitiveGeometry plus a per-vertex tint. The tint has to leave the
	// material to make batching possible at all: a uniform is per draw
	// call, so anything that varies per element and stays a uniform is a
	// draw call.
	class PYROS3D_API UIBatchGeometry : public PrimitiveGeometry {
	public:
		std::vector<Vec4> tColor;
		// aColor is only declared when there is tint data to declare it
		// with - Vulkan wants every shader input bound, and a text batch
		// (whose colour rides on aNormal already) declares no aColor.
		void CreateBatchBuffers();
	};

	// A Renderable whose contents are replaced every time the batch is
	// rebuilt. The same in-place Dispose/refill/Build cycle UIQuad and Text
	// use, so the RenderingMesh keeps pointing at one geometry object for
	// its whole life while the data underneath changes size freely.
	class PYROS3D_API UIBatchRenderable : public Primitive {
	public:
		UIBatchRenderable();
		UIBatchGeometry* batch;
		void Rebuild();
	};

	// One merged draw: its own renderable, its own material, and an owner
	// at the origin - batched vertices are already in canvas space, so the
	// model matrix is the identity.
	class PYROS3D_API UIBatch {
	public:
		UIBatch();
		~UIBatch();

		std::shared_ptr<UIBatchRenderable> renderable;
		std::shared_ptr<RenderingComponent> component;
		std::shared_ptr<GameObject> owner;
	};

	class PYROS3D_API UIBatcher {
	public:

		UIBatcher();
		~UIBatcher();

		// Order-preserving merge of `source`. Consecutive elements that
		// share a texture (images) or a font atlas (labels) become one
		// mesh; anything else passes through untouched, in place, so the
		// painted result is identical either way.
		//
		// Rebuilds only when something actually changed - the signature of
		// every source mesh is compared against the last call's, which is
		// cheap scalar work next to the draws it saves.
		const std::vector<RenderingMesh*> &Build(const std::vector<RenderingMesh*> &source);

		// How many of the last Build()'s draws were merged batches rather
		// than pass-through meshes. For tests and for the editor's stats.
		uint32 GetBatchCount() const { return batchCount; }

		void Invalidate() { signatures.clear(); }

	private:

		// What a rebuild would depend on. Compared byte-wise against the
		// previous frame's.
		struct Signature {
			RenderingMesh* mesh;
			void* key;
			uint32 revision;
			uint32 kind;
			f32 matrix[16];
			Vec4 tint;
		};

		std::vector<UIBatch*> pool;
		std::vector<RenderingMesh*> result;
		std::vector<Signature> signatures;
		uint32 batchCount;

		UIBatch* TakeBatch(const uint32 index);
	};

}

#endif /* UIBATCHER_H */
