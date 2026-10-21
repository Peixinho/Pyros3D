//============================================================================
// Name        : OcclusionBuffer.h
// Description : What the big things in view hide - a depth picture drawn on
//               the CPU from them, every frame, and asked with a sphere.
//============================================================================

#ifndef OCCLUSIONBUFFER_H
#define OCCLUSIONBUFFER_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <map>
#include <vector>

namespace p3d {

	class RenderingComponent;

	// Software occlusion culling. The largest still things in view (walls, houses, rocks - the
	// occluders) are drawn into a small depth picture on the CPU, the distance to the nearest
	// surface at each pixel; a thing whose bounding sphere lies wholly behind that picture, at
	// every pixel it could cover, is not drawn. Made again every frame from where the camera is,
	// so nothing lags; it errs towards "can be seen" - an object is hidden only if all of its
	// screen rectangle, grown by a pixel, is covered by something nearer than its nearest point,
	// and nothing crossing the near plane is ever hidden.
	//
	// Occluders are drawn with their coarsest level of detail, at most kMaxOccluders a frame and
	// kTriangleBudget triangles, the largest on screen first; their world-space triangles are kept
	// while they stay where they are.
	class PYROS3D_API OcclusionBuffer {
	public:
		static const uint32 kWidth = 256, kHeight = 128;
		static const uint32 kMaxOccluders = 64;
		static const uint32 kTriangleBudget = 80000;
		static const uint32 kMaxOccluderTriangles = 20000;  // (a thing of more is not an occluder)

		// On unless said (PYROS_OCCLUSION=0, or this): a process-wide switch.
		static void SetEnabled(const bool on);
		static bool GetEnabled();

		// A new frame: from this eye, through this view and projection (perspective only).
		void Begin(const Matrix &view, const Matrix &projection, const Vec3 &eye, const f32 nearPlane);
		// May this hide others? Still, opaque, large enough, and simple enough (at its coarsest
		// level) to draw here.
		bool CanOcclude(RenderingComponent* rc, const f32 sphereRadius);
		// Drawn into the picture (its coarsest level, every mesh of it). Returns the triangles it took.
		uint32 Draw(RenderingComponent* rc);
		// Done drawing: the coarser pictures for asking are made.
		void Finish();
		// A sphere in the world: true when what was drawn hides all of it.
		bool Hidden(const Vec3 &centre, const f32 radius) const;

		bool Ready() const { return ready; }
		uint32 TrianglesDrawn() const { return trianglesDrawn; }
		uint32 OccludersDrawn() const { return occludersDrawn; }

	private:
		void DrawTriangle(const Vec4 &a, const Vec4 &b, const Vec4 &c);
		void Raster(const Vec4 &a, const Vec4 &b, const Vec4 &c);
		void RasterRows(const uint32 rowBegin, const uint32 rowEnd);
		// The frame's triangles on the picture, front-facing ones: x, y (pixels) and 1/w for each
		// corner - drawn in Finish, in bands of rows on every core.
		struct ScreenTriangle { f32 x[3], y[3], iw[3]; int16 minX, maxX, minY, maxY; };
		std::vector<ScreenTriangle> screen;
		Matrix viewProjection;
		Vec3 eye;
		f32 nearPlane = 0.1f, focalX = 1.f, focalY = 1.f;
		bool ready = false;
		uint32 trianglesDrawn = 0, occludersDrawn = 0;
		// depth[0] the picture (view distance of the nearest surface, +inf where none);
		// depth[k] each 2x2 of depth[k-1] as its farthest - what a rectangle is asked against
		std::vector<std::vector<f32> > depth;
		// world-space triangles of the occluders, while their objects stay put
		struct Kept { Matrix world; std::vector<Vec3> triangles; uint32 lastUsed = 0; };
		std::map<RenderingComponent*, Kept> kept;
		// whether a thing may occlude, at all (what it is made of does not change): asked once
		std::map<RenderingComponent*, bool> canOcclude;
		uint32 frame = 0;
	};
}

#endif /* OCCLUSIONBUFFER_H */
