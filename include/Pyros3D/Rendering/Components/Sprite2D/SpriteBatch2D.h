//============================================================================
// Name        : SpriteBatch2D.h
// Description : Draws every Sprite2D beneath its object in as few draw calls
//               as their textures allow.
//
//               A sprite on its own is one draw call. That is fine for a
//               character and wrong for a field of coins, a bullet pattern or
//               a level dressed with four hundred pieces of scenery: each
//               draw is a pipeline bind, a descriptor bind and a uniform
//               write for four vertices. Put this on a parent - a Layer2D
//               root is the natural place - and the sprites under it are
//               merged into one mesh per run of sprites sharing a material,
//               in back-to-front order.
//
//               Order inside a batch is by sort order, then by depth, then by
//               position in the tree - the same rule the renderer applies to
//               separate sprites, so batching a layer does not reshuffle it.
//               The batch as a whole sorts against the rest of the scene as
//               one object, at its owner's depth: which is exactly what a
//               layer is.
//
//               A batch that nothing in changed costs nothing: the merged
//               mesh is kept. A sprite that moves, changes frame or changes
//               colour makes its batch re-merge that frame, which is a few
//               microseconds a sprite and one buffer upload.
//
//               Lit sprites are merged too, cut into cells. Lighting
//               picks the lights that reach each thing drawn, so one mesh
//               covering a whole level would be lit by whichever four
//               lights were nearest its middle; a cell a few units across
//               is lit by the lights near it, as each sprite in it would
//               have been. A sprite near a cell's edge can be reached by a
//               light the cell as a whole did not choose - the cell size is
//               that trade, and turning lit batching off is the other.
//============================================================================

#ifndef SPRITEBATCH2D_H
#define SPRITEBATCH2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class Sprite2D;
	class RenderingComponent;
	class IMaterial;
	class SceneGraph;
	class GameObject;

	class PYROS3D_API SpriteBatch2D : public IComponent {

	public:

		SpriteBatch2D();
		virtual ~SpriteBatch2D();
		static uint32 LiveCount();

		// Off, every sprite beneath goes back to drawing itself - for telling
		// whether a batch is the cause of something, without removing it.
		void SetEnabled(const bool e);
		bool IsEnabled() const { return enabled; }

		// Whether sprites that take lighting are merged as well (on unless
		// said otherwise), and how big the cells they are cut into are, in
		// the batch's own units.
		void SetBatchLit(const bool on) { if (on != batchLit) { batchLit = on; forceRebuild = true; } }
		bool BatchesLit() const { return batchLit; }
		void SetLightCellSize(const f32 size) { const f32 v = size > 0.25f ? size : 0.25f; if (v != lightCell) { lightCell = v; forceRebuild = true; } }
		f32 GetLightCellSize() const { return lightCell; }
		// The enabled batch `go` sits under (its own object counts), or NULL.
		static SpriteBatch2D* Above(GameObject* go);

		// What the last merge produced.
		uint32 SpriteCount() const { return (uint32)entries.size(); }
		uint32 DrawCalls() const { return (uint32)runMaterials.size(); }
		// How many times the mesh has been re-merged since the batch was made.
		uint32 Rebuilds() const { return rebuilds; }

		// Brings the batch up to date with the sprites beneath it. Called for
		// every batch of a scene just before that scene is drawn (see
		// Sprite2D::FlushScene), so a sprite moved by a script this frame is
		// drawn where it is this frame.
		void Flush();

		// Merges again at the next flush whatever the change counters say.
		void Refresh() { forceRebuild = true; }

		// From Sprite2D, which tells its batch what the batch cannot see by
		// looking at transforms.
		void NoteChanged(Sprite2D* s, const bool material);
		void Forget(Sprite2D* s);

		// Every live batch whose owner is in `scene`.
		static void FlushScene(SceneGraph* scene);
		// Whether `go` sits under an enabled batch (its own object counts).
		static bool EnabledAbove(GameObject* go);

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual bool NeedsUpdate() const { return false; }
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::SpriteBatch2D; }

	private:

		struct Entry {
			Sprite2D* sprite;
			uint32 revision;
			Matrix world;
		};

		void Collect(GameObject* go, std::vector<Entry> &out) const;
		void Release();
		RenderingComponent* FindSibling() const;

		bool meshOffByUs = false;
		bool enabled;
		bool batchLit;
		f32 lightCell;
		bool forceRebuild;
		uint32 rebuilds;
		// What the world's change counters read at the last look - see Flush.
		uint64 seenMoved, seenStructure, seenSprites;

		// The sprites merged last time, in tree order, with what each looked
		// like - so "did anything change" is a comparison and not a rebuild.
		std::vector<Entry> entries;
		// One per geometry on the sibling, in draw order.
		std::vector<std::shared_ptr<IMaterial> > runMaterials;
	};

};

#endif /* SPRITEBATCH2D_H */
