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

static void drawFrustum(DebugRenderer* dbg, const Vec3& pos, const Vec3& forward, const Vec3& upHint,
	float fovDeg, float aspect, float n, float f, const Vec4& color,
	bool orthographic = false, float orthoSize = 10.f)
{
	Vec3 fwd = forward.normalize();
	Vec3 right = (fwd.cross(upHint)).normalize();
	Vec3 up = (right.cross(fwd)).normalize();

	// An orthographic camera's volume is a box, not a pyramid: the near and
	// far faces are the same size. Drawing it as a frustum anyway would
	// show the wrong shape for the projection the camera actually uses.
	float nh, nw, fh, fw;
	if (orthographic)
	{
		nh = fh = orthoSize;
		nw = fw = orthoSize * aspect;
	}
	else
	{
		float fov = (float)DEGTORAD(fovDeg);
		nh = tanf(fov * 0.5f) * n;
		nw = nh * aspect;
		fh = tanf(fov * 0.5f) * f;
		fw = fh * aspect;
	}

	Vec3 nc = pos + fwd * n;
	Vec3 fc = pos + fwd * f;

	Vec3 ntl = nc + up * nh - right * nw;
	Vec3 ntr = nc + up * nh + right * nw;
	Vec3 nbl = nc - up * nh - right * nw;
	Vec3 nbr = nc - up * nh + right * nw;

	Vec3 ftl = fc + up * fh - right * fw;
	Vec3 ftr = fc + up * fh + right * fw;
	Vec3 fbl = fc - up * fh - right * fw;
	Vec3 fbr = fc - up * fh + right * fw;

	dbg->drawLine(ntl, ntr, color);
	dbg->drawLine(ntr, nbr, color);
	dbg->drawLine(nbr, nbl, color);
	dbg->drawLine(nbl, ntl, color);
	dbg->drawLine(ftl, ftr, color);
	dbg->drawLine(ftr, fbr, color);
	dbg->drawLine(fbr, fbl, color);
	dbg->drawLine(fbl, ftl, color);
	dbg->drawLine(ntl, ftl, color);
	dbg->drawLine(ntr, ftr, color);
	dbg->drawLine(nbl, fbl, color);
	dbg->drawLine(nbr, fbr, color);
}

void EditorDebugDraw::Draw(DebugRenderer* dbg, SceneGraph* sg, GameObject* viewCam,
	float fovDeg, float aspect, p3d::uint32 viewportHeight,
	GameObject* skipA, GameObject* skipB, GameObject* skipC,
	const std::vector<SceneCameraDebugEntry>* sceneCameras)
{
	if (!dbg || !viewCam || !sg) return;
	drawLightGizmos(dbg, viewCam, fovDeg, aspect, viewportHeight, sg, skipA, skipB, skipC, sceneCameras);
}

void EditorDebugDraw::drawLightGizmos(DebugRenderer* dbg, GameObject* viewCam, float fovDeg, float aspect,
	p3d::uint32 viewportHeight, SceneGraph* sg,
	GameObject* skipA, GameObject* skipB, GameObject* skipC,
	const std::vector<SceneCameraDebugEntry>* sceneCameras)
{
	(void)aspect;
	const Vec3 camPos = viewCam->GetWorldPosition();
	const float kMinIconPixels = 128.0f;

	if (showCameraFrustum && sceneCameras) {
		for (std::vector<SceneCameraDebugEntry>::const_iterator ci = sceneCameras->begin(); ci != sceneCameras->end(); ++ci) {
			GameObject* camGO = ci->go;
			if (!camGO || !IsCameraOn(camGO)) continue;
			Vec3 pos = camGO->GetWorldPosition();
			Vec3 forward = (camGO->GetDirection() * -1.0f);
			const Vec4& c = ci->isViewCamera ? kActiveCameraFrustumColor : kCameraOverlayColor;
			drawFrustum(dbg, pos, forward, Vec3(0, 1, 0), ci->settings.fov, aspect,
				ci->settings.nearPlane, ci->settings.farPlane, c,
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

			if (!IsOn(c)) continue;

			const float dist = (go->GetWorldPosition() - camPos).magnitude();
			const float minWorld = worldSizeForPixels(std::max(dist, 0.001f), fovDeg, (float)viewportHeight, kMinIconPixels);

			if (DirectionalLight* dl = dynamic_cast<DirectionalLight*>(c)) {
				Vec3 pos = go->GetWorldPosition();
				Vec3 dir = dl->GetLightDirection().normalize();
				float len = std::max(3.0f, minWorld);
				Vec3 to = pos + dir * (len * 0.8f);
				dbg->drawLine(pos, to, kLightOverlayColor);
				Vec3 upRef = fabs(dir.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
				Vec3 right = (upRef.cross(dir)).normalize();
				Vec3 up = (dir.cross(right)).normalize();
				float ah = 0.2f * len;
				Vec3 tip = to;
				Vec3 a0 = tip - dir * ah + right * (0.5f * ah);
				Vec3 a1 = tip - dir * ah - right * (0.5f * ah);
				Vec3 a2 = tip - dir * ah + up * (0.5f * ah);
				Vec3 a3 = tip - dir * ah - up * (0.5f * ah);
				dbg->drawLine(tip, a0, kLightOverlayColor);
				dbg->drawLine(tip, a1, kLightOverlayColor);
				dbg->drawLine(tip, a2, kLightOverlayColor);
				dbg->drawLine(tip, a3, kLightOverlayColor);
				drawCircle(dbg, pos, dir, 0.4f * minWorld, kLightOverlayColor, 28);
			} else if (PointLight* pl = dynamic_cast<PointLight*>(c)) {
				dbg->drawSphere(go->GetWorldPosition(), pl->GetLightRadius(), kLightOverlayColor);
			} else if (SpotLight* sl = dynamic_cast<SpotLight*>(c)) {
				Vec3 pos = go->GetWorldPosition();
				Vec3 dir = sl->GetLightDirection();
				float R = sl->GetLightRadius();
				float inner = (float)DEGTORAD(sl->GetLightInnerCone()) * 0.5f;
				float outer = (float)DEGTORAD(sl->GetLightOutterCone()) * 0.5f;
				Vec3 fwd = dir.normalize();
				Vec3 upRef = fabs(fwd.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
				Vec3 right = (upRef.cross(fwd)).normalize();
				Vec3 up = (fwd.cross(right)).normalize();
				Vec3 tip = pos + fwd * R;
				float rOuter = R * tanf(outer);
				float rInner = R * tanf(inner);
				drawCircle(dbg, tip, fwd, rOuter, kLightOverlayColor, 36);
				drawCircle(dbg, tip, fwd, rInner, kLightOverlayColor, 36);
				for (int i = 0; i < 6; i++) {
					float a = (2.0f * 3.1415926f * i) / 6;
					Vec3 dirR = (right * cosf(a) + up * sinf(a));
					dbg->drawLine(pos, tip + dirR * rOuter, kLightOverlayColor);
				}
			}
		}
	}
}
