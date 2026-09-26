//============================================================================
// Name        : MaterialGraphTypes.cpp
// Description : MaterialNode::Type <-> string table (shared by serialization
//               and the Add-Node menu) and the CPU-side preview evaluator
//               used by the node-graph canvas swatches.
//============================================================================

#include "MaterialGraphTypes.h"
#include <cstdio>
#include <cmath>

using namespace p3d;

namespace {
	struct TypeName { MaterialNode::Type type; const char* name; };
	// Single source of truth for every MaterialNode::Type <-> string mapping -
	// used by serialization (MaterialEditorDocument) and the grouped Add-Node
	// menu (MaterialEditor) so they can never drift apart.
	const TypeName kTypeNames[] = {
		{ MaterialNode::Color, "Color" }, { MaterialNode::Float, "Float" },
		{ MaterialNode::Texture, "Texture" }, { MaterialNode::Int, "Int" },
		{ MaterialNode::Bool, "Bool" }, { MaterialNode::Vec2Type, "Vec2" },
		{ MaterialNode::Vec3Type, "Vec3" }, { MaterialNode::Vec4Type, "Vec4" },
		{ MaterialNode::Add, "Add" }, { MaterialNode::Subtract, "Subtract" },
		{ MaterialNode::Multiply, "Multiply" }, { MaterialNode::Divide, "Divide" },
		{ MaterialNode::Power, "Power" }, { MaterialNode::Modulo, "Modulo" },
		{ MaterialNode::Negate, "Negate" }, { MaterialNode::Abs, "Abs" },
		{ MaterialNode::Sqrt, "Sqrt" }, { MaterialNode::Sin, "Sin" },
		{ MaterialNode::Cos, "Cos" }, { MaterialNode::Tan, "Tan" },
		{ MaterialNode::Min, "Min" }, { MaterialNode::Max, "Max" },
		{ MaterialNode::Clamp, "Clamp" }, { MaterialNode::Lerp, "Lerp" },
		{ MaterialNode::DotProduct, "DotProduct" }, { MaterialNode::CrossProduct, "CrossProduct" },
		{ MaterialNode::Length, "Length" }, { MaterialNode::Normalize, "Normalize" },
		{ MaterialNode::Distance, "Distance" },
		{ MaterialNode::Equal, "Equal" }, { MaterialNode::NotEqual, "NotEqual" },
		{ MaterialNode::GreaterThan, "GreaterThan" }, { MaterialNode::LessThan, "LessThan" },
		{ MaterialNode::And, "And" }, { MaterialNode::Or, "Or" }, { MaterialNode::Not, "Not" },
		{ MaterialNode::Step, "Step" }, { MaterialNode::SmoothStep, "SmoothStep" },
		{ MaterialNode::SplitVec2, "SplitVec2" }, { MaterialNode::SplitVec3, "SplitVec3" },
		{ MaterialNode::SplitVec4, "SplitVec4" }, { MaterialNode::CombineVec2, "CombineVec2" },
		{ MaterialNode::CombineVec3, "CombineVec3" }, { MaterialNode::CombineVec4, "CombineVec4" },
		{ MaterialNode::Output, "Output" },
		{ MaterialNode::ObjectPosition, "WorldPosition" }, { MaterialNode::CameraPosition, "CameraPosition" },
		{ MaterialNode::UVCoordinate, "UVCoordinate" }, { MaterialNode::NormalVector, "NormalVector" },
		{ MaterialNode::TimeValue, "TimeValue" },
		{ MaterialNode::ObjectOrigin, "ObjectOrigin" }, { MaterialNode::ViewDirection, "ViewDirection" },
		{ MaterialNode::Fresnel, "Fresnel" }, { MaterialNode::OneMinus, "OneMinus" },
		{ MaterialNode::Saturate, "Saturate" }, { MaterialNode::Fract, "Fract" },
		{ MaterialNode::Floor, "Floor" }, { MaterialNode::Remap, "Remap" },
		{ MaterialNode::Noise, "Noise" }, { MaterialNode::NormalMap, "NormalMap" },
		{ MaterialNode::FloatParameter, "FloatParameter" }, { MaterialNode::ColorParameter, "ColorParameter" },
		{ MaterialNode::CustomExpression, "CustomExpression" },
	};
	const int kTypeNameCount = sizeof(kTypeNames) / sizeof(kTypeNames[0]);
}

const char* MaterialNode::TypeToString(Type t) {
	for (int i = 0; i < kTypeNameCount; i++)
		if (kTypeNames[i].type == t) return kTypeNames[i].name;
	return "Float";
}

bool MaterialNode::TypeFromString(const std::string& s, Type& outType) {
	for (int i = 0; i < kTypeNameCount; i++) {
		if (s == kTypeNames[i].name) { outType = kTypeNames[i].type; return true; }
	}
	// The name every graph saved before WorldPosition was called that.
	if (s == "ObjectPosition") { outType = ObjectPosition; return true; }
	return false;
}

const char* MaterialNode::TypeDisplayName(Type t) {
	switch (t) {
		case ObjectPosition: return "World Position";
		case CameraPosition: return "Camera Position";
		case UVCoordinate: return "UV";
		case NormalVector: return "World Normal";
		case TimeValue: return "Time";
		case ObjectOrigin: return "Object Origin";
		case ViewDirection: return "View Direction";
		case OneMinus: return "One Minus";
		case NormalMap: return "Normal Map";
		case FloatParameter: return "Float Parameter";
		case ColorParameter: return "Color Parameter";
		case CustomExpression: return "Custom Expression";
		case DotProduct: return "Dot Product";
		case CrossProduct: return "Cross Product";
		case GreaterThan: return "Greater Than";
		case LessThan: return "Less Than";
		case NotEqual: return "Not Equal";
		case SmoothStep: return "Smooth Step";
		case SplitVec2: return "Split Vec2"; case SplitVec3: return "Split Vec3"; case SplitVec4: return "Split Vec4";
		case CombineVec2: return "Combine Vec2"; case CombineVec3: return "Combine Vec3"; case CombineVec4: return "Combine Vec4";
		default: return TypeToString(t);
	}
}

std::string MaterialNode::SanitizeParameterName(const std::string& name) {
	std::string out;
	for (char c : name) {
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
		out += ok ? c : '_';
	}
	if (!out.empty() && out[0] >= '0' && out[0] <= '9') out = "_" + out;
	return out;
}

bool MaterialNode::IsPreviewDynamic(const std::vector<MaterialNode>& nodes, const std::vector<MaterialConnection>& connections) const {
	// Depth-bounded rather than cycle-tracked: a cycle is a codegen error
	// anyway, and this only has to stop, not diagnose it.
	struct Walk {
		const std::vector<MaterialNode>& nodes;
		const std::vector<MaterialConnection>& connections;
		bool Dynamic(const MaterialNode& n, int depth) const {
			if (IsVaryingSource(n.type)) return true;
			if (depth > 64) return false;
			for (const auto& c : connections) {
				if (c.toNode != n.id) continue;
				for (const auto& src : nodes)
					if (src.id == c.fromNode && Dynamic(src, depth + 1)) return true;
			}
			return false;
		}
	};
	return Walk{ nodes, connections }.Dynamic(*this, 0);
}

Vec4 MaterialNode::ComputePreviewValue(const MaterialNode& self, const std::vector<MaterialNode>& nodes,
                                       const std::vector<MaterialConnection>& connections) const {
	auto FindNode = [&](uint32_t id) -> const MaterialNode* {
		for (const auto& n : nodes) if (n.id == id) return &n;
		return nullptr;
	};

	// Resolves the value feeding input pin `pinIndex` of `nodeId`, applying
	// the same per-output-pin swizzle MaterialCodegen uses (R/G/B/A/RGBA for
	// Color, X/Y/Z/W for Split*) so the CPU preview matches the generated GLSL.
	auto GetInputValue = [&](uint32_t nodeId, int pinIndex) -> Vec4 {
		for (const auto& conn : connections) {
			if (conn.toNode != nodeId || conn.toPinIndex != pinIndex) continue;
			const MaterialNode* src = FindNode(conn.fromNode);
			if (!src) return Vec4(0.f, 0.f, 0.f, 0.f);
			Vec4 v = src->ComputePreviewValue(*src, nodes, connections);
			if (src->type == Texture && conn.fromPinIndex > 0) {
				const float c = (conn.fromPinIndex == 1) ? v.x : (conn.fromPinIndex == 2) ? v.y : (conn.fromPinIndex == 3) ? v.z : v.w;
				return Vec4(c, c, c, c);
			}
			if (src->type == Color || src->type == ColorParameter) {
				switch (conn.fromPinIndex) {
					case 0: return Vec4(v.x, v.x, v.x, v.x);
					case 1: return Vec4(v.y, v.y, v.y, v.y);
					case 2: return Vec4(v.z, v.z, v.z, v.z);
					case 3: return Vec4(v.w, v.w, v.w, v.w);
					default: return v;
				}
			}
			if (src->type == SplitVec2 || src->type == SplitVec3 || src->type == SplitVec4) {
				float c = (conn.fromPinIndex == 0) ? v.x : (conn.fromPinIndex == 1) ? v.y
					: (conn.fromPinIndex == 2) ? v.z : v.w;
				return Vec4(c, c, c, c);
			}
			return v;
		}
		return Vec4(0.f, 0.f, 0.f, 0.f); // default disconnected value
	};
	auto In = [&](int pinIndex) { return GetInputValue(self.id, pinIndex); };

	switch (type) {
		case Color: case ColorParameter: {
			float c[4] = {1, 1, 1, 1};
			if (!self.userData.empty())
				sscanf(self.userData.c_str(), "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]);
			return Vec4(c[0], c[1], c[2], c[3]);
		}
		case Float: case FloatParameter: {
			float val = 0.5f;
			if (!self.userData.empty()) sscanf(self.userData.c_str(), "%f", &val);
			return Vec4(val, val, val, 1.f);
		}
		case Int: {
			int val = 0;
			if (!self.userData.empty()) sscanf(self.userData.c_str(), "%d", &val);
			return Vec4((float)val, (float)val, (float)val, 1.f);
		}
		case Bool: {
			float v = (self.userData == "1") ? 1.f : 0.f;
			return Vec4(v, v, v, 1.f);
		}
		case Vec2Type: {
			float v[2] = {0, 0};
			if (!self.userData.empty()) sscanf(self.userData.c_str(), "%f,%f", &v[0], &v[1]);
			return Vec4(v[0], v[1], 0.f, 0.f);
		}
		case Vec3Type: {
			float v[3] = {0, 0, 0};
			if (!self.userData.empty()) sscanf(self.userData.c_str(), "%f,%f,%f", &v[0], &v[1], &v[2]);
			return Vec4(v[0], v[1], v[2], 0.f);
		}
		case Vec4Type: {
			float v[4] = {0, 0, 0, 1};
			if (!self.userData.empty()) sscanf(self.userData.c_str(), "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
			return Vec4(v[0], v[1], v[2], v[3]);
		}
		case Texture: return Vec4(0.5f, 0.5f, 0.5f, 1.f); // sampled value only known on GPU
		case Add: return In(0) + In(1);
		case Subtract: return In(0) - In(1);
		case Multiply: {
			Vec4 a = In(0), b = In(1);
			return Vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
		}
		case Divide: {
			Vec4 a = In(0), b = In(1);
			// Same epsilon as the generated GLSL's `a / (b + 0.0001)`.
			return Vec4(a.x / (b.x + 0.0001f), a.y / (b.y + 0.0001f),
			           a.z / (b.z + 0.0001f), a.w / (b.w + 0.0001f));
		}
		case Power: {
			Vec4 a = In(0), b = In(1);
			// abs() like the GLSL's pow(abs(a), b): a negative base is NaN
			// in both otherwise, but only the shader guarded against it.
			return Vec4(powf(fabsf(a.x), b.x), powf(fabsf(a.y), b.y), powf(fabsf(a.z), b.z), powf(fabsf(a.w), b.w));
		}
		case Modulo: {
			Vec4 a = In(0), b = In(1);
			// GLSL mod() floors (x - y*floor(x/y)), unlike fmodf, and the
			// shader clamps y to >= 0.0001.
			auto m = [](float x, float y) { y = fmaxf(y, 0.0001f); return x - y * floorf(x / y); };
			return Vec4(m(a.x, b.x), m(a.y, b.y), m(a.z, b.z), m(a.w, b.w));
		}
		case Negate: { Vec4 a = In(0); return Vec4(-a.x, -a.y, -a.z, -a.w); }
		case Abs: { Vec4 a = In(0); return Vec4(fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w)); }
		case Sqrt: { Vec4 a = In(0); return Vec4(sqrtf(fabsf(a.x)), sqrtf(fabsf(a.y)), sqrtf(fabsf(a.z)), sqrtf(fabsf(a.w))); }
		case Sin: { Vec4 a = In(0); return Vec4(sinf(a.x), sinf(a.y), sinf(a.z), sinf(a.w)); }
		case Cos: { Vec4 a = In(0); return Vec4(cosf(a.x), cosf(a.y), cosf(a.z), cosf(a.w)); }
		case Tan: { Vec4 a = In(0); return Vec4(tanf(a.x), tanf(a.y), tanf(a.z), tanf(a.w)); }
		case Min: { Vec4 a = In(0), b = In(1); return Vec4(fminf(a.x,b.x), fminf(a.y,b.y), fminf(a.z,b.z), fminf(a.w,b.w)); }
		case Max: { Vec4 a = In(0), b = In(1); return Vec4(fmaxf(a.x,b.x), fmaxf(a.y,b.y), fmaxf(a.z,b.z), fmaxf(a.w,b.w)); }
		case Clamp: {
			Vec4 a = In(0), lo = In(1), hi = In(2);
			auto c = [](float x, float l, float h) { return fminf(fmaxf(x, l), h); };
			return Vec4(c(a.x,lo.x,hi.x), c(a.y,lo.y,hi.y), c(a.z,lo.z,hi.z), c(a.w,lo.w,hi.w));
		}
		case Lerp: {
			// Per channel, like the shader's mix(a, b, t).
			Vec4 a = In(0), b = In(1), t = In(2);
			return Vec4(a.x + (b.x - a.x) * t.x, a.y + (b.y - a.y) * t.y, a.z + (b.z - a.z) * t.z, a.w + (b.w - a.w) * t.w);
		}
		case DotProduct: {
			Vec4 a = In(0), b = In(1);
			float d = a.x*b.x + a.y*b.y + a.z*b.z;
			return Vec4(d, d, d, d);
		}
		case CrossProduct: {
			Vec4 a = In(0), b = In(1);
			return Vec4(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x, 0.f);
		}
		case Length: {
			Vec4 a = In(0);
			float l = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
			return Vec4(l, l, l, l);
		}
		case Normalize: {
			Vec4 a = In(0);
			float l = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
			if (l < 0.00001f) return Vec4(0.f, 0.f, 0.f, 0.f);
			return Vec4(a.x/l, a.y/l, a.z/l, 0.f);
		}
		case Distance: {
			Vec4 a = In(0), b = In(1);
			float dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
			float d = sqrtf(dx*dx + dy*dy + dz*dz);
			return Vec4(d, d, d, d);
		}
		case Equal: { Vec4 a=In(0), b=In(1); return Vec4(a.x==b.x, a.y==b.y, a.z==b.z, a.w==b.w); }
		case NotEqual: { Vec4 a=In(0), b=In(1); return Vec4(a.x!=b.x, a.y!=b.y, a.z!=b.z, a.w!=b.w); }
		case GreaterThan: { Vec4 a=In(0), b=In(1); return Vec4(a.x>b.x, a.y>b.y, a.z>b.z, a.w>b.w); }
		case LessThan: { Vec4 a=In(0), b=In(1); return Vec4(a.x<b.x, a.y<b.y, a.z<b.z, a.w<b.w); }
		case And: { float v = (In(0).x>0.5f && In(1).x>0.5f) ? 1.f : 0.f; return Vec4(v,v,v,v); }
		case Or: { float v = (In(0).x>0.5f || In(1).x>0.5f) ? 1.f : 0.f; return Vec4(v,v,v,v); }
		case Not: { float v = (In(0).x>0.5f) ? 0.f : 1.f; return Vec4(v,v,v,v); }
		case Step: { Vec4 e=In(0), x=In(1); auto s=[](float e,float x){return x<e?0.f:1.f;}; return Vec4(s(e.x,x.x),s(e.y,x.y),s(e.z,x.z),s(e.w,x.w)); }
		case SmoothStep: {
			Vec4 e0=In(0), e1=In(1), x=In(2);
			auto s = [](float e0, float e1, float x) {
				float t = (e1 - e0 > 0.00001f) ? fminf(fmaxf((x-e0)/(e1-e0), 0.f), 1.f) : 0.f;
				return t*t*(3.f - 2.f*t);
			};
			return Vec4(s(e0.x,e1.x,x.x), s(e0.y,e1.y,x.y), s(e0.z,e1.z,x.z), s(e0.w,e1.w,x.w));
		}
		case SplitVec2: case SplitVec3: case SplitVec4: return In(0); // pass-through; consumer swizzles
		case CombineVec2: { Vec4 a=In(0), b=In(1); return Vec4(a.x, b.x, 0.f, 0.f); }
		case CombineVec3: { Vec4 a=In(0), b=In(1), c=In(2); return Vec4(a.x, b.x, c.x, 0.f); }
		case CombineVec4: { Vec4 a=In(0), b=In(1), c=In(2), d=In(3); return Vec4(a.x, b.x, c.x, d.x); }
		case OneMinus: { Vec4 a = In(0); return Vec4(1.f - a.x, 1.f - a.y, 1.f - a.z, 1.f - a.w); }
		case Saturate: {
			Vec4 a = In(0);
			auto c = [](float x) { return fminf(fmaxf(x, 0.f), 1.f); };
			return Vec4(c(a.x), c(a.y), c(a.z), c(a.w));
		}
		case Fract: { Vec4 a = In(0); return Vec4(a.x - floorf(a.x), a.y - floorf(a.y), a.z - floorf(a.z), a.w - floorf(a.w)); }
		case Floor: { Vec4 a = In(0); return Vec4(floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w)); }
		case Remap: {
			Vec4 x = In(0), i0 = In(1), i1 = In(2), o0 = In(3), o1 = In(4);
			auto r = [](float x, float a, float b, float c, float d) {
				float span = b - a;
				if (fabsf(span) < 1e-5f) span = 1e-5f;
				return c + (x - a) / span * (d - c);
			};
			return Vec4(r(x.x, i0.x, i1.x, o0.x, o1.x), r(x.y, i0.y, i1.y, o0.y, o1.y),
			            r(x.z, i0.z, i1.z, o0.z, o1.z), r(x.w, i0.w, i1.w, o0.w, o1.w));
		}
		// Everything below varies per pixel - IsPreviewDynamic() marks the
		// swatch as a placeholder, so these are only neutral stand-ins.
		case Fresnel: return Vec4(0.2f, 0.2f, 0.2f, 1.f);
		case Noise: return Vec4(0.5f, 0.5f, 0.5f, 1.f);
		case NormalMap: return Vec4(0.f, 1.f, 0.f, 0.f);
		case ViewDirection: return Vec4(0.f, 0.f, 1.f, 0.f);
		case ObjectOrigin: return Vec4(0.f, 0.f, 0.f, 1.f);
		case CustomExpression: return Vec4(0.5f, 0.5f, 0.5f, 1.f);
		case UVCoordinate: return Vec4(0.5f, 0.5f, 0.f, 1.f); // UV not known outside the shader
		case NormalVector: return Vec4(0.5f, 0.5f, 1.f, 0.f); // approximate "flat forward" preview
		case ObjectPosition: return Vec4(0.f, 0.f, 0.f, 1.f);
		case CameraPosition: return Vec4(0.f, 0.f, 0.f, 1.f);
		case TimeValue: return Vec4(0.f, 0.f, 0.f, 1.f);
		case Output: return Vec4(0.5f, 0.5f, 0.5f, 1.f);
		default: return Vec4(0.5f, 0.5f, 0.5f, 1.f);
	}
}
