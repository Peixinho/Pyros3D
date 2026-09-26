//============================================================================
// Name        : MaterialGraphTypes.h
// Description : Shared node-graph data types for the Material Editor.
//               Document state (owned by MaterialEditorDocument), not editor
//               state - kept in their own header so both MaterialEditorDocument
//               and MaterialCodegen can include them without pulling in the
//               whole MaterialEditor drawing API.
//============================================================================

#ifndef MATERIALGRAPHTYPES_H
#define MATERIALGRAPHTYPES_H

#include <imgui.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Math/Vec4.h>
#include <string>
#include <vector>
#include <cstdint>

namespace p3d { class Texture; }

enum class MaterialEditKind { Generic, Custom };
enum class MaterialEditMode { Inspector, Text, NodeGraph };

struct MaterialNode;
struct MaterialConnection;

struct MaterialNode {
	uint32_t id = 0;
	std::string name;
	ImVec2 pos = ImVec2(100.f, 100.f);

	// Appended to, never reordered: the enum value itself is not saved
	// (.mat files store TypeToString's name), but keeping old values
	// stable keeps diffs of this list honest.
	enum Type {
		// Constants
		Color, Float, Texture, Int, Bool, Vec2Type, Vec3Type, Vec4Type,
		// Math operations
		Add, Subtract, Multiply, Divide, Power, Modulo, Negate, Abs, Sqrt, Sin, Cos, Tan,
		Min, Max, Clamp, Lerp, DotProduct, CrossProduct, Length, Normalize, Distance,
		// Comparison/Logic
		Equal, NotEqual, GreaterThan, LessThan, And, Or, Not, Step, SmoothStep,
		// Vector ops
		SplitVec2, SplitVec3, SplitVec4, CombineVec2, CombineVec3, CombineVec4,
		// Material output - single sink; every input pin below feeds one PBR channel
		Output,
		// Special. ObjectPosition is the FRAGMENT's world position (it always
		// was - the name predates the distinction); shown and saved as
		// "WorldPosition", with the old name still accepted on load.
		ObjectPosition, CameraPosition, UVCoordinate, NormalVector, TimeValue,
		// Added with the node-graph overhaul
		ObjectOrigin, ViewDirection, Fresnel, OneMinus, Saturate, Fract, Floor, Remap,
		Noise, NormalMap, FloatParameter, ColorParameter, CustomExpression
	} type = Float;

	// Output node input pins, in order. Indices are saved in .mat files -
	// append only.
	enum OutputPin {
		OutAlbedo = 0, OutNormal, OutMetallic, OutRoughness, OutEmissive, OutOcclusion,
		OutOpacity, OutAlphaClip, OutReflection, OutVertexOffset, OutPinCount
	};

	std::string userData;      // node-specific constant values (comma-separated floats); CustomExpression: the GLSL expression
	std::string texturePath;   // for Texture nodes: path to loaded texture, relative to assets/textures
	p3d::Texture* previewTex = nullptr;  // cached preview texture for display

	static int GetInputPinCount(Type t) {
		switch (t) {
			case Add: case Subtract: case Multiply: case Divide: case Power: case Modulo:
			case Min: case Max: case Equal: case NotEqual: case GreaterThan: case LessThan:
			case And: case Or: return 2;
			case Clamp: case Lerp: case SmoothStep: return 3;
			case DotProduct: case CrossProduct: case Distance: return 2;
			case CombineVec2: return 2; case CombineVec3: return 3; case CombineVec4: return 4;
			case Step: return 2;
			case Negate: case Abs: case Sqrt: case Sin: case Cos: case Tan:
			case Length: case Normalize: case Not:
			case SplitVec2: case SplitVec3: case SplitVec4: return 1;
			case Texture: return 1; // UV
			case OneMinus: case Saturate: case Fract: case Floor: return 1;
			case Fresnel: return 2;    // Power, Normal
			case Remap: return 5;      // Value, In Min, In Max, Out Min, Out Max
			case Noise: return 2;      // Position, Scale
			case NormalMap: return 2;  // Color, Strength
			case CustomExpression: return 4; // a, b, c, d
			case Output: return OutPinCount;
			default: return 0;
		}
	}

	static int GetOutputPinCount(Type t) {
		switch (t) {
			case Color: case ColorParameter: return 5; // R, G, B, A, RGBA
			case Texture: return 5;                     // RGBA, R, G, B, A (RGBA first: older graphs connect pin 0)
			case SplitVec2: return 2; case SplitVec3: return 3; case SplitVec4: return 4;
			case Output: return 0; // Output node only has inputs, no outputs
			default: return 1;
		}
	}

	static const char* GetInputPinLabel(Type t, int index) {
		switch (t) {
			case Output: {
				static const char* k[] = { "Albedo", "Normal", "Metallic", "Roughness", "Emissive", "Occlusion",
					"Opacity", "Alpha Clip", "Reflection", "Vertex Offset" };
				return (index >= 0 && index < OutPinCount) ? k[index] : nullptr;
			}
			case Texture: return index == 0 ? "UV" : nullptr;
			case Clamp: { static const char* k[] = { "X", "Min", "Max" }; return index < 3 ? k[index] : nullptr; }
			case Lerp: { static const char* k[] = { "A", "B", "T" }; return index < 3 ? k[index] : nullptr; }
			case SmoothStep: { static const char* k[] = { "Edge0", "Edge1", "X" }; return index < 3 ? k[index] : nullptr; }
			case Step: { static const char* k[] = { "Edge", "X" }; return index < 2 ? k[index] : nullptr; }
			case Power: { static const char* k[] = { "Base", "Exp" }; return index < 2 ? k[index] : nullptr; }
			case CombineVec2: case CombineVec3: case CombineVec4: { static const char* k[] = { "X", "Y", "Z", "W" }; return index < 4 ? k[index] : nullptr; }
			case Fresnel: { static const char* k[] = { "Power", "Normal" }; return index < 2 ? k[index] : nullptr; }
			case Remap: { static const char* k[] = { "Value", "In Min", "In Max", "Out Min", "Out Max" }; return index < 5 ? k[index] : nullptr; }
			case Noise: { static const char* k[] = { "Position", "Scale" }; return index < 2 ? k[index] : nullptr; }
			case NormalMap: { static const char* k[] = { "Color", "Strength" }; return index < 2 ? k[index] : nullptr; }
			case CustomExpression: { static const char* k[] = { "a", "b", "c", "d" }; return index < 4 ? k[index] : nullptr; }
			case Add: case Subtract: case Multiply: case Divide: case Modulo: case Min: case Max:
			case DotProduct: case CrossProduct: case Distance: case Equal: case NotEqual:
			case GreaterThan: case LessThan: case And: case Or:
				{ static const char* k[] = { "A", "B" }; return index < 2 ? k[index] : nullptr; }
			default: return nullptr;
		}
	}

	static const char* GetOutputPinLabel(Type t, int index) {
		if (t == Color || t == ColorParameter) {
			switch (index) {
				case 0: return "R"; case 1: return "G"; case 2: return "B";
				case 3: return "A"; case 4: return "RGBA";
			}
		} else if (t == Texture) {
			switch (index) {
				case 0: return "RGBA"; case 1: return "R"; case 2: return "G";
				case 3: return "B"; case 4: return "A";
			}
		} else if (t == SplitVec2) {
			switch (index) { case 0: return "X"; case 1: return "Y"; }
		} else if (t == SplitVec3) {
			switch (index) { case 0: return "X"; case 1: return "Y"; case 2: return "Z"; }
		} else if (t == SplitVec4) {
			switch (index) { case 0: return "X"; case 1: return "Y"; case 2: return "Z"; case 3: return "W"; }
		}
		return nullptr;
	}

	// How many components a pin carries, for the canvas's pin colours:
	// 1-4, or 0 for "whatever comes in" (the generic math ops). Every value
	// is a vec4 in the generated GLSL either way - this is a reading aid,
	// not a type system, and nothing refuses a connection over it.
	static int GetOutputPinWidth(Type t, int index) {
		switch (t) {
			case Color: case ColorParameter: return index == 4 ? 4 : 1;
			case Texture: return index == 0 ? 4 : 1;
			case SplitVec2: case SplitVec3: case SplitVec4: return 1;
			case Float: case Int: case Bool: case Length: case Distance: case DotProduct:
			case And: case Or: case Not: case Fresnel: case Noise: case FloatParameter: case TimeValue: return 1;
			case Vec2Type: case UVCoordinate: case CombineVec2: return 2;
			case Vec3Type: case CombineVec3: case CrossProduct: case Normalize: case NormalVector:
			case ObjectPosition: case ObjectOrigin: case CameraPosition: case ViewDirection: case NormalMap: return 3;
			case Vec4Type: case CombineVec4: return 4;
			default: return 0;
		}
	}
	static int GetInputPinWidth(Type t, int index) {
		if (t == Output) {
			static const int k[] = { 3, 3, 1, 1, 3, 1, 1, 1, 1, 3 };
			return (index >= 0 && index < OutPinCount) ? k[index] : 0;
		}
		switch (t) {
			case Texture: return 2;
			case CombineVec2: case CombineVec3: case CombineVec4: return 1;
			case Fresnel: return index == 0 ? 1 : 3;
			case Noise: return index == 0 ? 3 : 1;
			case NormalMap: return index == 0 ? 4 : 1;
			case DotProduct: case CrossProduct: case Length: case Normalize: case Distance: return 3;
			case And: case Or: case Not: return 1;
			default: return 0;
		}
	}

	// Nodes whose value depends on the fragment/vertex being shaded (or on
	// time), so the canvas's CPU swatch cannot show a real value for them
	// or for anything downstream - see IsPreviewDynamic().
	static bool IsVaryingSource(Type t) {
		switch (t) {
			case Texture: case UVCoordinate: case NormalVector: case ObjectPosition: case CameraPosition:
			case TimeValue: case ObjectOrigin: case ViewDirection: case Fresnel: case Noise: case NormalMap:
			case CustomExpression:
				return true;
			default: return false;
		}
	}
	// True when this node's value varies per pixel/over time (itself or any
	// input, transitively) - its swatch is then a placeholder, not a value.
	bool IsPreviewDynamic(const std::vector<MaterialNode>& nodes, const std::vector<MaterialConnection>& connections) const;

	static bool IsParameter(Type t) { return t == FloatParameter || t == ColorParameter; }

	// Compute preview value based on connected inputs and node type - CPU-side
	// approximation used for the node-graph canvas swatches. Kept in sync with
	// MaterialCodegen's GLSL semantics (see MaterialCodegen.cpp's header comment).
	// Unqualified Vec4 (not p3d::Vec4): it actually lives in p3d::Math, and
	// Math.h's global `using namespace p3d::Math;` is what makes the bare
	// name resolve - same convention every other engine header/source
	// using Vec4/Vec3/etc. already relies on.
	Vec4 ComputePreviewValue(const MaterialNode& self, const std::vector<MaterialNode>& nodes,
	                         const std::vector<MaterialConnection>& connections) const;

	const char* GetOpName() const {
		switch (type) {
			case Add: return "+"; case Subtract: return "-"; case Multiply: return "*";
			case Divide: return "/"; case Power: return "^"; case Modulo: return "%";
			case Negate: return "neg"; case Abs: return "|x|"; case Sqrt: return "\xE2\x88\x9A";
			case Sin: return "sin"; case Cos: return "cos"; case Tan: return "tan";
			case Min: return "min"; case Max: return "max"; case Clamp: return "clamp";
			case Lerp: return "lerp"; case DotProduct: return "dot"; case CrossProduct: return "cross";
			case Length: return "|v|"; case Normalize: return "norm"; case Distance: return "dist";
			case Equal: return "=="; case NotEqual: return "!="; case GreaterThan: return ">";
			case LessThan: return "<"; case And: return "&"; case Or: return "|"; case Not: return "!";
			case Step: return "step"; case SmoothStep: return "smoothstep";
			case OneMinus: return "1-x"; case Saturate: return "sat"; case Fract: return "fract";
			case Floor: return "floor"; case Remap: return "remap"; case Fresnel: return "fresnel";
			case Noise: return "noise";
			default: return "";
		}
	}

	// String round-trip for serialization + the Add-Node menu, kept in a
	// single table so both stay in sync automatically.
	static const char* TypeToString(Type t);
	static bool TypeFromString(const std::string& s, Type& outType);
	// Menu/title text - "World Position" rather than "ObjectPosition".
	static const char* TypeDisplayName(Type t);
	// A parameter node's name as a GLSL identifier suffix: anything that is
	// not [A-Za-z0-9_] becomes '_', and a leading digit gets a '_' before
	// it. Empty in, empty out.
	static std::string SanitizeParameterName(const std::string& name);
};

// Named texture uniform declared from Text mode (see MaterialEditor's
// "Textures" list on the Text tab) - the node-graph equivalent of a Texture
// node, but since Text mode has no nodes to hang a texturePath off of, the
// document keeps its own flat list of these instead.
struct MaterialTextureInput {
	uint32_t id = 0;
	std::string name = "uTexture";     // GLSL uniform sampler2D name, referenced directly in the user's snippet
	std::string texturePath;           // path to loaded texture, relative to assets/textures
	p3d::Texture* previewTex = nullptr; // cached preview texture for display
};

struct PinPosition {
	uint32_t nodeId = 0;
	int pinIndex = 0;
	bool isOutput = false;
	ImVec2 screenPos;
};

struct MaterialConnection {
	uint32_t fromNode = 0;
	int fromPinIndex = 0; // which output pin
	uint32_t toNode = 0;
	int toPinIndex = 0;   // which input pin
};

#endif /* MATERIALGRAPHTYPES_H */
