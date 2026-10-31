// Pyros3D the renderer - what every shader of the new renderer shares.
//
// The binding model (the device layer):
//   set 0, binding 0   the draw's own data (DrawData): up to 256 bytes, a slot of a ring the device writes
//   set 1              the material group: the frame's data at binding 0 and the frame's lights at binding 1
//                      (dynamic offsets - the frame's slot), then the material's textures and samplers,
//                      separate (as WebGPU wants them)

#define PI 3.14159265359

layout(set = 1, binding = 0, std140) uniform FrameData
{
	mat4 viewProjection;
	vec4 cameraPosition;     // xyz world
	vec4 sunDirection;       // xyz: toward the sun, w: 1 when there is a sun
	vec4 sunRadiance;        // rgb linear
	vec4 ambient;            // rgb linear
	vec4 frame;              // x: seconds, y: exposure, z: 1 when the sky lights the scene (its SH), w: debug view (1 cascades, 2 AO off, 3 AO only, 4 sun shadows off, 5 the clouds' shadow)
	vec4 sky;                // x: the camera's altitude (km), y: the sun's angular radius (rad), z: seconds (uTime), w: haze (0 none, 1 the real air's)
	vec4 projection;         // x, y: the projection's scale on x and y (P00, P11), z: near plane; w: 0 perspective (reversed Z,
	                         // infinite), else orthographic and w its far plane (reversed Z, depth 1 at near, 0 at far)
	vec4 viewport;           // xy its size in pixels, zw one over
	mat4 view;               // world -> view
	vec4 globals[16];        // the application's (ShaderGlobals.h)
	vec4 ambientSky;         // where there is no sky: w the ambient's mode (0 flat, 1 a gradient, 2 SH) - rgb the gradient's top
	vec4 ambientEquator;     // its horizon
	vec4 ambientGround;      // its bottom
	vec4 ambientSH[9];       // order-2 spherical harmonics of the light all round (linear radiance)
	vec4 weather;            // x: the rain, 0..1 (RenderGlobals::Rain): what faces up wet and puddled in proportion; y: 1 the sun's shadow ray-traced (surface.frag's uAO, green); z: 1 a transparent background (View); w: the screen's HDR headroom (1: SDR)
	mat4 previousViewProjection;     // last frame's, without its jitter: where a point was in last frame's picture (reflections)
} uFrame;

// Where a world point was in last frame's picture (0..1, y down), -1 where it was not in it
vec2 PreviousUV(vec3 world)
{
	vec4 c = uFrame.previousViewProjection * vec4(world, 1.0);
	if (c.w <= 0.0) return vec2(-1.0);
	vec2 uv = vec2(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5);
	return any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) ? vec2(-1.0) : uv;
}

// The projection, either kind (projection.w): what a depth value is as a distance in front of the eye, the view-space
// point under a pixel at a distance (ndc -1..1, y up), a view-space point to clip space, and the way to the eye
bool Orthographic() { return uFrame.projection.w > 0.0; }
// (an sRGB-encoded value as the linear one an sRGB target's store encodes back to it)
vec3 SrgbDecode(vec3 c) { vec3 x = clamp(c, 0.0, 1.0); return mix(x / 12.92, pow((x + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), x)); }
float DepthDistance(float d) { return Orthographic() ? mix(uFrame.projection.w, uFrame.projection.z, d) : uFrame.projection.z / max(d, 1e-7); }
vec3 ViewPointAt(vec2 ndc, float dist)
{
	vec2 xy = Orthographic() ? ndc / uFrame.projection.xy : ndc * dist / uFrame.projection.xy;
	return vec3(xy, -dist);
}
vec4 ProjectView(vec3 v)
{
	if (Orthographic()) return vec4(v.xy * uFrame.projection.xy, (uFrame.projection.w + v.z) / (uFrame.projection.w - uFrame.projection.z), 1.0);
	return vec4(v.xy * uFrame.projection.xy, uFrame.projection.z, -v.z);
}
vec3 ToEye(vec3 worldPosition)
{
	// (orthographic: every ray parallel - the camera's own backward axis, the view's third row)
	if (Orthographic()) return normalize(vec3(uFrame.view[0][2], uFrame.view[1][2], uFrame.view[2][2]));
	return normalize(uFrame.cameraPosition.xyz - worldPosition);
}
