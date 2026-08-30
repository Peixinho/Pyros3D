//============================================================================
// Name        : UIBatcher
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Merges a canvas's draw list into as few draws as it can
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIBatcher.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <cstring>

namespace p3d {

	void UIBatchGeometry::CreateBatchBuffers()
	{
		CalculateBounding();
		if (tVertex.empty()) return;

		AttributeBuffer* Vertex = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);
		Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3, &tVertex[0], tVertex.size());
		Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3, &tNormal[0], tNormal.size());
		Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2, &tTexcoord[0], tTexcoord.size());
		if (!tColor.empty())
			Vertex->AddAttribute("aColor", Buffer::Attribute::Type::Vec4, &tColor[0], tColor.size());
		Attributes.push_back(Vertex);
	}

	UIBatchRenderable::UIBatchRenderable() : Primitive()
	{
		// Primitive's constructor allocates a plain PrimitiveGeometry;
		// swap in one that can carry tint. Nothing has been built yet at
		// this point, so there is nothing to migrate.
		delete geometry;
		batch = new UIBatchGeometry();
		geometry = batch;
		Rebuild();
	}

	void UIBatchRenderable::Rebuild()
	{
		if (!Geometries.empty())
		{
			batch->Dispose();
			Geometries.clear();
		}

		batch->CreateBatchBuffers();
		batch->SendBuffers();

		batch->materialProperties.haveColor = true;
		batch->materialProperties.haveBones = false;
		batch->materialProperties.haveSpecular = false;
		batch->materialProperties.haveColorMap = false;
		batch->materialProperties.haveSpecularMap = false;
		batch->materialProperties.haveNormalMap = false;
		batch->materialProperties.Color = Vec4(1.f, 1.f, 1.f, 1.f);

		Geometries.push_back(batch);

		// Screen-space and never culled, but RenderingMesh still reads
		// these - leave them wide rather than wrong.
		minBounds = Vec3(-1e6f, -1e6f, -1.f);
		maxBounds = Vec3(1e6f, 1e6f, 1.f);
		BoundingSphereCenter = Vec3(0.f, 0.f, 0.f);
		BoundingSphereRadius = 1e6f;
	}

	UIBatch::UIBatch()
	{
		renderable = std::make_shared<UIBatchRenderable>();
		// A throwaway material: Build() assigns the real one, which depends
		// on what ended up in the batch.
		std::shared_ptr<IMaterial> placeholder(new GenericShaderMaterial(ShaderUsage::Texture | ShaderUsage::VertexColor));
		component = std::make_shared<RenderingComponent>(
			std::static_pointer_cast<Renderable>(renderable), placeholder);
		component->SetRenderLayer(RenderLayer::UI);
		component->DisableCullTest();
		component->DisableCastShadows();

		// Deliberately not in any scene: UIRenderer walks the canvas's own
		// list, not the scene's, and a batch is an artifact of drawing
		// rather than a thing the scene contains. The owner exists only to
		// give RenderObject() a model matrix, and that matrix is identity.
		owner = std::make_shared<GameObject>();
		owner->Add(std::static_pointer_cast<IComponent>(component));
		owner->RefreshTransformation();
	}

	UIBatch::~UIBatch() {}

	UIBatcher::UIBatcher() { batchCount = 0; rebuildCount = 0; }

	UIBatcher::~UIBatcher()
	{
		for (size_t i = 0; i < pool.size(); i++) delete pool[i];
		pool.clear();
	}

	UIBatch* UIBatcher::TakeBatch(const uint32 index)
	{
		while (pool.size() <= index) pool.push_back(new UIBatch());
		return pool[index];
	}

	// What decides whether two neighbours can share a draw. Images key on
	// their texture, labels on their font atlas; the two kinds never mix
	// because they are different shader variants.
	namespace {

		enum { KIND_NONE = 0, KIND_IMAGE, KIND_TEXT };

		// One element of the source list, already in canvas space.
		struct Item {
			RenderingMesh* mesh;
			uint32 kind;
			void* key;
			Vec4 tint;
			std::vector<Vec3> vertex;
			f32 x0, y0, x1, y1;
			Item() : mesh(NULL), kind(KIND_NONE), key(NULL), x0(0.f), y0(0.f), x1(0.f), y1(0.f) {}
		};

		// The elements that will share one draw, and the rectangle they
		// cover between them - conservative on purpose, a union is cheap to
		// test and only ever refuses a merge that would have been legal.
		struct Group {
			uint32 kind;
			void* key;
			f32 x0, y0, x1, y1;
			std::vector<size_t> items;

			Group(const uint32 kind, void* key, const Item &first, const size_t index)
				: kind(kind), key(key), x0(first.x0), y0(first.y0), x1(first.x1), y1(first.y1)
			{
				items.push_back(index);
			}

			void Add(const Item &it, const size_t index)
			{
				items.push_back(index);
				if (it.x0 < x0) x0 = it.x0;
				if (it.y0 < y0) y0 = it.y0;
				if (it.x1 > x1) x1 = it.x1;
				if (it.y1 > y1) y1 = it.y1;
			}

			// The union first, as a rejection test: most groups are
			// nowhere near the element and answer in four comparisons. A
			// union that does overlap is usually a false alarm - a group
			// spanning half the canvas has holes all through it - so the
			// members get asked individually before believing it.
			bool Overlaps(const Item &it, const std::vector<Item> &all) const
			{
				if (it.x1 <= x0 || it.x0 >= x1 || it.y1 <= y0 || it.y0 >= y1) return false;
				// Past a certain size the per-member scan costs more than
				// the draw call it might save.
				if (items.size() > 64) return true;
				for (size_t i = 0; i < items.size(); i++)
				{
					const Item &o = all[items[i]];
					if (!(it.x1 <= o.x0 || it.x0 >= o.x1 || it.y1 <= o.y0 || it.y0 >= o.y1)) return true;
				}
				return false;
			}
		};

		bool GeometryOf(RenderingMesh* m, const uint32 kind,
			const std::vector<Vec3>* &verts, const std::vector<Vec3>* &normals,
			const std::vector<Vec2>* &uvs, const std::vector<__INDEX_C_TYPE__>* &idx)
		{
			if (m == NULL || m->Geometry == NULL) return false;
			if (kind == KIND_IMAGE)
			{
				PrimitiveGeometry* g = static_cast<PrimitiveGeometry*>(m->Geometry);
				verts = &g->tVertex; normals = &g->tNormal; uvs = &g->tTexcoord; idx = &g->index;
			}
			else
			{
				TextGeometry* g = static_cast<TextGeometry*>(m->Geometry);
				verts = &g->tVertex; normals = &g->tNormal; uvs = &g->tTexcoord; idx = &g->index;
			}
			return !verts->empty() && !idx->empty();
		}

		uint32 KindOf(RenderingMesh* m, void* &keyOut, Vec4 &tintOut)
		{
			keyOut = NULL;
			tintOut = Vec4(1.f, 1.f, 1.f, 1.f);
			if (m == NULL || m->renderingComponent == NULL) return KIND_NONE;
			const uint32 type = m->renderingComponent->GetComponentType();
			if (type == ComponentType::UIImage)
			{
				UIImage* img = static_cast<UIImage*>(m->renderingComponent);
				keyOut = img->GetTexture().get();
				tintOut = img->GetDisplayTint();
				return KIND_IMAGE;
			}
			if (type == ComponentType::UIText)
			{
				UIText* txt = static_cast<UIText*>(m->renderingComponent);
				keyOut = txt->GetFont().get();
				return KIND_TEXT;
			}
			return KIND_NONE;
		}

		// `first` is any element of the batch - they all share the texture
		// or font this keys on, which is what put them in one batch.
		std::shared_ptr<IMaterial> MakeBatchMaterial(const uint32 kind, RenderingMesh* first)
		{
			GenericShaderMaterial* mat;
			if (kind == KIND_TEXT)
			{
				Font* font = static_cast<UIText*>(first->renderingComponent)->GetFont().get();
				mat = new GenericShaderMaterial(ShaderUsage::TextRendering
					| ((font && font->IsSDF()) ? ShaderUsage::TextSDF : 0));
				mat->SetTextFont(font);
			}
			else
			{
				mat = new GenericShaderMaterial(ShaderUsage::Texture | ShaderUsage::VertexColor);
				// The tint moved to the vertices, so the colormap is all
				// the material still carries.
				mat->SetColorMap(static_cast<UIImage*>(first->renderingComponent)->GetTexture());
			}
			// Same screen-space contract every UI material has - see
			// UIImage.cpp, which is where these came from.
			mat->EnableBlending();
			mat->BlendingEquation(BlendEq::Add);
			mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
			mat->DisableDepthTest();
			mat->DisableDepthWrite();
			mat->SetTransparencyFlag(true);
			mat->SetCullFace(CullFace::DoubleSided);
			mat->DisableCastingShadows();
			return std::shared_ptr<IMaterial>(mat);
		}

	}

	const std::vector<RenderingMesh*> &UIBatcher::Build(const std::vector<RenderingMesh*> &source)
	{
		// ---- has anything changed? ----
		std::vector<Signature> now;
		now.reserve(source.size());
		for (size_t i = 0; i < source.size(); i++)
		{
			RenderingMesh* m = source[i];
			Signature s;
			memset(&s, 0, sizeof(s));
			s.mesh = m;
			s.kind = KindOf(m, s.key, s.tint);
			s.revision = (m && m->Geometry) ? m->Geometry->buffersRevision : 0;
			if (m && m->renderingComponent && m->renderingComponent->GetOwner())
			{
				const Matrix model = m->renderingComponent->GetOwner()->GetWorldTransformation() * m->Pivot;
				memcpy(s.matrix, model.m, sizeof(s.matrix));
			}
			now.push_back(s);
		}

		if (now.size() == signatures.size() &&
			(now.empty() || memcmp(&now[0], &signatures[0], now.size() * sizeof(Signature)) == 0))
			return result;

		signatures.swap(now);
		rebuildCount++;

		// ---- every element, in canvas space ----
		//
		// Transformed once here rather than once per draw by the vertex
		// shader's model matrix, which is the other half of what batching
		// buys: the batches themselves sit at the origin.
		std::vector<Item> items;
		items.resize(source.size());
		for (size_t i = 0; i < source.size(); i++)
		{
			Item &it = items[i];
			it.mesh = source[i];
			it.kind = KindOf(source[i], it.key, it.tint);
			it.x0 = it.y0 = 1e30f;
			it.x1 = it.y1 = -1e30f;
			if (it.kind == KIND_NONE) continue;

			const std::vector<Vec3>* verts; const std::vector<Vec3>* normals;
			const std::vector<Vec2>* uvs; const std::vector<__INDEX_C_TYPE__>* idx;
			if (!GeometryOf(it.mesh, it.kind, verts, normals, uvs, idx)) { it.kind = KIND_NONE; continue; }

			// Pivot and not just the owner's transform: a label's alignment
			// rides on the mesh pivot rather than its owner's position (see
			// UIText::Realign), so a batch that dropped it drew every label
			// left-aligned at the top of its rect.
			const Matrix world = it.mesh->renderingComponent->GetOwner()->GetWorldTransformation() * it.mesh->Pivot;
			it.vertex.reserve(verts->size());
			for (size_t v = 0; v < verts->size(); v++)
			{
				const Vec3 p = world * (*verts)[v];
				it.vertex.push_back(p);
				if (p.x < it.x0) it.x0 = p.x;
				if (p.y < it.y0) it.y0 = p.y;
				if (p.x > it.x1) it.x1 = p.x;
				if (p.y > it.y1) it.y1 = p.y;
			}
		}

		// ---- assign elements to batches ----
		//
		// Merging only immediate neighbours would find almost nothing: a
		// panel, its label, its icon and the next panel alternate textures,
		// so no two elements in a row ever match. An element may instead
		// join an earlier batch, as long as nothing drawn between the two
		// overlaps it - moving it earlier in the paint order is then
		// invisible. Order within a batch is the index buffer's order, and
		// a draw call rasterizes its primitives in that order, so elements
		// that do overlap inside one batch still blend correctly.
		std::vector<Group> groups;
		for (size_t i = 0; i < items.size(); i++)
		{
			const Item &it = items[i];
			if (it.kind == KIND_NONE || it.vertex.empty())
			{
				// Not ours to merge: it stays exactly where it is, and
				// blocks anything after it from moving past it.
				groups.push_back(Group(KIND_NONE, NULL, it, i));
				continue;
			}

			int join = -1;
			for (int b = (int)groups.size() - 1; b >= 0; b--)
			{
				if (groups[b].kind == it.kind && groups[b].key == it.key) { join = b; break; }
				// Something between here and there covers the same pixels
				// and is painted after this element would be.
				if (groups[b].Overlaps(it, items)) break;
			}

			if (join >= 0) groups[join].Add(it, i);
			else groups.push_back(Group(it.kind, it.key, it, i));
		}

		// ---- build ----
		result.clear();
		batchCount = 0;

		for (size_t gi = 0; gi < groups.size(); gi++)
		{
			Group &g = groups[gi];
			// One element on its own is never cheaper as a batch: same
			// draw call, plus a copy of its geometry.
			if (g.items.size() < 2)
			{
				result.push_back(items[g.items[0]].mesh);
				continue;
			}

			UIBatch* b = TakeBatch(batchCount);
			UIBatchGeometry* geo = b->renderable->batch;
			geo->index.clear();
			geo->tVertex.clear();
			geo->tNormal.clear();
			geo->tTexcoord.clear();
			geo->tColor.clear();

			for (size_t e = 0; e < g.items.size(); e++)
			{
				const Item &it = items[g.items[e]];
				const uint32 base = (uint32)geo->tVertex.size();

				const std::vector<Vec3>* verts; const std::vector<Vec3>* normals;
				const std::vector<Vec2>* uvs; const std::vector<__INDEX_C_TYPE__>* idx;
				GeometryOf(it.mesh, it.kind, verts, normals, uvs, idx);

				for (size_t v = 0; v < it.vertex.size(); v++)
				{
					geo->tVertex.push_back(it.vertex[v]);
					geo->tNormal.push_back(v < normals->size() ? (*normals)[v] : Vec3(0.f, 0.f, 1.f));
					geo->tTexcoord.push_back(v < uvs->size() ? (*uvs)[v] : Vec2(0.f, 0.f));
					// Labels already carry their colour per-vertex, on
					// aNormal - see Text.cpp - so only images need this.
					if (g.kind == KIND_IMAGE) geo->tColor.push_back(it.tint);
				}
				for (size_t n = 0; n < idx->size(); n++)
					geo->index.push_back(base + (*idx)[n]);
			}

			b->renderable->Rebuild();
			std::vector<RenderingMesh*> &meshes = b->component->GetMeshes();
			std::shared_ptr<IMaterial> mat = MakeBatchMaterial(g.kind, items[g.items[0]].mesh);
			for (size_t m = 0; m < meshes.size(); m++)
			{
				meshes[m]->Material = mat;
				result.push_back(meshes[m]);
			}
			batchCount++;
		}

		return result;
	}

}
