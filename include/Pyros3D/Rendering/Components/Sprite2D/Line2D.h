//============================================================================
// Name        : Line2D.h
// Description : A line with width, drawn through a list of points: a rope,
//               a laser, a lightning bolt, the outline of something, a path
//               on a map. Its width and its colour can run from one value at
//               its start to another at its end.
//
//               Or a trail. Told to, it stops being a list you give it and
//               becomes the path its object has just taken: a point laid
//               down every so far, each one fading and thinning until it is
//               gone - behind a bullet, a sword, a comet.
//
//               It holds the description; the ribbon of quads lives on a
//               sibling RenderingComponent it makes for itself, as a
//               sprite's does. Unlit; its colours are its vertices', so one
//               line is one draw call whatever it does. It can carry an
//               image along its length - the links of a chain, the strands
//               of a rope, the core of a beam - stretched once from end to
//               end or repeated every so far.
//============================================================================

#ifndef LINE2D_H
#define LINE2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class SceneGraph;
	class RenderingComponent;
	class IMaterial;
	struct SpriteMesh2D;

	class PYROS3D_API Line2D : public IComponent {

	public:

		Line2D();
		virtual ~Line2D();
		static uint32 LiveCount();

		// The points, in the object's local space. Fewer than two draw
		// nothing. Ignored while it is a trail, which makes its own.
		void SetPoints(const std::vector<Vec2> &pts);
		const std::vector<Vec2> &GetPoints() const { return points; }
		void AddPoint(const Vec2 &p);
		void SetPoint(const uint32 index, const Vec2 &p);
		void ClearPoints();
		// Joined back to its first point.
		void SetClosed(const bool c);
		bool IsClosed() const { return closed; }

		// Width at the start and at the end, in world units; in between it
		// runs evenly from one to the other. A trail's start is its newest
		// end - the object - and its end the oldest, about to vanish.
		void SetWidth(const f32 start, const f32 end);
		f32 GetStartWidth() const { return widthStart; }
		f32 GetEndWidth() const { return widthEnd; }
		void SetColors(const Vec4 &start, const Vec4 &end);
		const Vec4 &GetStartColor() const { return colorStart; }
		const Vec4 &GetEndColor() const { return colorEnd; }
		// Added to what is behind it and not laid over it: light.
		void SetAdditive(const bool a);
		bool IsAdditive() const { return additive; }
		void SetSortOrder(const int32 order);
		int32 GetSortOrder() const { return sortOrder; }

		// An image along the line: its u runs along the length, its v across
		// the width. `resolvedPath` is where it is on disk; `authoredPath`
		// what a scene file stores. Empty for none (a plain colour).
		// `repeatEvery` is how much of the line's length one copy of the
		// image covers, in world units; 0 stretches a single copy from one
		// end to the other. The vertex colours tint it.
		void SetTexture(const std::string &resolvedPath, const std::string &authoredPath);
		const std::string &GetTexturePath() const { return texturePath; }
		void SetTextureRepeat(const f32 repeatEvery);
		f32 GetTextureRepeat() const { return textureRepeat; }
		// Nearest filtering, for pixel art.
		void SetTextureNearest(const bool n);
		bool IsTextureNearest() const { return textureNearest; }

		// --- trail ----------------------------------------------------------
		// On, the line is where the object has been over the last `lifetime`
		// seconds, a point laid down each `minDistance` it travels. Only
		// while the game runs: in the editor a trail lays nothing down.
		void SetTrail(const bool on);
		bool IsTrail() const { return trail; }
		void SetTrailLifetime(const f32 seconds);
		f32 GetTrailLifetime() const { return lifetime; }
		void SetTrailMinDistance(const f32 d);
		f32 GetTrailMinDistance() const { return minDistance; }
		// Off, no new points are laid down and the ones there fade out: the
		// way to end a trail without it vanishing at once.
		void SetEmitting(const bool e) { emitting = e; }
		bool IsEmitting() const { return emitting; }
		// Forgets where it has been - after a teleport, so the trail does
		// not draw a line across the level.
		void ClearTrail();
		uint32 TrailPointCount() const { return (uint32)trailPoints.size(); }

		// The ribbon for a list of points, as quads: what is drawn. Public
		// for the tests, and for anything that wants the mesh without the
		// component. `ages` (0 newest .. 1 gone), when given, fades and
		// thins each point further - a trail's.
		// `repeatEvery` > 0 makes u the distance along the line over it
		// (for a repeating image); otherwise u runs 0 to 1 over the whole.
		static void BuildMesh(const std::vector<Vec2> &pts, const bool closed, const f32 widthStart, const f32 widthEnd,
			const Vec4 &colorStart, const Vec4 &colorEnd, const std::vector<f32>* ages, SpriteMesh2D &out,
			const f32 repeatEvery = 0.f);

		// Rebuilds the scene's changed lines. Called just before a scene is drawn.
		static void FlushScene(SceneGraph* scene);

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual bool NeedsUpdate() const { return trail; }
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Line2D; }

	private:

		void Rebuild();
		RenderingComponent* FindSibling() const;
		std::shared_ptr<IMaterial> Material();

		std::vector<Vec2> points;
		bool closed;
		f32 widthStart, widthEnd;
		Vec4 colorStart, colorEnd;
		bool additive;
		int32 sortOrder;
		std::string texturePath, textureResolved;
		f32 textureRepeat = 0.f;
		bool textureNearest = false;
		std::string builtKey;

		bool trail;
		bool emitting;
		f32 lifetime, minDistance;
		struct TrailPoint { Vec2 world; f32 age; };
		// The points laid down, newest first. The object's own position is
		// not one of them: it is the line's head, added when it is drawn.
		std::vector<TrailPoint> trailPoints;
		Vec2 lastLaid;
		bool haveLastLaid;
		f64 lastTime;
		bool haveLastTime;

		bool meshOffByUs = false;
		bool dirty;
		bool built;
		bool builtAdditive;
		std::shared_ptr<IMaterial> material;
	};

};

#endif /* LINE2D_H */
