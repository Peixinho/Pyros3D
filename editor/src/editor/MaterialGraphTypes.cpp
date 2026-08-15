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
		{ MaterialNode::ObjectPosition, "ObjectPosition" }, { MaterialNode::CameraPosition, "CameraPosition" },
		{ MaterialNode::UVCoordinate, "UVCoordinate" }, { MaterialNode::NormalVector, "NormalVector" },
		{ MaterialNode::TimeValue, "TimeValue" },
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
	return false;
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
			if (src->type == Color) {
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
		case Color: {
			float c[4] = {1, 1, 1, 1};
			if (!self.userData.empty())
				sscanf(self.userData.c_str(), "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]);
			return Vec4(c[0], c[1], c[2], c[3]);
		}
		case Float: {
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
			return Vec4(a.x / (b.x + 0.001f), a.y / (b.y + 0.001f),
			           a.z / (b.z + 0.001f), a.w / (b.w + 0.001f));
		}
		case Power: {
			Vec4 a = In(0), b = In(1);
			return Vec4(powf(a.x, b.x), powf(a.y, b.y), powf(a.z, b.z), powf(a.w, b.w));
		}
		case Modulo: {
			Vec4 a = In(0), b = In(1);
			auto m = [](float x, float y) { return y != 0.f ? fmodf(x, y) : 0.f; };
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
			Vec4 a = In(0), b = In(1), t = In(2);
			return a + (b - a) * t.x;
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
		case UVCoordinate: return Vec4(0.5f, 0.5f, 0.f, 1.f); // UV not known outside the shader
		case NormalVector: return Vec4(0.5f, 0.5f, 1.f, 0.f); // approximate "flat forward" preview
		case ObjectPosition: return Vec4(0.f, 0.f, 0.f, 1.f);
		case CameraPosition: return Vec4(0.f, 0.f, 0.f, 1.f);
		case TimeValue: return Vec4(0.f, 0.f, 0.f, 1.f);
		case Output: return Vec4(0.5f, 0.5f, 0.5f, 1.f);
		default: return Vec4(0.5f, 0.5f, 0.5f, 1.f);
	}
}
