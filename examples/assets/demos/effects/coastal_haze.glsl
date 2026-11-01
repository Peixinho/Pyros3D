//! effect CoastalHaze
//! input LastRTT
//! input Depth
//! param float uNear 0.1 Camera near
//! param float uFar 4000.0 Camera far
//! param vec4 uHazeColor 0.86,0.80,0.74,1.0 Haze colour
//! param float uHazeDensity 0.012 0.0 0.05 Haze per metre
//! param float uHazeStart 15.0 0.0 200.0 Haze starts at
//! param float uScanlines 0.06 0.0 0.5 Scan lines
//! param float uTime 0.0 Seconds (filled by the engine)

// A scene's own post effect, written as the old renderer's asset effects were: the picture (uTex0) and the depth
// (uTex1, 0 near .. 1 far between uNear and uFar), sampled with texture_2D at vTexcoord (y up), FragColor out. Here:
// the distance hazed toward a warm sea-mist colour, and a faint rolling scan line over everything - what a scene can
// add to the renderer's own look without the engine knowing about it.
void main() {
	vec3 c = texture_2D(uTex0, vTexcoord).rgb;
	float d = texture_2D(uTex1, vTexcoord).r;
	if (d < 1.0) {
		float z = (uNear * uFar) / (d * (uNear - uFar) + uFar);
		float f = 1.0 - exp(-max(z - uHazeStart, 0.0) * uHazeDensity);
		c = mix(c, uHazeColor.rgb, clamp(f, 0.0, 1.0) * uHazeColor.a);
	}
	float line = 0.5 + 0.5 * sin((vTexcoord.y * 360.0 - uTime * 2.0) * 3.14159);
	c *= 1.0 - uScanlines * line;
	FragColor = vec4(c, 1.0);
}
