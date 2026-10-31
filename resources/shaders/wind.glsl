// A plant in the wind (GenericShaderMaterial::SetWind): what stands above the mesh's own origin swayed - a blade card
// modelled round its centre bends from the middle up and keeps its foot on the ground. wind: x how far (per metre of
// height), y how fast, z how far apart in the world two plants still sway alike, w the normal's lean to the sky.
// `origin` is where the plant stands (its - instanced - model's translation), so a field does not sway in lockstep;
// two waves at an irrational ratio, so it never visibly repeats one gust. The same in every pass that draws it - the
// surface, the depth prepass (with last frame's time: its motion for the temporal resolve) and its shadow.
vec3 WindSway(vec3 p, vec3 origin, vec4 wind, float seconds)
{
	if (wind.x <= 0.0) return p;
	float phase = seconds * wind.y + (origin.x + origin.z) * wind.z;
	float sway = sin(phase) * 0.75 + sin(phase * 2.37 + 1.7) * 0.25;
	return p + vec3(sway, 0.0, sway * 0.35) * (max(p.y, 0.0) * wind.x);
}
