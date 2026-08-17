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
		// Special
		ObjectPosition, CameraPosition, UVCoordinate, NormalVector, TimeValue
	} type = Float;

	std::string userData;      // node-specific constant values (comma-separated floats)
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
			case Output: return 6; // Albedo, Normal, Metallic, Roughness, Emissive, Occlusion
			default: return 0;
		}
	}

	static int GetOutputPinCount(Type t) {
		switch (t) {
			case Color: return 5; // R, G, B, A, RGBA
			case SplitVec2: return 2; case SplitVec3: return 3; case SplitVec4: return 4;
			case Output: return 0; // Output node only has inputs, no outputs
			default: return 1;
		}
	}

	static const char* GetInputPinLabel(Type t, int index) {
		if (t == Output) {
			switch (index) {
				case 0: return "Albedo"; case 1: return "Normal"; case 2: return "Metallic";
				case 3: return "Roughness"; case 4: return "Emissive"; case 5: return "Occlusion";
			}
		}
		return nullptr;
	}

	static const char* GetOutputPinLabel(Type t, int index) {
		if (t == Color) {
			switch (index) {
				case 0: return "R"; case 1: return "G"; case 2: return "B";
				case 3: return "A"; case 4: return "RGBA";
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
			default: return "";
		}
	}

	// String round-trip for serialization + the Add-Node menu, kept in a
	// single table so both stay in sync automatically.
	static const char* TypeToString(Type t);
	static bool TypeFromString(const std::string& s, Type& outType);
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
