//============================================================================
// Name        : DirectionalLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Directional Light
//============================================================================

#ifndef DIRECTIONALLIGHT_H
#define	DIRECTIONALLIGHT_H

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

namespace p3d {

// Maximum Number of Splits
#define MAX_SPLITS 4
// Split Weight
#define SPLIT_WEIGHT 0.75
// the 0.2f factor is important because we might get artifacts at
#define CASCADE_FACTOR 0.2

	struct PYROS3D_API Cascade {

		// Sub Frustum Properties
		f32 Near;
		f32 Far;
		f32 Ratio;
		f32 Fov;
		f32 Width;
		f32 Height;
		Vec3 point[8];
		Matrix CropMatrix;
		Projection ortho;

		void UpdateFrustumPoints(const Vec3& position, const Vec3& direction);

		// this function builds a projection matrix for rendering from the shadow's POV.
		// First, it computes the appropriate z-range and sets an orthogonal projection.
		// Then, it translates and scales it, so that it exactly captures the bounding box
		// of the current frustum slice
		Matrix CreateCropMatrix(const Matrix &viewMatrix, std::vector<RenderingMesh*> rcomps);

		Matrix GetCropMatrix();
	};

	struct SunShadowCache;
	class PYROS3D_API DirectionalLight : public ILightComponent {

	public:

		DirectionalLight() : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = Vec4(1, 1, 1, 1); Direction = Vec3(0, 1, 0); ShadowCascades = 0; }
		DirectionalLight(const Vec4 &color) : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = color; Direction = Vec3(0, -1, 0); ShadowCascades = 0; }
		DirectionalLight(const Vec4 &color, const Vec3 &direction) : ILightComponent(LIGHT_TYPE::DIRECTIONAL) { Color = color; Direction = direction; ShadowCascades = 0; }
		virtual ~DirectionalLight() {}

		// A sun: its light is what the air leaves of it, by how high it stands - white
		// overhead, gold then red as it comes down, gone a little under the horizon. Turned
		// in an editor or by a script, the light it gives follows; nothing else has to.
		void SetSun(const bool on) { sun = on; }
		bool IsSun() const { return sun; }
		// Sun shafts: the light the air scatters toward the eye, broken by shadows (the deferred
		// renderer's sun pass marches the view ray through the shadow map). x: density at the
		// ground per metre (0: off), y: forward scattering (Henyey-Greenstein g, 0..0.95), z: how fast
		// it thins with height per metre, w: how far out it is marched in metres.
		// tint: the scattered light's colour (times the sun's), w: steps along the ray.
		void SetVolumetric(const Vec4 &params, const Vec4 &tint = Vec4(1, 1, 1, 20)) { volumetric = params; volumetricTint = tint; }
		const Vec4 &GetVolumetric() const { return volumetric; }
		const Vec4 &GetVolumetricTint() const { return volumetricTint; }
		// Ray-traced shadows (the deferred renderer, where the device can trace rays -
		// IRenderDevice::SupportsRayQuery()): what is solid and still casts by rays traced
		// toward the sun from every pixel, exact at contact and softening with distance
		// (`softness`: the sun's angular radius, radians); out to `distance` metres from the
		// camera. People (skinned) and cut-out or see-through things keep the shadow map.
		void SetRayTracedShadows(const bool on, const f32 distance = 150.f, const f32 softness = 0.0093f) { rayShadows = on; rayShadowDistance = distance; raySoftness = softness; }
		// The shadow cache (IRenderer's sun pass): what stands still is drawn into a cascade's map once
		// and kept - scrolled as the view moves, its parts redrawn where something arrives, leaves or
		// moves, the whole when the sun has turned more than a texel can hide - and only what moves is
		// drawn every frame, over it. On unless turned off (or PYROS_SUN_CACHE=0).
		void SetShadowCache(const bool on) { shadowCacheOn = on; }
		bool GetShadowCache() const { return shadowCacheOn; }
		// What sways in the wind (a material with wind: trees, bushes) sways in its shadow too, and a kept
		// shadow cannot: from which cascade on such casters are kept all the same, standing still
		// (0 every cascade; -1, the default, none - drawn every frame, swaying). A game's choice.
		void SetShadowCacheWindFrom(const int32 cascade) { shadowCacheWindFrom = cascade; }
		int32 GetShadowCacheWindFrom() const { return shadowCacheWindFrom; }
		// How far the light may turn (radians) before a kept map is drawn again: until then its cascade
		// is drawn the way the light was, movers too, so the two agree. A cascade a frame is drawn again
		// past it, so a sun that keeps moving (a day going by) costs one cascade's still things a frame
		// at most. 0.25 degrees by default - a 10 m wall's shadow 4 cm off.
		void SetShadowCacheTurn(const f32 radians) { shadowCacheTurn = radians; }
		f32 GetShadowCacheTurn() const { return shadowCacheTurn; }
		// (the renderer's: the kept maps and what they hold)
		std::shared_ptr<SunShadowCache> shadowCache;
		bool GetRayTracedShadows() const { return rayShadows; }
		// Contact shadows (the deferred renderer): a short march through the depth buffer toward the
		// light from every lit pixel, for what is too small or too close for the shadow map - a tyre
		// on the road, a foot on the kerb, a crease in a facade. `length` metres of march (0 off).
		void SetContactShadows(const f32 length) { contactShadow = length; }
		f32 GetContactShadows() const { return contactShadow; }
		// Ray-traced reflections (the deferred renderer, where the device traces rays): a
		// reflective surface traces its reflection against the same ray scene, out to
		// `distance` metres, in place of the screen-space march - what is off the screen or
		// behind something reflects too. Off: screen-space reflections, as before.
		void SetRayTracedReflections(const bool on, const f32 distance = 80.f) { rayReflections = on; rayReflectionDistance = distance; }
		bool GetRayTracedReflections() const { return rayReflections; }
		f32 GetRayTracedReflectionDistance() const { return rayReflectionDistance; }
		f32 GetRayTracedShadowDistance() const { return rayShadowDistance; }
		f32 GetRayTracedShadowSoftness() const { return raySoftness; }
		// What it lights with now - GetLightRadiance(), and for a sun what the air leaves of
		// it at the height it stands (SunlightAt) - and the way to it in the world. Every
		// renderer lights a directional light by this, so a sun sets alike in all of them.
		Vec4 GetLightRadianceNow(Vec3* toLightOut = NULL);
		// (what the air leaves, for a sun `height` up: the y of the way to it, -1..1)
		static Vec3 SunlightAt(const f32 height)
		{
			const f32 up = height < 0.f ? 0.f : (height > 1.f ? 1.f : height);
			const f32 low = 1.f - (up < 0.42f ? up / 0.42f : 1.f);
			f32 lit = (height + 0.06f) / 0.16f; lit = lit < 0.f ? 0.f : (lit > 1.f ? 1.f : lit);
			const f32 strength = lit * (0.42f + 0.58f * (up * 2.6f > 1.f ? 1.f : up * 2.6f));
			return Vec3(1.f, 1.f - 0.42f * low, 1.f - 0.74f * low) * strength;
		}

		// The hour of the day, for a sun: where it stands at `hour` (0..24: up at six in the
		// east, highest at noon, down at eighteen) on a path that rises `azimuth` degrees
		// round from +x and leans `tilt` degrees from overhead. Sets the light's direction;
		// the light it gives follows as for any turn of a sun (SunlightAt).
		void SetTimeOfDay(const f32 hour) { Direction = SunDirectionAt(hour, sunAzimuth, sunTilt); }
		// (the hour its direction now stands for, on its own path)
		f32 GetTimeOfDay() const { return SunHourOf(Direction, sunAzimuth, sunTilt); }
		void SetSunPath(const f32 azimuthDegrees, const f32 tiltDegrees) { sunAzimuth = azimuthDegrees; sunTilt = tiltDegrees; }
		f32 GetSunAzimuth() const { return sunAzimuth; }
		f32 GetSunTilt() const { return sunTilt; }
		static void SunPathAxes(const f32 azimuth, const f32 tilt, Vec3 &east, Vec3 &up)
		{
			const f32 az = azimuth * 0.01745329f, t = tilt * 0.01745329f;
			east = Vec3(cosf(az), 0.f, sinf(az));
			up = Vec3(-sinf(az) * sinf(t), cosf(t), cosf(az) * sinf(t));
		}
		// (the light's Direction - from the sun, down - at that hour)
		static Vec3 SunDirectionAt(const f32 hour, const f32 azimuth, const f32 tilt)
		{
			Vec3 east, up; SunPathAxes(azimuth, tilt, east, up);
			const f32 a = (hour - 6.f) / 12.f * 3.14159265f;
			return (east * cosf(a) + up * sinf(a)) * -1.f;
		}
		static f32 SunHourOf(const Vec3 &direction, const f32 azimuth, const f32 tilt)
		{
			Vec3 east, up; SunPathAxes(azimuth, tilt, east, up);
			const Vec3 to = direction * -1.f;
			const f32 x = to.x * east.x + to.y * east.y + to.z * east.z, y = to.x * up.x + to.y * up.y + to.z * up.z;
			f32 hour = atan2f(y, x) / 3.14159265f * 12.f + 6.f;
			if (hour < 0.f) hour += 24.f;
			return hour;
		}

		virtual void Start() {};
		virtual void Update(const f64 time = 0) {};
		virtual void Destroy() {};

		virtual uint32 GetComponentType() const { return ComponentType::DirectionalLight; }

		void EnableCastShadows(const uint32 Width, const uint32 Height, const Projection &projection, const f32 Near, const f32 Far, const uint32 Cascades = 1);
		// How many cascades the shadow is in and how far from the eye it
		// reaches, changed on a light that is already casting: its map's size,
		// its biases and its softness stay as they are. What a game's "shadow
		// quality" setting moves. Nothing on a light that casts none.
		void SetShadowCascades(const uint32 Cascades, const f32 Far);

		// Legacy crop-box fit, kept for the Lua binding only. The renderer
		// uses FitCascade() - see there for what was wrong with this one.
		Matrix GetLightProjection(const Matrix &ShadowViewMatrix, const uint32 Cascade, const std::vector<RenderingMesh*> RCompList);
		void UpdateCascadeFrustumPoints(const uint32 Cascade, const Vec3 &CameraPosition, const Vec3 &CameraDirection);

		// The light's shadow view: a pure rotation looking down the light
		// direction. The up vector switches away from -Z when the light
		// itself runs along Z, where LookAt would otherwise degenerate.
		static Matrix ShadowViewMatrix(const Vec3 &worldDirection);

		// Fits cascade `Cascade` to the camera that is actually rendering
		// this frame and returns its orthographic projection for the given
		// light view. A bounding sphere of the frustum slice, not a box:
		// the sphere's size does not change as the camera turns, so the
		// shadow map's texel footprint stays constant. Its centre is then
		// snapped to whole texels in light space, so moving the camera
		// slides the map by whole texels rather than re-rasterising every
		// edge at a sub-texel offset. Those two together are what stop the
		// cascades shimmering - the old crop box was refitted to the
		// frustum's light-space AABB every frame and failed both.
		//
		// Depth covers the sphere and is extended toward the light over
		// every shadow-casting mesh whose bounds overlap it, so a caster
		// outside the view still throws its shadow in.
		Matrix FitCascade(const uint32 Cascade, const Matrix &CameraWorld, const Projection &CameraProjection, const Matrix &LightView, const std::vector<RenderingMesh*> &Casters,
			const Vec4* Spheres = NULL, const uint8* Flags = NULL, const uint8 CastMask = 0);

		// Linear view-space far distance of each cascade (unused ones 0).
		// The shaders select a cascade by comparing the fragment's view
		// depth against these.
		Vec4 GetCascadeSplits();

		// Fraction of each cascade, at its far end, over which it is
		// cross-faded into the next - and the last one faded out - so there
		// is no visible seam where the resolution steps. The next cascade
		// is fitted to start that much earlier to cover the band.
		static constexpr f32 CascadeBlendFraction = 0.1f;

		uint32 GetNumberCascades() 
		{
			return ShadowCascades; 
		}

		Cascade GetCascade(const uint32 Cascade)
		{
			return Cascades[Cascade];
		}

		const Vec3 &GetLightDirection() const 
		{ 
			return Direction; 
		}
		void SetLightDirection(const Vec3 &direction) 
		{
			Direction = direction; 
		}

	private:
		bool shadowCacheOn = true;
		int32 shadowCacheWindFrom = -1;
		f32 shadowCacheTurn = 0.0043633f;
		bool sun = false;
		f32 sunAzimuth = 0.f, sunTilt = 30.f;

		// Light Direction
		Vec3 Direction;

		// Shadow Cast
		uint32 ShadowCascades;
		Vec4 volumetric = Vec4(0.f, 0.6f, 0.08f, 120.f);
		Vec4 volumetricTint = Vec4(1.f, 1.f, 1.f, 20.f);
		bool rayShadows = false;
		f32 contactShadow = 0.f;
		bool rayReflections = false;
		f32 rayReflectionDistance = 80.f;
		f32 rayShadowDistance = 150.f, raySoftness = 0.0093f;
		Matrix ShadowProjection;
		Cascade Cascades[4];

	};

}

#endif	/* DIRECTIONALLIGHT_H */