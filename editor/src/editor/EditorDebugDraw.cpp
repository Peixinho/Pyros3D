#include "EditorDebugDraw.h"

#include <algorithm>
#include <cmath>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>

using namespace p3d;

static const Vec4 kLightOverlayColor = Vec4(1.0f, 1.0f, 0.0f, 1.0f);
static const Vec4 kCameraOverlayColor = Vec4(0.0f, 1.0f, 1.0f, 1.0f);
static const Vec4 kActiveCameraFrustumColor = Vec4(0.2f, 1.0f, 0.2f, 1.0f);

EditorDebugDraw::EditorDebugDraw() {}
EditorDebugDraw::~EditorDebugDraw() {}

void EditorDebugDraw::ToggleForComponent(IComponent* comp) {
	if (!comp) return;
	if (compsHidden.count(comp)) compsHidden.erase(comp);
	else compsHidden.insert(comp);
}

bool EditorDebugDraw::IsOn(IComponent* comp) const { return compsHidden.count(comp) == 0; }

void EditorDebugDraw::ToggleForCamera(GameObject* camGO) {
	if (!camGO) return;
	if (camerasHidden.count(camGO)) camerasHidden.erase(camGO);
	else camerasHidden.insert(camGO);
}

bool EditorDebugDraw::IsCameraOn(GameObject* camGO) const { return camerasHidden.count(camGO) == 0; }

void EditorDebugDraw::ForgetCamera(GameObject* camGO) {
	if (!camGO) return;
	camerasHidden.erase(camGO);
}

void EditorDebugDraw::ToggleNormalsForRenderingComponent(IComponent* comp) {
	if (!comp) return;
	if (renderingNormalsOn.count(comp)) renderingNormalsOn.erase(comp);
	else renderingNormalsOn.insert(comp);
}

bool EditorDebugDraw::IsNormalsOn(IComponent* comp) const { return renderingNormalsOn.count(comp) > 0; }

void EditorDebugDraw::ForgetComponent(IComponent* comp) {
	if (!comp) return;
	compsHidden.erase(comp);
	renderingNormalsOn.erase(comp);
}

static void drawCircle(DebugRenderer* dbg, const Vec3& center, const Vec3& normal, float radius, const Vec4& color, int segments = 48) {
	Vec3 n = normal.normalize();
	Vec3 basis = fabs(n.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
	Vec3 x = (basis.cross(n)).normalize();
	Vec3 y = (n.cross(x)).normalize();
	Vec3 prev = center + x * radius;
	for (int i = 1; i <= segments; i++) {
		float a = (2.0f * 3.1415926f * i) / segments;
		Vec3 p = center + x * (cosf(a) * radius) + y * (sinf(a) * radius);
		dbg->drawLine(prev, p, color);
		prev = p;
	}
}

static float worldSizeForPixels(float distance, float fovDeg, float viewportHeightPixels, float pixels) {
	float fov = (float)DEGTORAD(fovDeg);
	float worldPerPixel = 2.0f * distance * tanf(fov * 0.5f) / std::max(1.0f, viewportHeightPixels);
	return worldPerPixel * pixels;
}

static bool shouldSkipGO(GameObject* go, GameObject* skipA, GameObject* skipB, GameObject* skipC) {
	return go == skipA || go == skipB || go == skipC;
}

// An arc of the circle around `center` in the plane spanned by the unit
// vectors x and y, from angle a0 to a1 (radians, measured from x toward y).
static void drawArc(DebugRenderer* dbg, const Vec3& center, const Vec3& x, const Vec3& y,
	float radius, float a0, float a1, const Vec4& color, int segments = 24)
{
	Vec3 prev = center + x * (cosf(a0) * radius) + y * (sinf(a0) * radius);
	for (int i = 1; i <= segments; i++) {
		float a = a0 + (a1 - a0) * ((float)i / segments);
		Vec3 p = center + x * (cosf(a) * radius) + y * (sinf(a) * radius);
		dbg->drawLine(prev, p, color);
		prev = p;
	}
}

// A light's direction is stored in its owner's LOCAL space - every renderer
// transforms it by the owner's world matrix before use (see
// ForwardRenderer/DeferredRenderer). Drawing the raw vector showed where the
// light would point on an unrotated object, so rotating a light with the
// gizmo left its arrow/cone pointing the old way while the lighting moved.
static Vec3 lightWorldDirection(GameObject* go, const Vec3& localDir) {
	Vec3 d = (go->GetWorldTransformation() * Vec4(localDir, 0.f)).xyz();
	return d.magnitude() > 1e-6f ? d.normalize() : Vec3(0, 0, -1);
}

// The camera's basis straight from its world matrix: the frustum rolls with
// the camera. This used to rebuild "up" from a fixed world-Y hint, which
// dropped any roll the camera had and produced NaNs (the cross product
// collapses) for a camera looking straight up or down - a top-down camera,
// the most common 2.5D setup, drew no frustum at all.
static void cameraBasis(GameObject* cam, Vec3& fwd, Vec3& right, Vec3& up) {
	const Matrix& M = cam->GetWorldTransformation();
	fwd = Vec3(-M.m[8], -M.m[9], -M.m[10]).normalize();
	up = Vec3(M.m[4], M.m[5], M.m[6]).normalize();
	right = fwd.cross(up).normalize();
	up = right.cross(fwd).normalize();
}

static void drawRect(DebugRenderer* dbg, const Vec3& c, const Vec3& right, const Vec3& up,
	float hw, float hh, const Vec4& color, Vec3* corners = NULL)
{
	Vec3 tl = c + up * hh - right * hw;
	Vec3 tr = c + up * hh + right * hw;
	Vec3 bl = c - up * hh - right * hw;
	Vec3 br = c - up * hh + right * hw;
	dbg->drawLine(tl, tr, color);
	dbg->drawLine(tr, br, color);
	dbg->drawLine(br, bl, color);
	dbg->drawLine(bl, tl, color);
	if (corners) { corners[0] = tl; corners[1] = tr; corners[2] = br; corners[3] = bl; }
}

// `markerDist` places a small camera "body" - an image plane with an up
// triangle over it - at a distance that reads on screen. The volume itself
// runs to the far plane, which at the default 2000 is kilometres away: the
// edges fan out of view and nothing in sight says which way is up or where
// the camera's picture is. Blender draws its cameras the same way.
static void drawFrustum(DebugRenderer* dbg, GameObject* cam, float fovDeg, float aspect,
	float n, float f, float markerDist, const Vec4& color,
	bool orthographic, float orthoSize)
{
	Vec3 pos = cam->GetWorldPosition();
	Vec3 fwd, right, up;
	cameraBasis(cam, fwd, right, up);

	// An orthographic camera's volume is a box, not a pyramid: the near and
	// far faces are the same size. orthoSize is the half-height, matching
	// RenderCameraPreview's Ortho() call.
	const float tanHalf = tanf((float)DEGTORAD(fovDeg) * 0.5f);
	auto halfH = [&](float d) { return orthographic ? orthoSize : tanHalf * d; };

	Vec3 nearC[4], farC[4];
	drawRect(dbg, pos + fwd * n, right, up, halfH(n) * aspect, halfH(n), color, nearC);
	drawRect(dbg, pos + fwd * f, right, up, halfH(f) * aspect, halfH(f), color, farC);
	for (int i = 0; i < 4; i++) dbg->drawLine(nearC[i], farC[i], color);

	// The body. A perspective camera's edges meet at the eye, so they are
	// drawn back to it from the marker plane; an ortho box has no apex.
	float d = std::min(std::max(markerDist, n), f);
	float hh = halfH(d), hw = hh * aspect;
	Vec3 mc = pos + fwd * d;
	Vec3 mk[4];
	drawRect(dbg, mc, right, up, hw, hh, color, mk);
	if (!orthographic)
		for (int i = 0; i < 4; i++) dbg->drawLine(pos, mk[i], color);
	float t = std::min(hw, hh) * 0.5f;
	Vec3 t0 = mc + up * (hh * 1.1f) - right * t;
	Vec3 t1 = mc + up * (hh * 1.1f) + right * t;
	Vec3 t2 = mc + up * (hh * 1.1f + t * 0.9f);
	dbg->drawLine(t0, t1, color);
	dbg->drawLine(t1, t2, color);
	dbg->drawLine(t2, t0, color);
}

void EditorDebugDraw::Draw(DebugRenderer* dbg, SceneGraph* sg, GameObject* viewCam,
	float fovDeg, float aspect, p3d::uint32 viewportHeight,
	GameObject* skipA, GameObject* skipB, GameObject* skipC,
	const std::vector<SceneCameraDebugEntry>* sceneCameras,
	IComponent* selectedComponent, GameObject* selectedCamera)
{
	if (!dbg || !viewCam || !sg) return;
	drawLightGizmos(dbg, viewCam, fovDeg, aspect, viewportHeight, sg, skipA, skipB, skipC, sceneCameras,
		selectedComponent, selectedCamera);
}

void EditorDebugDraw::drawLightGizmos(DebugRenderer* dbg, GameObject* viewCam, float fovDeg, float aspect,
	p3d::uint32 viewportHeight, SceneGraph* sg,
	GameObject* skipA, GameObject* skipB, GameObject* skipC,
	const std::vector<SceneCameraDebugEntry>* sceneCameras,
	IComponent* selectedComponent, GameObject* selectedCamera)
{
	(void)aspect;
	const Vec3 camPos = viewCam->GetWorldPosition();
	const float kMinIconPixels = 128.0f;

	if (sceneCameras && showGizmoLines) {
		for (std::vector<SceneCameraDebugEntry>::const_iterator ci = sceneCameras->begin(); ci != sceneCameras->end(); ++ci) {
			GameObject* camGO = ci->go;
			// Looking through a camera, its own frustum is the viewport edge:
			// drawing it is at best invisible and at worst its near plane
			// ruled across the middle of the picture.
			if (!camGO || camGO == viewCam) continue;
			const bool selected = (camGO == selectedCamera);
			if (!selected && !(showAllCameraFrustums && IsCameraOn(camGO))) continue;
			const float dist = (camGO->GetWorldPosition() - camPos).magnitude();
			const float marker = worldSizeForPixels(std::max(dist, 0.001f), fovDeg, (float)viewportHeight, 60.0f);
			const Vec4& c = ci->isViewCamera ? kActiveCameraFrustumColor : kCameraOverlayColor;
			drawFrustum(dbg, camGO, ci->settings.fov, ci->aspect,
				ci->settings.nearPlane, ci->settings.farPlane, marker, c,
				ci->settings.orthographic, ci->settings.orthoSize);
		}
	}

	// Recursive, for the same reason DrawSceneViewportIcons is: a child added
	// with GameObject::Add() never appears in GetAllGameObjectList(), so in a
	// layered scene this drew no light volumes, no spot cones and no normals
	// for anything.
	std::vector<GameObject*> all;
	sg->CollectGameObjectsRecursive(all);
	for (std::vector<GameObject*>::iterator it = all.begin(); it != all.end(); ++it) {
		GameObject* go = *it;
		if (!go || shouldSkipGO(go, skipA, skipB, skipC)) continue;

		const std::vector<std::shared_ptr<IComponent>>& comps = go->GetComponents();
		for (std::vector<std::shared_ptr<IComponent>>::const_iterator ci = comps.begin(); ci != comps.end(); ++ci) {
			IComponent* c = (*ci).get();
			if (!c) continue;

			if (dynamic_cast<RenderingComponent*>(c)) {
				if (!IsNormalsOn(c)) continue;
				RenderingComponent* rc = (RenderingComponent*)c;
				Renderable* rend = rc->GetRenderable();
				if (!rend) continue;
				const Matrix& M = go->GetWorldTransformation();
				const Vec4 col = Vec4(0.2f, 0.8f, 1.0f, 1.0f);
				const float len = 0.25f;
				for (std::vector<IGeometry*>::iterator gi = rend->Geometries.begin(); gi != rend->Geometries.end(); ++gi) {
					IGeometry* geo = *gi;
					if (!geo) continue;
					const std::vector<Vec3>& verts = geo->GetVertexData();
					const std::vector<__INDEX_C_TYPE__>& idx = geo->GetIndexData();
					if (!idx.empty()) {
						for (size_t i = 0; i + 2 < idx.size(); i += 3) {
							size_t i0 = (size_t)idx[i], i1 = (size_t)idx[i + 1], i2 = (size_t)idx[i + 2];
							if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;
							Vec3 wv0 = M * verts[i0];
							Vec3 wv1 = M * verts[i1];
							Vec3 wv2 = M * verts[i2];
							Vec3 center = (wv0 + wv1 + wv2) * (1.0f / 3.0f);
							Vec3 wn = (wv1 - wv0).cross(wv2 - wv0).normalize();
							dbg->drawLine(center, center + wn * len, col);
						}
					} else {
						for (size_t i = 0; i + 2 < verts.size(); i += 3) {
							Vec3 wv0 = M * verts[i];
							Vec3 wv1 = M * verts[i + 1];
							Vec3 wv2 = M * verts[i + 2];
							Vec3 center = (wv0 + wv1 + wv2) * (1.0f / 3.0f);
							Vec3 wn = (wv1 - wv0).cross(wv2 - wv0).normalize();
							dbg->drawLine(center, center + wn * len, col);
						}
					}
				}
				continue;
			}

			if (!showGizmoLines) continue;
			if (c != selectedComponent && !(showAllLightGizmos && IsOn(c))) continue;

			const float dist = (go->GetWorldPosition() - camPos).magnitude();
			const float minWorld = worldSizeForPixels(std::max(dist, 0.001f), fovDeg, (float)viewportHeight, kMinIconPixels);

			if (DirectionalLight* dl = dynamic_cast<DirectionalLight*>(c)) {
				// A sun has no position that matters and no volume, so what
				// is drawn is its direction: a disc facing the light with
				// parallel rays running out of it - parallel because that is
				// what makes it read as a directional light rather than a
				// spot - and an arrowhead on the central ray. Sized in screen
				// pixels; the old single thin arrow was easy to miss.
				Vec3 pos = go->GetWorldPosition();
				Vec3 dir = lightWorldDirection(go, dl->GetLightDirection());
				Vec3 upRef = fabs(dir.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
				Vec3 right = (upRef.cross(dir)).normalize();
				Vec3 up = (dir.cross(right)).normalize();
				const float r = 0.3f * minWorld;
				const float rayLen = 0.9f * minWorld;
				drawCircle(dbg, pos, dir, r, kLightOverlayColor, 32);
				drawCircle(dbg, pos, dir, r * 0.5f, kLightOverlayColor, 24);
				for (int i = 0; i < 8; i++) {
					float a = (2.0f * 3.1415926f * i) / 8;
					Vec3 o = pos + (right * cosf(a) + up * sinf(a)) * r;
					dbg->drawLine(o, o + dir * (rayLen * 0.7f), kLightOverlayColor);
				}
				Vec3 tip = pos + dir * rayLen;
				dbg->drawLine(pos, tip, kLightOverlayColor);
				float ah = 0.15f * rayLen;
				for (int i = 0; i < 4; i++) {
					float a = (2.0f * 3.1415926f * i) / 4;
					Vec3 side = (right * cosf(a) + up * sinf(a)) * (0.5f * ah);
					dbg->drawLine(tip, tip - dir * ah + side, kLightOverlayColor);
				}
			} else if (PointLight* pl = dynamic_cast<PointLight*>(c)) {
				// Three great circles plus the silhouette facing the viewer.
				// DebugRenderer::drawSphere draws meridians only, all meeting
				// at the poles: dense at the top and bottom, and no outline at
				// all, so the radius - the one thing this is here to show -
				// was hard to read off it.
				Vec3 pos = go->GetWorldPosition();
				float R = pl->GetLightRadius();
				drawCircle(dbg, pos, Vec3(1, 0, 0), R, kLightOverlayColor);
				drawCircle(dbg, pos, Vec3(0, 1, 0), R, kLightOverlayColor);
				drawCircle(dbg, pos, Vec3(0, 0, 1), R, kLightOverlayColor);
				Vec3 toEye = camPos - pos;
				float dEye = toEye.magnitude();
				if (dEye > R * 1.001f) {
					// The visible outline of a sphere seen in perspective is
					// a smaller circle nearer the eye, not a great circle.
					float off = R * R / dEye;
					float rs = sqrtf(std::max(R * R - off * off, 0.0f));
					Vec3 n = toEye * (1.0f / dEye);
					drawCircle(dbg, pos + n * off, n, rs, kLightOverlayColor);
				}
			} else if (SpotLight* sl = dynamic_cast<SpotLight*>(c)) {
				// The lit volume is a cone of SLANT length R capped by a piece
				// of the radius sphere: attenuation is by distance from the
				// light (1 - d/R), not by depth along the axis. The cone angles
				// are HALF angles - SetLightOutterCone stores cos(angle) and
				// the shader compares it against dot(axis, L) directly - so
				// they are not halved again here; this used to draw every
				// spot at half its real width.
				Vec3 pos = go->GetWorldPosition();
				Vec3 fwd = lightWorldDirection(go, sl->GetLightDirection());
				float R = sl->GetLightRadius();
				float outer = std::min((float)DEGTORAD(sl->GetLightOutterCone()), 3.1415926f * 0.5f);
				float inner = std::min((float)DEGTORAD(sl->GetLightInnerCone()), outer);
				Vec3 upRef = fabs(fwd.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
				Vec3 right = (upRef.cross(fwd)).normalize();
				Vec3 up = (fwd.cross(right)).normalize();
				drawCircle(dbg, pos + fwd * (R * cosf(outer)), fwd, R * sinf(outer), kLightOverlayColor, 36);
				if (inner > 1e-3f && inner < outer - 1e-3f) {
					const Vec4 innerColor = Vec4(0.6f, 0.6f, 0.0f, 1.0f);
					drawCircle(dbg, pos + fwd * (R * cosf(inner)), fwd, R * sinf(inner), innerColor, 36);
				}
				const Vec3 sides[2] = { right, up };
				for (int i = 0; i < 2; i++) {
					drawArc(dbg, pos, fwd, sides[i], R, -outer, outer, kLightOverlayColor);
					dbg->drawLine(pos, pos + (fwd * cosf(outer) + sides[i] * sinf(outer)) * R, kLightOverlayColor);
					dbg->drawLine(pos, pos + (fwd * cosf(outer) - sides[i] * sinf(outer)) * R, kLightOverlayColor);
				}
			}
		}
	}
}
