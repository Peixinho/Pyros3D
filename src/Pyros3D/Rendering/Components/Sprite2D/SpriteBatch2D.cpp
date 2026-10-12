//============================================================================
// Name        : SpriteBatch2D.cpp
// Description : See the header.
//============================================================================

#include <Pyros3D/Rendering/Components/Sprite2D/SpriteBatch2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include "SpriteGeometry2D.h"
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_set>

namespace p3d {

	namespace {

		std::atomic<int> gLiveBatch2D(0);

		std::vector<SpriteBatch2D*> &Batches()
		{
			static std::vector<SpriteBatch2D*> all;
			return all;
		}
		// A batch can be made on a loader thread, as any component can.
		std::mutex &BatchesLock()
		{
			static std::mutex m;
			return m;
		}

		class BatchRenderable : public Renderable {
		public:
			void AddRun(const SpriteMesh2D &m) { Geometries.push_back(new SpriteGeometry2D(m)); }
			void Finish() { CalculateBounding(); }
		};

		bool HasEnabledBatch(GameObject* go)
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t i = 0; i < comps.size(); i++)
				if (comps[i] && comps[i]->GetComponentType() == ComponentType::SpriteBatch2D
					&& static_cast<SpriteBatch2D*>(comps[i].get())->IsEnabled())
					return true;
			return false;
		}

	}

	uint32 SpriteBatch2D::LiveCount() { const int n = gLiveBatch2D.load(); return n > 0 ? (uint32)n : 0; }

	SpriteBatch2D::SpriteBatch2D()
		: enabled(true), batchLit(true), lightCell(8.f), forceRebuild(true), rebuilds(0), seenMoved(0), seenStructure(0), seenSprites(0)
	{
		gLiveBatch2D++;
		std::lock_guard<std::mutex> lock(BatchesLock());
		Batches().push_back(this);
	}

	SpriteBatch2D::~SpriteBatch2D()
	{
		gLiveBatch2D--;
		{
			std::lock_guard<std::mutex> lock(BatchesLock());
			std::vector<SpriteBatch2D*> &all = Batches();
			all.erase(std::remove(all.begin(), all.end(), this), all.end());
		}
		Release();
	}

	void SpriteBatch2D::Release()
	{
		// By value: SetBatched reaches back here through nothing, but the
		// list must not be the one being walked if that ever changes.
		const std::vector<Entry> was = entries;
		entries.clear();
		for (size_t i = 0; i < was.size(); i++)
			if (was[i].sprite && was[i].sprite->GetBatch() == this) was[i].sprite->SetBatched(NULL);
	}

	void SpriteBatch2D::SetEnabled(const bool e)
	{
		if (enabled == e) return;
		enabled = e;
		forceRebuild = true;
	}

	void SpriteBatch2D::NoteChanged(Sprite2D*, const bool)
	{
		// The revision each entry remembers is what actually detects the
		// change; this only has to exist so a sprite can say so without
		// knowing how the batch notices.
	}

	void SpriteBatch2D::Forget(Sprite2D* s)
	{
		for (size_t i = 0; i < entries.size(); i++)
			if (entries[i].sprite == s)
			{
				entries.erase(entries.begin() + (std::ptrdiff_t)i);
				forceRebuild = true;
				return;
			}
	}

	bool SpriteBatch2D::EnabledAbove(GameObject* go)
	{
		return Above(go) != NULL;
	}

	SpriteBatch2D* SpriteBatch2D::Above(GameObject* go)
	{
		for (GameObject* at = go; at != NULL; at = at->GetParent())
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = at->GetComponents();
			for (size_t i = 0; i < comps.size(); i++)
				if (comps[i] && comps[i]->GetComponentType() == ComponentType::SpriteBatch2D
					&& static_cast<SpriteBatch2D*>(comps[i].get())->IsEnabled())
					return static_cast<SpriteBatch2D*>(comps[i].get());
		}
		return NULL;
	}

	void SpriteBatch2D::FlushScene(SceneGraph* scene)
	{
		if (gLiveBatch2D.load() <= 0) return;
		// A copy: Flush() can add a RenderingComponent, and nothing it does
		// may be walking the list of batches.
		std::vector<SpriteBatch2D*> all;
		{
			std::lock_guard<std::mutex> lock(BatchesLock());
			all = Batches();
		}
		for (size_t i = 0; i < all.size(); i++)
		{
			GameObject* o = all[i]->GetOwner();
			if (o && o->GetOwningScene() == scene) all[i]->Flush();
		}
	}

	RenderingComponent* SpriteBatch2D::FindSibling() const
	{
		if (!Owner) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
				return static_cast<RenderingComponent*>(comps[i].get());
		return NULL;
	}

	void SpriteBatch2D::Collect(GameObject* go, std::vector<Entry> &out) const
	{
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
		{
			if (!comps[i] || comps[i]->GetComponentType() != ComponentType::Sprite2D) continue;
			Sprite2D* s = static_cast<Sprite2D*>(comps[i].get());
			if (!s->IsActive() || (s->IsLit() && !batchLit)) continue;
			Entry e;
			e.sprite = s;
			e.revision = s->GetRevision();
			e.world = go->GetWorldTransformation();
			out.push_back(e);
		}
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
		{
			if (!kids[i]) continue;
			// A batch further down draws its own subtree.
			if (HasEnabledBatch(kids[i].get())) continue;
			Collect(kids[i].get(), out);
		}
	}

	void SpriteBatch2D::Flush()
	{
		if (!Owner) return;
		RenderingComponent* rc = FindSibling();

		if (!enabled)
		{
			if (!entries.empty()) Release();
			if (rc && !runMaterials.empty()) { if (rc->IsActive()) { rc->Disable(); meshOffByUs = true; } runMaterials.clear(); }
			forceRebuild = true;
			return;
		}

		// Nothing anywhere moved, changed shape or changed a sprite since the
		// last look: the merged mesh is still right, and finding that out
		// costs three comparisons rather than a walk of everything beneath.
		const uint64 moved = RenderState::MovedCount();
		const uint64 structure = RenderState::Structure.load();
		const uint64 sprites = Sprite2D::ChangeCount();
		if (!forceRebuild && moved == seenMoved && structure == seenStructure && sprites == seenSprites) return;
		seenMoved = moved;
		seenStructure = structure;

		std::vector<Entry> now;
		now.reserve(entries.size());
		Collect(Owner, now);

		bool changed = forceRebuild || now.size() != entries.size();
		bool membership = now.size() != entries.size();
		for (size_t i = 0; i < now.size() && !membership; i++)
			if (now[i].sprite != entries[i].sprite) membership = true;
		if (membership) changed = true;
		for (size_t i = 0; i < now.size() && !changed; i++)
			if (now[i].revision != entries[i].revision
				|| memcmp(now[i].world.m, entries[i].world.m, sizeof(f32) * 16) != 0)
				changed = true;
		if (!changed) { seenSprites = sprites; return; }

		if (membership || forceRebuild)
		{
			std::unordered_set<Sprite2D*> present;
			for (size_t i = 0; i < now.size(); i++) present.insert(now[i].sprite);
			const std::vector<Entry> was = entries;
			// Before the hand-backs: SetBatched does not call Forget, but
			// entries must already describe the new set if anything asks.
			entries = now;
			for (size_t i = 0; i < was.size(); i++)
				if (!present.count(was[i].sprite) && was[i].sprite->GetBatch() == this)
					was[i].sprite->SetBatched(NULL);
			for (size_t i = 0; i < now.size(); i++)
				if (now[i].sprite->GetBatch() != this) now[i].sprite->SetBatched(this);
			// SetBatched bumps a sprite's revision; take the settled values.
			for (size_t i = 0; i < entries.size(); i++) entries[i].revision = entries[i].sprite->GetRevision();
		}
		else entries = now;
		forceRebuild = false;
		rebuilds++;
		// Read again: taking sprites into the batch touched them.
		seenSprites = Sprite2D::ChangeCount();

		// Back to front: by sort order, then by depth in the batch's own
		// space, then by position in the tree - the renderer's rule for
		// separate sprites.
		const Matrix inverse = Owner->GetWorldTransformation().Inverse();
		std::vector<Matrix> local(entries.size());
		std::vector<uint32> order(entries.size());
		for (size_t i = 0; i < entries.size(); i++)
		{
			local[i] = inverse * entries[i].world;
			order[i] = (uint32)i;
		}
		// A lit sprite's cell: where in the batch its origin is, in squares
		// lightCell across. Unlit ones are all in one - nothing picks lights
		// for them, so nothing is gained by cutting them up.
		std::vector<int64> cell(entries.size(), 0);
		bool anyLit = false;
		for (size_t i = 0; i < entries.size(); i++)
		{
			if (!entries[i].sprite->IsLit()) continue;
			anyLit = true;
			const int64 cx = (int64)floorf(local[i].m[12] / lightCell), cy = (int64)floorf(local[i].m[13] / lightCell);
			cell[i] = ((cx & 0x7fffffffll) << 32) | (cy & 0xffffffffll) | (1ll << 63);
		}
		const std::vector<Entry> &es = entries;
		std::stable_sort(order.begin(), order.end(), [&es, &local, &cell](const uint32 a, const uint32 b) {
			const int32 oa = es[a].sprite->GetSortOrder(), ob = es[b].sprite->GetSortOrder();
			if (oa != ob) return oa < ob;
			if (local[a].m[14] != local[b].m[14]) return local[a].m[14] < local[b].m[14];
			// Same order, same depth: which is drawn first was never said,
			// so the ones of a cell are put together and are one draw. Among
			// unlit sprites (cell 0 all) the order in the tree stands.
			return cell[a] < cell[b];
		});

		std::vector<SpriteMesh2D> runs;
		std::vector<std::shared_ptr<IMaterial> > mats;
		int64 runCell = 0;
		for (size_t k = 0; k < order.size(); k++)
		{
			Sprite2D* s = entries[order[k]].sprite;
			std::shared_ptr<IMaterial> mat = s->SharedMaterial();
			if (!mat) continue;
			if (mats.empty() || mats.back() != mat || runCell != cell[order[k]])
			{
				// Only made once a sprite has something to put in it: an
				// empty geometry is a crash, not an empty draw.
				SpriteMesh2D probe;
				if (!s->BuildMesh(probe, &local[order[k]])) continue;
				mats.push_back(mat);
				runs.push_back(probe);
				runCell = cell[order[k]];
				continue;
			}
			s->BuildMesh(runs.back(), &local[order[k]]);
		}

		if (runs.empty())
		{
			if (rc && !runMaterials.empty() && rc->IsActive()) { rc->Disable(); meshOffByUs = true; }
			runMaterials.clear();
			return;
		}

		if (!rc)
		{
			std::shared_ptr<RenderingComponent> made = std::make_shared<RenderingComponent>(
				std::static_pointer_cast<Renderable>(std::make_shared<Plane>(1.f, 1.f)), mats[0]);
			made->DisableCastShadows();
			Owner->Add(std::static_pointer_cast<IComponent>(made));
			rc = made.get();
			runMaterials.clear();
		}

		Renderable* r = rc->GetRenderable();
		const bool inPlace = !runMaterials.empty() && runMaterials == mats
			&& r && r->Geometries.size() == runs.size();
		if (inPlace)
		{
			for (size_t i = 0; i < runs.size(); i++)
				static_cast<SpriteGeometry2D*>(r->Geometries[i])->Fill(runs[i]);
		}
		else
		{
			std::shared_ptr<BatchRenderable> built = std::make_shared<BatchRenderable>();
			for (size_t i = 0; i < runs.size(); i++) built->AddRun(runs[i]);
			built->Finish();
			// Not cull-tested. The merged mesh's extent follows whatever is
			// under the batch and changes as they move; keeping the owner's
			// box current would mean re-registering every frame, to save
			// drawing quads the GPU clips for nothing.
			rc->DisableCullTest();
			rc->AdoptGeneratedRenderable(built, mats);
		}
		// Each run's own bounds are what its lights are chosen by.
		rc->SetLightsPerGeometry(anyLit);
		const bool hidden = Layer2D::HiddenAbove(Owner);
		// Only what it switched off itself is switched back on: the mesh is
		// a RenderingComponent like any other, and a game may have its own
		// reasons to have turned it off.
		if (hidden) { if (rc->IsActive()) { rc->Disable(); meshOffByUs = true; } }
		else if (meshOffByUs) { if (!rc->IsActive()) rc->Enable(); meshOffByUs = false; }
		runMaterials = mats;
	}

};
