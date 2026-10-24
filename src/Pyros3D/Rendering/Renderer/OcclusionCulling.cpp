//============================================================================
// Name        : OcclusionCulling.cpp
// Author      : Duarte Peixinho
// Description : What the frame leaves out because something in front of it
//               hides it - the big still things in view, and the ground.
//               Shared by the forward and deferred renderers.
//============================================================================

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace p3d {

	// What is itself under the ground - in a cave, a cellar dug into a hill - is not
	// hidden by the ground's horizon: the horizon is made from the heights of the
	// surface, and says only that there is hill between the eye and that place, which
	// of a thing inside the hill is always true. (From a cave's mouth, nothing in the
	// cave was drawn.) Asked only of what the horizon would hide, and kept by its place
	// in the list until it moves: the height of the ground is not looked up every frame.
	static bool UnderTheGround(SceneGraph* scene, const size_t k, const Vec4 &sphere, const size_t count)
	{
		static thread_local std::vector<Vec4> at;
		static thread_local std::vector<uint8> known;         // 0 not asked, 1 above, 2 under
		if (known.size() != count) { known.assign(count, 0); at.assign(count, Vec4()); }
		if (known[k] != 0 && at[k].x == sphere.x && at[k].y == sphere.y && at[k].z == sphere.z) return known[k] == 2;
		f32 ground = 0.f;
		const bool under = TerrainEditor::HeightAt(scene, sphere.x, sphere.z, ground) && sphere.y < ground - 0.3f;
		at[k] = sphere;
		known[k] = under ? 2 : 1;
		return under;
	}

	// inView: indices into the frame's list (rmesh), opaque and in the view. shown gets those that
	// are not hidden, in the same order; occluded counts the others.
	void IRenderer::HideOccluded(const std::vector<uint32> &inView, const Projection &projection, GameObject* Camera, SceneGraph* Scene, std::vector<uint32> &shown, uint32 &occluded)
	{
		shown.clear();
		if (Camera == NULL) { shown = inView; return; }
		// What the ground hides from the camera is left out of the picture (it
		// still casts its shadow: the shadow maps have their own lists).
		bool occluding = false;
		{
			PYROS_PROFILE_SCOPE("Occlusion.Update");
			occluding = terrainOcclusion.Update(Scene, Camera->GetDrawWorldPosition());
		}
		// What the big still things in view hide (OcclusionBuffer): the largest on screen drawn
		// into a small depth picture, the rest asked against it. Perspective views only.
		bool byOccluders = false;
		// It pays its way: drawing the occluders costs a few milliseconds of the main thread, and
		// over open ground (Erangel's title: 64 occluders, 3.7 ms, 16 000 triangles hidden) it hides
		// less than it costs. Thirty frames in a row hiding under kWorth triangles and it rests - made
		// only every fifteenth frame, to see whether the view has come where it pays (then every frame
		// again). Resting, everything is drawn: nothing can come out wrong, only slower or faster.
		// And looked again only if the view is not where it was when it last looked: what it would
		// hide is the camera's and what is in view's (the occluders are still things), so in a view
		// that has not changed the answer is the same - and the look was a spike in a still scene's
		// frame times every quarter second (2.3 ms of drawing occluders, the lists remade with it).
		static const uint32 kWorth = 50000, kQuietFrames = 30, kRestEvery = 15;
		occlusionTick++;
		bool resting = occlusionQuiet >= kQuietFrames && (occlusionTick % kRestEvery) != 0;
		if (occlusionQuiet >= kQuietFrames && !resting)
		{
			const Matrix &eye = Camera->GetDrawWorld();
			const Vec3 from = Camera->GetDrawWorldPosition(), along(-eye.m[8], -eye.m[9], -eye.m[10]);
			const bool same = inView.size() == occlusionLookedAt && from.distanceSQR(occlusionLookedFrom) < 0.25f * 0.25f
				&& along.dotProduct(occlusionLookedAlong) > 0.9995f;
			if (same) resting = true;
			else { occlusionLookedFrom = from; occlusionLookedAlong = along; occlusionLookedAt = inView.size(); }
		}
		else if (occlusionQuiet < kQuietFrames) occlusionLookedAt = (size_t)-1;
		FrameProfiler::Instance().Counter("Occlusion.Resting", resting ? 1.0 : 0.0);
		if (OcclusionBuffer::GetEnabled() && projection.m.m[15] == 0.f && !resting)
		{
			PYROS_PROFILE_SCOPE("Occlusion.Occluders");
			const Vec3 eyeAt = Camera->GetDrawWorldPosition();
			occlusionBuffer.Begin(Camera->GetDrawWorld().Inverse(), projection.m, eyeAt, projection.Near);
			// (one entry a thing, its size on screen: radius over distance)
			static thread_local std::vector<std::pair<f32, RenderingComponent*> > picks;
			picks.clear();
			RenderingComponent* last = NULL;
			for (size_t i = 0; i < inView.size(); i++)
			{
				const uint32 k = inView[i];
				RenderingComponent* rc = rmesh[k]->renderingComponent;
				if (rc == last || cullSphere[k].w <= 0.f) continue;
				last = rc;
				const Vec3 c(cullSphere[k].x, cullSphere[k].y, cullSphere[k].z);
				const f32 r = std::max(cullSphere[k].w, rc->GetOwner() ? rc->GetOwner()->GetBoundingSphereRadiusWorldSpace() : 0.f);
				if (!occlusionBuffer.CanOcclude(rc, r)) continue;
				const f32 dist = std::max(0.5f, sqrtf(eyeAt.distanceSQR(c)) - r * 0.5f);
				picks.push_back(std::make_pair(r / dist, rc));
			}
			FrameProfiler::Instance().Begin("Occlusion.Raster");
			std::sort(picks.begin(), picks.end(), [](const std::pair<f32, RenderingComponent*> &a, const std::pair<f32, RenderingComponent*> &b) { return a.first > b.first; });
			picks.erase(std::unique(picks.begin(), picks.end(), [](const std::pair<f32, RenderingComponent*> &a, const std::pair<f32, RenderingComponent*> &b) { return a.second == b.second; }), picks.end());
			for (size_t i = 0; i < picks.size() && i < OcclusionBuffer::kMaxOccluders; i++) occlusionBuffer.Draw(picks[i].second);
			occlusionBuffer.Finish();
			FrameProfiler::Instance().End();
			byOccluders = occlusionBuffer.Ready();
			FrameProfiler::Instance().Counter("Occlusion.Occluders", (f64)occlusionBuffer.OccludersDrawn());
			FrameProfiler::Instance().Counter("Occlusion.OccluderTris", (f64)occlusionBuffer.TrianglesDrawn());
		}
		uint32 hiddenByOccluders = 0, hiddenTriangles = 0;
		FrameProfiler::Instance().Begin("Occlusion.Test");
		for (size_t i = 0; i < inView.size(); i++)
		{
			const size_t k = inView[i];
			// (what is drawn over everything, depth test off - an editor icon - is never behind anything)
			if (rmesh[k]->Material && !rmesh[k]->Material->IsDepthTesting()) { shown.push_back((uint32)k); continue; }
			{
				if (byOccluders && cullSphere[k].w > 0.f && occlusionBuffer.Hidden(Vec3(cullSphere[k].x, cullSphere[k].y, cullSphere[k].z), cullSphere[k].w))
				{
					hiddenByOccluders++;
					if (rmesh[k]->Geometry) hiddenTriangles += (uint32)(rmesh[k]->Geometry->GetIndexData().size() / 3);
					occluded++;
					continue;
				}
				// (and not what the ground hides: behind a hill from here)
				if (occluding && cullSphere[k].w > 0.f && terrainOcclusion.Hidden(Vec3(cullSphere[k].x, cullSphere[k].y, cullSphere[k].z), cullSphere[k].w)
					&& !UnderTheGround(Scene, k, cullSphere[k], rmesh.size()))
				{
					// PYROS_VERIFY_OCCLUSION=1: every "hidden" held against the ground
					// itself - a line from the eye to the sphere's top, and to its top
					// at either side, has to go into the ground on the way.
					static const bool verify = std::getenv("PYROS_VERIFY_OCCLUSION") != NULL;
					if (verify)
					{
						static uint64 asked = 0, wrong = 0;
						const Vec3 eyeNow = Camera->GetDrawWorldPosition();
						const Vec4 &s = cullSphere[k];
						const Vec3 c(s.x, s.y, s.z);
						Vec3 side(-(c.z - eyeNow.z), 0.f, c.x - eyeNow.x);
						const f32 sl = sqrtf(side.x * side.x + side.z * side.z);
						if (sl > 1e-3f) side = side * (s.w / sl);
						const Vec3 tops[3] = { c + Vec3(0.f, s.w, 0.f), c + Vec3(0.f, s.w, 0.f) + side, c + Vec3(0.f, s.w, 0.f) - side };
						bool seen = false;
						for (int t = 0; t < 3; t++) if (!TerrainOcclusion::GroundBetween(Scene, eyeNow, tops[t])) seen = true;
						asked++;
						if (seen && ++wrong <= 20) fprintf(stderr, "[occlusion] WRONG: %s hidden, but a line to its top is clear (%.0f m off, radius %.1f)\n", rmesh[k]->renderingComponent->GetOwner()->GetName().c_str(), sqrtf(eyeNow.distanceSQR(c)), s.w);
						if (asked % 200000 == 0) fprintf(stderr, "[occlusion] %llu hidden checked, %llu wrong\n", (unsigned long long)asked, (unsigned long long)wrong);
					}
					occluded++; continue;
				}
				shown.push_back((uint32)k);
			}
		}
		FrameProfiler::Instance().End();
		FrameProfiler::Instance().Counter("Occlusion.ByOccluders", (f64)hiddenByOccluders);
		FrameProfiler::Instance().Counter("Occlusion.HiddenTris", (f64)hiddenTriangles);
		if (byOccluders) occlusionQuiet = hiddenTriangles >= kWorth ? 0 : occlusionQuiet + 1;
	}

}
